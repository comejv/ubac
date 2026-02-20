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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>
#include <sys/param.h>

static const char *TAG = "DNS_SERVER";
static TaskHandle_t xDnsTask = NULL;
static int dns_socket = -1;

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
  char rx_buffer[128];
  char tx_buffer[128];
  struct sockaddr_in dest_addr;

  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(53);

  dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

  ESP_LOGI(TAG, "DNS Server listening on port 53");

  struct sockaddr_in source_addr;
  socklen_t socklen = sizeof(source_addr);

  while (1)
  {
    int len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *) &source_addr, &socklen);
    if (len < 0)
    {
      ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
      break;
    }

    if (len > sizeof(dns_header_t))
    {
      // Prepare response (A Record pointing to us)
      // Copy ID
      // Set flags: QR=1 (Response), Opcode=0, AA=1, RD=1, RA=1, RCODE=0
      // 0x8180 is typical for standard query response

      // Simple DNS Hijack: Respond with 192.168.4.1 (Default SoftAP IP)
      // to ANY query.

      // Header
      memcpy(tx_buffer, rx_buffer, len);   // Copy query
      dns_header_t *resp_header = (dns_header_t *) tx_buffer;
      resp_header->flags = htons(0x8180);   // Standard response, No error
      resp_header->an_count = htons(1);     // 1 Answer

      // The query section is variable length (qname), so we need to skip it to append answer
      // QNAME ends with 0x00
      char *query_end = tx_buffer + sizeof(dns_header_t);
      while (*query_end != 0 && (query_end - tx_buffer) < len)
      {
        query_end++;
      }
      if (*query_end == 0)
        query_end++;   // Skip null terminator

      // Skip QTYPE and QCLASS (4 bytes)
      query_end += 4;

      // Now append Answer
      // Name ptr (pointer to offset 12, start of query name)
      *query_end++ = 0xC0;
      *query_end++ = 0x0C;

      // TYPE A (0x0001)
      *query_end++ = 0x00;
      *query_end++ = 0x01;

      // CLASS IN (0x0001)
      *query_end++ = 0x00;
      *query_end++ = 0x01;

      // TTL (0x0000003C = 60s)
      *query_end++ = 0x00;
      *query_end++ = 0x00;
      *query_end++ = 0x00;
      *query_end++ = 0x3C;

      // RDLENGTH (4 bytes for IPv4)
      *query_end++ = 0x00;
      *query_end++ = 0x04;

      // RDATA (192.168.4.1)
      *query_end++ = 192;
      *query_end++ = 168;
      *query_end++ = 4;
      *query_end++ = 1;

      int resp_len = query_end - tx_buffer;

      sendto(dns_socket, tx_buffer, resp_len, 0, (struct sockaddr *) &source_addr, sizeof(source_addr));
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
