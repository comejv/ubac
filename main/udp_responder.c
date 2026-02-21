/*
 * UBAC:udp_responder.c for ESP32 to answer broadcasted UDP requests.
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

#include "udp_responder.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>

static const char *TAG = "UDP_RESP";
#define UDP_PORT 12345

static void udp_server_task(void *pvParameters)
{
  char rx_buffer[128];
  char addr_str[128];
  struct sockaddr_in dest_addr;

  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(UDP_PORT);

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0)
  {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }

  int err = bind(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
  if (err < 0)
  {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    close(sock);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "UDP Listener running on port %d", UDP_PORT);

  struct sockaddr_in source_addr;
  socklen_t socklen = sizeof(source_addr);

  while (1)
  {
    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *) &source_addr, &socklen);

    if (len < 0)
    {
      ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
      break;
    }

    rx_buffer[len] = 0;   // Null-terminate whatever we received
    inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
    ESP_LOGI(TAG, "Received %d bytes from %s: %s", len, addr_str, rx_buffer);

    // Check for "what is your ip"
    if (strncmp(rx_buffer, "what is your ip", 15) == 0)
    {
      // Get our IP address
      esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      esp_netif_ip_info_t ip_info;
      if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
      {
        char resp_buffer[64];
        snprintf(resp_buffer, sizeof(resp_buffer), "UBAC_IP:%d.%d.%d.%d",
                 IP2STR(&ip_info.ip));

        int err = sendto(sock, resp_buffer, strlen(resp_buffer), 0, (struct sockaddr *) &source_addr, sizeof(source_addr));
        if (err < 0)
        {
          ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        }
      }
    }
  }

  close(sock);
  vTaskDelete(NULL);
}

void udp_responder_start(void)
{
  xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}
