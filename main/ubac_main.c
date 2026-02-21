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

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "ads1115.h"
#include "dns_server.h"
#include "fan_ctrl.h"
#include "i2c_manager.h"
#include "mux.h"
#include "ntc_history.h"
#include "ntc_sensor.h"
#include "udp_responder.h"
#include "web_server.h"
#include "wifi_app.h"

static const char *TAG = "UBAC_MAIN";

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));

    // Start UDP responder service once we have an IP
    udp_responder_start();

    // Stop DNS server as we are no longer an AP (or shouldn't be hijacking)
    dns_server_stop();
  }
}

void ntc_reader_task(void *pvParameters)
{
  while (1)
  {
    ESP_LOGI(TAG, "--- Reading Temperatures ---");
    float temps[NTC_CHANNELS_COUNT];
    for (int i = 0; i < NTC_CHANNELS_COUNT; i++)
    {
      temps[i] = ntc_get_temp_celsius(i);
      ESP_LOGI(TAG, "NTC %d: Temp: %.2f C", i, temps[i]);
    }
    int is_valid_temps = 1;
    for (int i = 0; i < NTC_CHANNELS_COUNT; i++)
    {
      if (temps[i] == NTC_INVALID_TEMP)
      {
        is_valid_temps = 0;
        ESP_LOGW(TAG, "NTC %d: Invalid Temp: %.2f C (Skipping)", i, temps[i]);
        break;
      }
    }
    if (is_valid_temps)
    {
      ntc_history_add_record(temps);
    }

    vTaskDelay(pdMS_TO_TICKS(NTC_DELAY_SEC * 1000));
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "Starting UBAC Application...");

  // Initialize NVS and Wi-Fi stack
  wifi_app_init();

  // Initialize History
  ntc_history_init();

  // Register IP event handler for main app logic (starting UDP responder)
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &ip_event_handler,
                                                      NULL,
                                                      NULL));

  // Initialize hardware
  ESP_ERROR_CHECK(i2c_manager_init());
  ESP_ERROR_CHECK(ads1115_init());
  mux_init();
  ESP_ERROR_CHECK(fan_ctrl_init());

  // Start SoftAP
  wifi_app_start_ap();

  // Start DNS Server (Captive Portal)
  dns_server_start();

  // Start Web Server
  ESP_ERROR_CHECK(web_server_start());

  // Create tasks
  xTaskCreate(ntc_reader_task, "ntc_task", 4096, NULL, 5, NULL);
}
