/*
 * UBAC: Wi-Fi Application Manager.
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

#include "net/wifi_app.h"
#include "drivers/led_indications.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WIFI_APP";
static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static int s_retry_num = 0;
#define MAX_RETRY 5

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
    s_retry_num = 0;
    led_indications_set(LED_COLOR_BLUE, LED_MODE_BLINK_FAST);
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    led_indications_set(LED_COLOR_RED, LED_MODE_BLINK_SLOW);
    wifi_config_t config;
    esp_wifi_get_config(WIFI_IF_STA, &config);
    if (strlen((char *) config.sta.ssid) > 0)
    {
      if (s_retry_num < MAX_RETRY)
      {
        s_retry_num++;
        ESP_LOGI(TAG, "Disconnected from AP, retrying (%d/%d)...", s_retry_num, MAX_RETRY);
        esp_wifi_connect();
      }
      else
      {
        ESP_LOGW(TAG, "Max retries reached. Stopping reconnection attempts.");
      }
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    s_retry_num = 0;
    led_indications_set(LED_COLOR_BLUE, LED_MODE_SOLID);
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

esp_err_t wifi_app_init(void)
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
      return err;
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK)
    return ret;

  s_wifi_event_group = xEventGroupCreate();
  if (!s_wifi_event_group)
    return ESP_ERR_NO_MEM;

  esp_err_t err = esp_netif_init();
  if (err != ESP_OK)
    return err;

  esp_netif_create_default_wifi_ap();
  esp_netif_create_default_wifi_sta();

  // Initialize WiFi stack
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK)
    return err;

  // Explicitly set storage to FLASH for long-term credential persistence
  err = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  if (err != ESP_OK)
    return err;

  // Enable WiFi Power Save Mode to reduce power consumption
  err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  if (err != ESP_OK)
    return err;

  err = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
  if (err != ESP_OK)
    return err;

  err = esp_event_handler_instance_register(IP_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
  if (err != ESP_OK)
    return err;

  // Check if we have credentials
  wifi_config_t config;
  esp_wifi_get_config(WIFI_IF_STA, &config);

  if (strlen((char *) config.sta.ssid) > 0)
  {
    ESP_LOGI(TAG, "Found saved SSID '%s'. Attempting to connect...", config.sta.ssid);
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
      return err;
    err = esp_wifi_start();
    if (err != ESP_OK)
      return err;

    // Wait 3 seconds for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(3000));
    if (bits & WIFI_CONNECTED_BIT)
    {
      ESP_LOGI(TAG, "Successfully connected to saved WiFi.");
      return ESP_OK;   // Skip AP start
    }
    else
    {
      ESP_LOGW(TAG, "Failed to connect to saved WiFi within 10s.");
    }
  }

  // If no SSID or connection failed, start AP
  return wifi_app_start_ap();
}

esp_err_t wifi_app_start_ap(void)
{
  wifi_config_t wifi_config = {
      .ap = {
          .ssid = CONFIG_WIFI_AP_SSID,
          .ssid_len = strlen(CONFIG_WIFI_AP_SSID),
          .channel = 1,
          .password = CONFIG_WIFI_AP_PASS,
          .max_connection = CONFIG_WIFI_AP_MAX_STA,
          .authmode = WIFI_AUTH_WPA_WPA2_PSK},
  };

  if (strlen(CONFIG_WIFI_AP_PASS) < 8)
  {
    if (strlen(CONFIG_WIFI_AP_PASS) > 0)
    {
      ESP_LOGW(TAG, "Password too short for WPA2 (min 8 chars). Switching to OPEN.");
    }
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    memset(wifi_config.ap.password, 0, sizeof(wifi_config.ap.password));
  }

  esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (err != ESP_OK)
    return err;
  err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  if (err != ESP_OK)
    return err;
  err = esp_wifi_start();
  if (err != ESP_OK)
    return err;

  led_indications_set(LED_COLOR_YELLOW, LED_MODE_BLINK_SLOW);

  ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s channel:%d",
           CONFIG_WIFI_AP_SSID, 1);
  return ESP_OK;
}

char *wifi_app_scan(void)
{
  wifi_scan_config_t scan_config = {
      .ssid = 0,
      .bssid = 0,
      .channel = 0,
      .show_hidden = true};

  esp_err_t err = esp_wifi_scan_start(&scan_config, true);
  if (err != ESP_OK)
  {
    if (err == ESP_ERR_WIFI_STATE)
    {
      ESP_LOGW(TAG, "Scan failed: Wi-Fi is in a state where scanning is not allowed (e.g., connecting).");
    }
    else
    {
      ESP_LOGE(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
    }
    return NULL;
  }

  uint16_t ap_count = 0;
  err = esp_wifi_scan_get_ap_num(&ap_count);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_wifi_scan_get_ap_num failed: %s", esp_err_to_name(err));
    return NULL;
  }

  wifi_ap_record_t *ap_list = (wifi_ap_record_t *) malloc(sizeof(wifi_ap_record_t) * ap_count);
  if (!ap_list)
    return NULL;

  if (esp_wifi_scan_get_ap_records(&ap_count, ap_list) != ESP_OK)
  {
    free(ap_list);
    return NULL;
  }

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
      free(json);
      return NULL;
    }
    strcat(json, entry);
  }
  strcat(json, "]");

  free(ap_list);
  return json;
}

esp_err_t wifi_app_connect_sta(const char *ssid, const char *password)
{
  wifi_config_t wifi_config = {0};
  strncpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char *) wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

  esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK)
    return err;
  err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (err != ESP_OK)
    return err;

  err = esp_wifi_start();
  if (err != ESP_OK && err != ESP_ERR_WIFI_STATE)
  {
    ESP_LOGW(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
    return err;
  }

  err = esp_wifi_connect();
  if (err != ESP_OK)
    return err;

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Register IP event handler for this phase
  return esp_event_handler_instance_register(IP_EVENT,
                                             IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler,
                                             NULL,
                                             NULL);
}
