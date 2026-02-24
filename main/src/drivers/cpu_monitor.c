/*
 * UBAC: CPU Monitor.
 * Copyright (C) 2026 Côme VINCENT
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "drivers/cpu_monitor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "CPU_MONITOR";

#if defined(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS) && defined(CONFIG_FREERTOS_USE_TRACE_FACILITY)

esp_err_t cpu_monitor_init(void)
{
  ESP_LOGI(TAG, "Initializing CPU Monitor (FreeRTOS Stats enabled)");
  return ESP_OK;
}

esp_err_t cpu_monitor_get_raw_stats(task_stats_t *tasks, int max_tasks, int *actual_tasks, uint32_t *total_runtime)
{
  TaskStatus_t *pxTaskStatusArray;
  UBaseType_t uxArraySize, x;
  uint32_t ulTotalRunTime;

  uxArraySize = uxTaskGetNumberOfTasks();
  pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

  if (pxTaskStatusArray == NULL)
  {
    return ESP_ERR_NO_MEM;
  }

  uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

  if (total_runtime)
  {
    *total_runtime = ulTotalRunTime;
  }

  int count = 0;
  for (x = 0; x < uxArraySize; x++)
  {
    if (count < max_tasks)
    {
      strncpy(tasks[count].name, pxTaskStatusArray[x].pcTaskName, sizeof(tasks[count].name) - 1);
      tasks[count].name[sizeof(tasks[count].name) - 1] = '\0';
      tasks[count].runtime = pxTaskStatusArray[x].ulRunTimeCounter;

      if (pxTaskStatusArray[x].xCoreID == tskNO_AFFINITY)
      {
        tasks[count].core = -1;
      }
      else
      {
        tasks[count].core = pxTaskStatusArray[x].xCoreID;
      }
      count++;
    }
  }

  if (actual_tasks)
  {
    *actual_tasks = count;
  }

  vPortFree(pxTaskStatusArray);
  return ESP_OK;
}

// Stateful function for logging (single consumer assumed or add mutex if needed)
esp_err_t cpu_monitor_get_usage(cpu_usage_t *usage)
{
  static TaskStatus_t s_last_tasks[MAX_TRACKED_TASKS];
  static int s_last_tasks_count = 0;
  static uint32_t s_last_total_time = 0;

  // Use a static mutex for this function specifically if thread safety is required
  // For now, assuming only one task calls this for logging.

  TaskStatus_t *pxTaskStatusArray;
  UBaseType_t uxArraySize, x;
  uint32_t ulTotalRunTime;

  uxArraySize = uxTaskGetNumberOfTasks();
  pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

  if (pxTaskStatusArray == NULL)
  {
    return ESP_ERR_NO_MEM;
  }

  uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

  float core0_idle_usage = 0.0f;
  float core1_idle_usage = 0.0f;
  bool calculated = false;

  if (s_last_total_time > 0)
  {
    uint32_t total_diff = ulTotalRunTime - s_last_total_time;
    if (total_diff > 0)
    {
      for (x = 0; x < uxArraySize; x++)
      {
        uint32_t last_runtime = 0;
        bool found = false;
        for (int i = 0; i < s_last_tasks_count; i++)
        {
          if (s_last_tasks[i].xHandle == pxTaskStatusArray[x].xHandle)
          {
            last_runtime = s_last_tasks[i].ulRunTimeCounter;
            found = true;
            break;
          }
        }

        if (found)
        {
          uint32_t task_diff = pxTaskStatusArray[x].ulRunTimeCounter - last_runtime;
          if (pxTaskStatusArray[x].ulRunTimeCounter < last_runtime)
          {
            task_diff = (UINT32_MAX - last_runtime) + pxTaskStatusArray[x].ulRunTimeCounter + 1;
          }

          float task_usage = (100.0F * (float) task_diff / (float) total_diff);

          if (strcmp(pxTaskStatusArray[x].pcTaskName, "IDLE") == 0 ||
              strcmp(pxTaskStatusArray[x].pcTaskName, "IDLE0") == 0)
          {
            core0_idle_usage = task_usage;
          }
          else if (strcmp(pxTaskStatusArray[x].pcTaskName, "IDLE1") == 0)
          {
            core1_idle_usage = task_usage;
          }
        }
      }
      calculated = true;
    }
  }

  // Update last state
  s_last_tasks_count = MIN(uxArraySize, MAX_TRACKED_TASKS);
  for (x = 0; x < s_last_tasks_count; x++)
  {
    s_last_tasks[x] = pxTaskStatusArray[x];
  }
  s_last_total_time = ulTotalRunTime;

  vPortFree(pxTaskStatusArray);

  if (calculated)
  {
    usage->core0_usage = 100.0f - core0_idle_usage;
    if (usage->core0_usage < 0.0f)
      usage->core0_usage = 0.0f;
    if (usage->core0_usage > 100.0f)
      usage->core0_usage = 100.0f;

    usage->core1_usage = 100.0f - core1_idle_usage;
    if (usage->core1_usage < 0.0f)
      usage->core1_usage = 0.0f;
    if (usage->core1_usage > 100.0f)
      usage->core1_usage = 100.0f;
    return ESP_OK;
  }

  return ESP_ERR_INVALID_STATE;   // Not enough data yet
}

#else

esp_err_t cpu_monitor_init(void)
{
  ESP_LOGW(TAG, "CPU Monitoring requires CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS and CONFIG_FREERTOS_USE_TRACE_FACILITY enabled in sdkconfig");
  return ESP_OK;
}

esp_err_t cpu_monitor_get_usage(cpu_usage_t *usage)
{
  usage->core0_usage = -1.0F;
  usage->core1_usage = -1.0F;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t cpu_monitor_get_raw_stats(task_stats_t *tasks, int max_tasks, int *actual_tasks, uint32_t *total_runtime)
{
  *actual_tasks = 0;
  if (total_runtime)
    *total_runtime = 0;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif
