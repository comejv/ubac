/*
 * UBAC: CPU Monitor.
 * Copyright (C) 2026 Côme VINCENT
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

#define MAX_TRACKED_TASKS 64

/**
 * @brief Structure to hold CPU usage for each core (calculated).
 */
typedef struct
{
  float core0_usage;
  float core1_usage;
} cpu_usage_t;

/**
 * @brief Structure to hold raw CPU runtime stats for a task.
 */
typedef struct
{
  char name[16];
  uint32_t runtime;   // Raw runtime counter
  int core;
} task_stats_t;

/**
 * @brief Initialize the CPU monitor.
 * @return ESP_OK on success.
 */
esp_err_t cpu_monitor_init(void);

/**
 * @brief Get the current calculated CPU core usage (stateful, for logging).
 * @param usage Pointer to a cpu_usage_t structure to fill.
 * @return ESP_OK on success.
 */
esp_err_t cpu_monitor_get_usage(cpu_usage_t *usage);

/**
 * @brief Get raw CPU stats (snapshot).
 * @param tasks Pointer to an array of task_stats_t to fill.
 * @param max_tasks Maximum number of tasks to fill.
 * @param actual_tasks Pointer to store the number of tasks filled.
 * @param total_runtime Pointer to store the total system runtime.
 * @return ESP_OK on success.
 */
esp_err_t cpu_monitor_get_raw_stats(task_stats_t *tasks, int max_tasks, int *actual_tasks, uint32_t *total_runtime);
