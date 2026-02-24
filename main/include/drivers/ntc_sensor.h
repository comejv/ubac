/*
 * UBAC: NTC Sensor Reading.
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
 * @file ntc_sensor.h
 * @brief NTC temperature sensor reading and Steinhart-Hart conversion.
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"
#include <stdint.h>

#define NTC_CHANNELS_COUNT CONFIG_NTC_CHANNELS_COUNT
#define NTC_INVALID_TEMP   -999.0F
#define NTC_DELAY_SEC      CONFIG_NTC_DELAY_SEC

/**
 * @brief Read temperature from an NTC sensor channel.
 * @param channel Channel number (0 to NTC_CHANNELS_COUNT-1).
 * @param out_temp Pointer to store the temperature in Celsius.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t ntc_get_temp_celsius(uint8_t channel, float *out_temp);
