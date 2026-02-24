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

/**
 * @file wifi_app.h
 * @brief Management of Wi-Fi modes (AP and STA) and connectivity.
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

/**
 * @brief Initialize NVS and Wi-Fi stack
 * @return ESP_OK on success, or an error code.
 */
esp_err_t wifi_app_init(void);

/**
 * @brief Start SoftAP mode
 * @return ESP_OK on success, or an error code.
 */
esp_err_t wifi_app_start_ap(void);

/**
 * @brief Scan for networks and return a JSON string
 * @return JSON string (caller must free), or NULL on error.
 */
char *wifi_app_scan(void);

/**
 * @brief Stop SoftAP and connect to Station
 * @param ssid SSID of the target network.
 * @param password Password of the target network.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t wifi_app_connect_sta(const char *ssid, const char *password);
