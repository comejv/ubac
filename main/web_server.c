/*
 * UBAC:web_server.c for ESP32 to serve web pages.
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

#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "ntc_history.h"
#include "wifi_app.h"
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <sys/param.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

/* HTML Content */
extern const uint8_t config_html_start[] asm("_binary_config_html_start");
extern const uint8_t config_html_end[] asm("_binary_config_html_end");

extern const uint8_t dashboard_html_start[] asm("_binary_dashboard_html_start");
extern const uint8_t dashboard_html_end[] asm("_binary_dashboard_html_end");

/* Handler for the root URL */
static esp_err_t index_get_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");

  // Check if we are connected to STA
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
  {
    httpd_resp_send(req, (const char *) dashboard_html_start, HTTPD_RESP_USE_STRLEN);
  }
  else
  {
    httpd_resp_send(req, (const char *) config_html_start, HTTPD_RESP_USE_STRLEN);
  }
  return ESP_OK;
}

/* Handler for /history.json */
typedef struct
{
  httpd_req_t *req;
  size_t count;
  size_t skip;
  size_t seen;
} stream_ctx_t;

static bool history_stream_cb(const ntc_record_t *rec, void *ctx)
{
  stream_ctx_t *c = (stream_ctx_t *) ctx;

  if (c->seen < c->skip)
  {
    c->seen++;
    return true;
  }

  if (c->count == 0)
  {
    httpd_resp_sendstr_chunk(c->req, "[");
  }
  else
  {
    httpd_resp_sendstr_chunk(c->req, ",");
  }

  char buf[256];
  int len = snprintf(
      buf, sizeof(buf),
      "{\"t\":%" PRIu32 ",\"s\":%d,"
      "\"v\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]}",
      rec->timestamp, NTC_TEMP_SCALE, rec->temps_cC[0],
      rec->temps_cC[1], rec->temps_cC[2], rec->temps_cC[3],
      rec->temps_cC[4], rec->temps_cC[5], rec->temps_cC[6],
      rec->temps_cC[7], rec->temps_cC[8], rec->temps_cC[9]);

  httpd_resp_send_chunk(c->req, buf, len);
  c->count++;

  return true;
}

static esp_err_t history_get_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "application/json");

  stream_ctx_t ctx = {
      .req = req,
      .count = 0,
      .skip = 0,
      .seen = 0,
  };

  size_t max = 1024;

  size_t actual_total = ntc_history_iterate(0, 0, NULL, NULL);
  if (actual_total > max)
  {
    ctx.skip = actual_total - max;
  }

  ntc_history_iterate(0, 0, history_stream_cb, &ctx);

  if (ctx.count == 0)
  {
    httpd_resp_send(req, "[]", 2);
  }
  else
  {
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_send_chunk(req, NULL, 0);
  }

  return ESP_OK;
}

/* Handler for the scan URL */
static esp_err_t scan_get_handler(httpd_req_t *req)
{
  char *json = wifi_app_scan();
  if (json)
  {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
  }
  else
  {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

/* Async Connection Task */
typedef struct
{
  char ssid[32];
  char password[64];
} wifi_creds_t;

static void connect_task(void *pvParameters)
{
  wifi_creds_t *creds = (wifi_creds_t *) pvParameters;
  vTaskDelay(pdMS_TO_TICKS(1000));   // Delay to allow HTTP response to flush
  wifi_app_connect_sta(creds->ssid, creds->password);
  free(creds);
  vTaskDelete(NULL);
}

/* Handler for the connect POST */
static esp_err_t connect_post_handler(httpd_req_t *req)
{
  char buf[100];
  int ret;
  size_t remaining = req->content_len;

  if (remaining >= sizeof(buf))
  {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  ret = httpd_req_recv(req, buf, remaining);
  if (ret <= 0)
  {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  wifi_creds_t *creds = malloc(sizeof(wifi_creds_t));
  if (!creds)
  {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  memset(creds, 0, sizeof(wifi_creds_t));

  char *ssid_ptr = strstr(buf, "ssid=");
  char *pass_ptr = strstr(buf, "password=");

  if (ssid_ptr)
  {
    ssid_ptr += 5;
    char *end = strchr(ssid_ptr, '&');
    if (!end)
      end = buf + strlen(buf);
    int len = MIN(end - ssid_ptr, sizeof(creds->ssid) - 1);
    strncpy(creds->ssid, ssid_ptr, len);
  }

  if (pass_ptr)
  {
    pass_ptr += 9;
    char *end = strchr(pass_ptr, '&');
    if (!end)
      end = buf + strlen(buf);
    int len = MIN(end - pass_ptr, sizeof(creds->password) - 1);
    strncpy(creds->password, pass_ptr, len);
  }

  ESP_LOGI(TAG, "Received Connect Request: SSID='%s'", creds->ssid);

  httpd_resp_send(req, "Connecting... Please reconnect to the new network.", HTTPD_RESP_USE_STRLEN);

  // Spawn task to handle connection
  xTaskCreate(connect_task, "connect_task", 4096, creds, 5, NULL);

  return ESP_OK;
}

static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
    .user_ctx = NULL};

static const httpd_uri_t history_uri = {
    .uri = "/history.json",
    .method = HTTP_GET,
    .handler = history_get_handler,
    .user_ctx = NULL};

static const httpd_uri_t scan_uri = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_get_handler,
    .user_ctx = NULL};

static const httpd_uri_t connect_uri = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_post_handler,
    .user_ctx = NULL};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
  if (err == HTTPD_404_NOT_FOUND)
  {
    // Redirect to captive portal
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }
  return ESP_FAIL;
}

esp_err_t web_server_start(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 12;
  config.stack_size = 8192;   // Increase stack for scan handling

  ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &history_uri);
    httpd_register_uri_handler(server, &scan_uri);
    httpd_register_uri_handler(server, &connect_uri);
    // Register error handler for captive portal effect
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return ESP_FAIL;
}
