/*
 * UBAC: Firmware for ESP32 to monitor NTC sensors and control a fan via PWM.
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

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "fan_ctrl.h"
#include "i2c_manager.h"
#include "mux.h"
#include "ntc_sensor.h"
#include "web_server.h"

static const char *TAG = "UBAC_MAIN";

void ntc_reader_task(void *pvParameters)
{
  while (1)
  {
    ESP_LOGI(TAG, "--- Reading Temperatures ---");
    for (int i = 0; i < NTC_CHANNELS_COUNT; i++)
    {
      float temp_c = ntc_get_temp_celsius(i);
      ESP_LOGI(TAG, "NTC %d: Temp: %.2f C", i, temp_c);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "Starting UBAC Application...");

  // Initialize hardware
  ESP_ERROR_CHECK(i2c_manager_init());
  mux_init();
  ESP_ERROR_CHECK(fan_ctrl_init());

  // Start services
  ESP_ERROR_CHECK(web_server_start());

  // Create tasks
  xTaskCreate(ntc_reader_task, "ntc_task", 4096, NULL, 5, NULL);
}
