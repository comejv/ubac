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

#include "app/ntc_history.h"
#include "drivers/ads1115.h"
#include "drivers/cpu_monitor.h"
#include "drivers/fan_ctrl.h"
#include "drivers/i2c_manager.h"
#include "drivers/led_indications.h"
#include "drivers/mux.h"
#include "drivers/ntc_sensor.h"
#include "drivers/ubac_board_v1.h"
#include "net/dns_server.h"
#include "net/sntp_app.h"
#include "net/udp_responder.h"
#include "net/web_server.h"
#include "net/wifi_app.h"
#include "sdkconfig.h"

#include "dht.h"
#include "ds18b20.h"
#include "onewire_bus.h"

static const char *TAG = "UBAC_MAIN";

static ds18b20_device_handle_t s_ds18b20 = NULL;
static onewire_bus_handle_t s_ow_bus = NULL;

static esp_err_t ds18b20_init(void)
{
  onewire_bus_config_t bus_config = {.bus_gpio_num = DS18B20_PIN};
  onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 10};
  esp_err_t err = onewire_new_bus_rmt(&bus_config, &rmt_config, &s_ow_bus);
  if (err != ESP_OK)
    return err;

  onewire_device_iter_handle_t iter = NULL;
  onewire_device_t next_onewire_device;
  err = onewire_new_device_iter(s_ow_bus, &iter);
  if (err != ESP_OK)
    return err;

  if (onewire_device_iter_get_next(iter, &next_onewire_device) == ESP_OK)
  {
    ds18b20_config_t ds_cfg = {};
    err = ds18b20_new_device_from_enumeration(&next_onewire_device, &ds_cfg, &s_ds18b20);
  }
  else
  {
    ESP_LOGW(TAG, "No DS18B20 sensor found");
  }
  onewire_del_device_iter(iter);
  return err;
}

#ifdef CONFIG_ENABLE_CPU_MONITOR
void cpu_monitor_task(void *pvParameters)
{
  static int i = 0;
  while (1)
  {
    cpu_usage_t usage;
    if (cpu_monitor_get_usage(&usage) == ESP_OK)
    {
      if (i++ % 5 == 0)
      {
        ESP_LOGI(TAG, "CPU Core 0: %.1f%%, Core 1: %.1f%%", usage.core0_usage, usage.core1_usage);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(CONFIG_CPU_MONITOR_POLL_MS));
  }
}
#endif

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));

    // Start UDP responder service once we have an IP
    ESP_ERROR_CHECK(udp_responder_start());

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
    int is_valid_temps = 1;

    for (int i = 0; i < NTC_CHANNELS_COUNT; i++)
    {
      esp_err_t err = ntc_get_temp_celsius(i, &temps[i]);
      if (err != ESP_OK)
      {
        is_valid_temps = 0;
        ESP_LOGW(TAG, "NTC %d: Failed to read temp (err: %s)", i, esp_err_to_name(err));
        break;
      }
      ESP_LOGI(TAG, "NTC %d: Temp: %.2f C", i, temps[i]);
    }

    if (is_valid_temps)
    {
      float room_temp = NTC_INVALID_TEMP;
      float humidity = NTC_INVALID_TEMP;

      if (s_ds18b20)
      {
        if (ds18b20_trigger_temperature_conversion(s_ds18b20) == ESP_OK)
        {
          vTaskDelay(pdMS_TO_TICKS(800));
          if (ds18b20_get_temperature(s_ds18b20, &room_temp) != ESP_OK)
          {
            room_temp = NTC_INVALID_TEMP;
          }
        }
      }

      int16_t humidity_raw, temp_raw;
      if (dht_read_data(DHT_TYPE_DHT11, DHT_PIN, &humidity_raw, &temp_raw) == ESP_OK)
      {
        humidity = (float) humidity_raw / 10.0f;
      }

      esp_err_t err = ntc_history_add_record(temps, room_temp, humidity);
      if (err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to add record to history: %s", esp_err_to_name(err));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(NTC_DELAY_SEC * 1000));
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "Starting UBAC Application...");

  // Initialize the default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Register IP event handler for main app logic (starting UDP responder)
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &ip_event_handler,
                                                      NULL,
                                                      NULL));

  // Initialize NVS and Wi-Fi stack
  ESP_ERROR_CHECK(wifi_app_init());

  // Initialize SNTP
  sntp_app_init();

  // Initialize History
  ESP_ERROR_CHECK(ntc_history_init());

  // Initialize LED indications
  ESP_ERROR_CHECK(led_indications_init());
  led_indications_set(LED_COLOR_YELLOW, LED_MODE_BLINK_SLOW);

  // Initialize hardware
  ESP_ERROR_CHECK(i2c_manager_init());
  ESP_ERROR_CHECK(ads1115_init());
  ESP_ERROR_CHECK(mux_init());
  ESP_ERROR_CHECK(fan_ctrl_init());
  ESP_ERROR_CHECK(ds18b20_init());
  gpio_set_pull_mode(DHT_PIN, GPIO_FLOATING);

  // Start DNS Server (Captive Portal)
  ESP_ERROR_CHECK(dns_server_start());

  // Start Web Server
  ESP_ERROR_CHECK(web_server_start());

#ifdef CONFIG_ENABLE_CPU_MONITOR
  ESP_ERROR_CHECK(cpu_monitor_init());
  xTaskCreate(cpu_monitor_task, "cpu_mon", 4096, NULL, 1, NULL);
#endif

  // Create tasks
  xTaskCreate(ntc_reader_task, "ntc_task", 4096, NULL, 5, NULL);
}
