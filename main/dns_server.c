/*
 * UBAC:dns_server.c for ESP32 to hijack DNS queries for a captive portal.
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

#include "dns_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>
#include <sys/param.h>

#define DNS_PORT        53
#define DNS_MAX_PAYLOAD 512

static const char *TAG = "DNS_SERVER";
static TaskHandle_t xDnsTask = NULL;
static int dns_socket = -1;

// Standard DNS Header
typedef struct __attribute__((packed))
{
  uint16_t id;
  uint16_t flags;
  uint16_t qd_count;
  uint16_t an_count;
  uint16_t ns_count;
  uint16_t ar_count;
} dns_header_t;

static void dns_server_task(void *pvParameters)
{
  uint8_t rx_buffer[DNS_MAX_PAYLOAD];
  uint8_t tx_buffer[DNS_MAX_PAYLOAD];
  struct sockaddr_in dest_addr;

  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(DNS_PORT);

  int dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (dns_socket < 0)
  {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }

  int err = bind(dns_socket, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
  if (err < 0)
  {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    close(dns_socket);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "DNS Server listening on port %d", DNS_PORT);

  struct sockaddr_in source_addr;
  socklen_t socklen = sizeof(source_addr);

  // Fetch current AP IP Address dynamically
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(netif, &ip_info);
  uint8_t *ip = (uint8_t *) &ip_info.ip.addr;

  while (1)
  {
    ssize_t len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer), 0,
                           (struct sockaddr *) &source_addr, &socklen);
    if (len < 0)
    {
      ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
      break;
    }

    if (len >= sizeof(dns_header_t))
    {
      memcpy(tx_buffer, rx_buffer, len);
      dns_header_t *resp_header = (dns_header_t *) tx_buffer;

      // Extract QNAME to find QTYPE
      uint8_t *query_end = tx_buffer + sizeof(dns_header_t);
      while (*query_end != 0 && (query_end - tx_buffer) < len)
      {
        query_end++;
      }

      // Ensure we haven't exceeded the buffer and we have space for QTYPE/QCLASS
      if (*query_end == 0 && (query_end - tx_buffer) + 5 <= len)
      {
        query_end++;   // Skip null terminator

        uint16_t qtype = (query_end[0] << 8) | query_end[1];
        query_end += 4;   // Skip QTYPE (2) and QCLASS (2)

        // Only answer A records (Type 1) or ANY (Type 255)
        if (qtype == 0x01 || qtype == 0xFF)
        {
          // Check if appending 16 bytes will cause a buffer overflow
          if ((query_end - tx_buffer) + 16 <= DNS_MAX_PAYLOAD)
          {
            resp_header->flags = htons(0x8180);   // Standard response, NOERROR
            resp_header->an_count = htons(1);     // 1 Answer

            // Name ptr (pointer to offset 12, start of query name)
            *query_end++ = 0xC0;
            *query_end++ = 0x0C;

            // TYPE A (0x0001)
            *query_end++ = 0x00;
            *query_end++ = 0x01;

            // CLASS IN (0x0001)
            *query_end++ = 0x00;
            *query_end++ = 0x01;

            // TTL (60s)
            *query_end++ = 0x00;
            *query_end++ = 0x00;
            *query_end++ = 0x00;
            *query_end++ = 0x3C;

            // RDLENGTH (4 bytes for IPv4)
            *query_end++ = 0x00;
            *query_end++ = 0x04;

            // RDATA (Dynamic IP)
            *query_end++ = ip[0];
            *query_end++ = ip[1];
            *query_end++ = ip[2];
            *query_end++ = ip[3];

            int resp_len = query_end - tx_buffer;
            sendto(dns_socket, tx_buffer, resp_len, 0,
                   (struct sockaddr *) &source_addr, sizeof(source_addr));
          }
        }
        else
        {
          // For non-A records (like AAAA), respond with NOERROR, 0 Answers
          // This tells the OS the record does not exist and prevents timeouts
          resp_header->flags = htons(0x8180);
          resp_header->an_count = htons(0);
          sendto(dns_socket, tx_buffer, len, 0, (struct sockaddr *) &source_addr,
                 sizeof(source_addr));
        }
      }
    }
  }

  close(dns_socket);
  vTaskDelete(NULL);
}

void dns_server_start(void)
{
  xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &xDnsTask);
}

void dns_server_stop(void)
{
  if (xDnsTask)
  {
    vTaskDelete(xDnsTask);
    xDnsTask = NULL;
  }
  if (dns_socket >= 0)
  {
    close(dns_socket);
    dns_socket = -1;
  }
}
