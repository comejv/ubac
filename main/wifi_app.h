#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

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
