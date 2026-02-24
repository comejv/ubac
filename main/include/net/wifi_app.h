/*
 * UBAC:wifi_app.h for ESP32 to control a Wi-Fi access point.
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

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

// Wi-Fi Configuration from Kconfig
#define WIFI_AP_SSID    CONFIG_WIFI_AP_SSID
#define WIFI_AP_PASS    CONFIG_WIFI_AP_PASS
#define WIFI_AP_MAX_STA CONFIG_WIFI_AP_MAX_STA

// Initialize NVS and Wi-Fi stack
void wifi_app_init(void);

// Start SoftAP mode
void wifi_app_start_ap(void);

// Scan for networks and return a JSON string (caller must free)
char *wifi_app_scan(void);

// Stop SoftAP and connect to Station
void wifi_app_connect_sta(const char *ssid, const char *password);
