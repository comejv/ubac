/*
 * UBAC: Temperature History Management.
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
 * @file ntc_history.h
 * @brief Persistent storage and retrieval of NTC temperature records.
 */

#pragma once

#include "drivers/ntc_sensor.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NTC_TEMP_SCALE CONFIG_NTC_TEMP_SCALE   // centi-degrees

typedef struct
{
  uint32_t timestamp;   // unix seconds
  int16_t temps_cC[NTC_CHANNELS_COUNT];
  int16_t room_temp_cC;
  int16_t humidity_cRH;
} ntc_record_t;

typedef bool (*ntc_history_iter_cb_t)(const ntc_record_t *rec, void *ctx);

/**
 * @brief Initialize the history storage.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t ntc_history_init(void);

/**
 * @brief Add a new temperature record to history.
 * @param temps Array of temperatures in Celsius.
 * @param room_temp Room temperature in Celsius.
 * @param humidity Relative humidity in %.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t ntc_history_add_record(const float temps[NTC_CHANNELS_COUNT], float room_temp, float humidity);

/**
 * @brief Force flush of RAM buffer to flash.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t ntc_history_flush(void);

/**
 * @brief Get total capacity of the history storage in records.
 * @return Capacity in records.
 */
size_t ntc_history_get_capacity(void);

/**
 * @brief Iterate records in chronological order (oldest -> newest).
 *
 * @param since_ts  Only return records with timestamp >= since_ts (0 disables)
 * @param max       Maximum number of records to emit (0 means "no limit",
 *                  but still internally capped for safety in the caller)
 * @param cb        Callback called for each record; return false to stop
 * @param ctx       User context passed to cb
 *
 * @return number of records for which cb was called
 */
size_t ntc_history_iterate(uint32_t since_ts, size_t max,
                           ntc_history_iter_cb_t cb, void *ctx);

/**
 * @brief Get newest records (chronological order).
 *
 * Reads up to max_records newest records and returns them oldest->newest.
 * @return Number of records read.
 */
size_t ntc_history_get_records(ntc_record_t *out_records, size_t max_records);

/**
 * @brief Erase the whole partition and re-initialize an empty log.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t ntc_history_erase_all(void);
