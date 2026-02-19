#pragma once

#include "esp_err.h"

// Wi-Fi Configuration
#define WIFI_AP_SSID    "UBAC_Config"
#define WIFI_AP_PASS    "password"
#define WIFI_AP_MAX_STA 4

// Initialize NVS and Wi-Fi stack
void wifi_app_init(void);

// Start SoftAP mode
void wifi_app_start_ap(void);

// Scan for networks and return a JSON string (caller must free)
char *wifi_app_scan(void);

// Stop SoftAP and connect to Station
void wifi_app_connect_sta(const char *ssid, const char *password);
