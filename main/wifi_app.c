/*
 * UBAC:wifi_app.c for ESP32 to control a Wi-Fi access point.
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

#include "wifi_app.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#include "freertos/event_groups.h"

static const char *TAG = "WIFI_APP";
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
    ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
             MAC2STR(event->mac), event->aid);
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
  {
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
    ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
             MAC2STR(event->mac), event->aid);
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    wifi_config_t config;
    esp_wifi_get_config(WIFI_IF_STA, &config);
    if (strlen((char *) config.sta.ssid) > 0)
    {
      ESP_LOGI(TAG, "Disconnected from AP, retrying...");
      esp_wifi_connect();
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
  }
}

void wifi_app_init(void)
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
  {
    ESP_ERROR_CHECK(ret);
  }

  esp_netif_create_default_wifi_ap();
  esp_netif_create_default_wifi_sta();

  // Initialize WiFi stack
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Explicitly set storage to FLASH for long-term credential persistence
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));

  // Check if we have credentials
  wifi_config_t config;
  esp_wifi_get_config(WIFI_IF_STA, &config);

  if (strlen((char *) config.sta.ssid) > 0)
  {
    ESP_LOGI(TAG, "Found saved SSID '%s'. Attempting to connect...", config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait 5 seconds for connection
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(5000));
    if (bits & WIFI_CONNECTED_BIT)
    {
      ESP_LOGI(TAG, "Successfully connected to saved WiFi.");
      return;   // Skip AP start
    }
    else
    {
      ESP_LOGW(TAG, "Failed to connect to saved WiFi within 10s.");
    }
  }

  // If no SSID or connection failed, start AP
  wifi_app_start_ap();
}

void wifi_app_start_ap(void)
{
  wifi_config_t wifi_config = {
      .ap = {
          .ssid = WIFI_AP_SSID,
          .ssid_len = strlen(WIFI_AP_SSID),
          .channel = 1,
          .password = WIFI_AP_PASS,
          .max_connection = WIFI_AP_MAX_STA,
          .authmode = WIFI_AUTH_WPA_WPA2_PSK},
  };

  if (strlen(WIFI_AP_PASS) < 8)
  {
    if (strlen(WIFI_AP_PASS) > 0)
    {
      ESP_LOGW(TAG, "Password too short for WPA2 (min 8 chars). Switching to OPEN.");
    }
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    memset(wifi_config.ap.password, 0, sizeof(wifi_config.ap.password));
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s channel:%d",
           WIFI_AP_SSID, 1);
}

char *wifi_app_scan(void)
{
  wifi_scan_config_t scan_config = {
      .ssid = 0,
      .bssid = 0,
      .channel = 0,
      .show_hidden = true};

  ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));   // Block until done

  uint16_t ap_count = 0;
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

  wifi_ap_record_t *ap_list = (wifi_ap_record_t *) malloc(sizeof(wifi_ap_record_t) * ap_count);
  if (!ap_list)
    return NULL;

  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

  // Estimated size: 50 chars per AP * 20 APs = 1000 bytes
  char *json = (char *) malloc((ap_count * 100) + 32);
  if (!json)
  {
    free(ap_list);
    return NULL;
  }

  strcpy(json, "[");
  for (int i = 0; i < ap_count; i++)
  {
    char entry[100];
    // Simple JSON escaping might be needed for SSID but assuming simple chars for now
    int len = snprintf(entry, sizeof(entry), "{\"ssid\":\"%s\",\"rssi\":%d}%s",
                       ap_list[i].ssid, ap_list[i].rssi, (i < ap_count - 1) ? "," : "");
    if (len >= sizeof(entry) || len < 0)
    {
      free(ap_list);
      return NULL;
    }
    strcat(json, entry);
  }
  strcat(json, "]");

  free(ap_list);
  return json;
}

void wifi_app_connect_sta(const char *ssid, const char *password)
{
  // Stop AP and switch to STA
  // Note: We need to be careful with netif handling in a real robust app,
  // but for this simple flow, we will just re-init or switch modes.

  // STA netif was created in wifi_app_start_ap (as we used APSTA)

  wifi_config_t wifi_config = {0};
  strncpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char *) wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "esp_wifi_start failed (might be already started): %s", esp_err_to_name(err));
    // Try connecting anyway
  }

  esp_wifi_connect();

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Register IP event handler for this phase
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));
}
