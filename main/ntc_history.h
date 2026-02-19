/*
 * UBAC: Firmware for ESP32 to monitor NTC sensors and control a flash-based
 * ring buffer for history.
 * Copyright (C) 2026 Côme VINCENT
 */

#pragma once

#include "esp_err.h"
#include "ntc_sensor.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NTC_TEMP_SCALE 100   // centi-degrees

typedef struct
{
  uint32_t timestamp;   // unix seconds
  int16_t temps_cC[NTC_CHANNELS_COUNT];
} ntc_record_t;

typedef bool (*ntc_history_iter_cb_t)(const ntc_record_t *rec, void *ctx);

void ntc_history_init(void);

void ntc_history_add_record(const float temps[NTC_CHANNELS_COUNT]);

void ntc_history_flush(void);

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
 */
size_t ntc_history_get_records(ntc_record_t *out_records, size_t max_records);

/**
 * @brief Erase the whole partition and re-initialize an empty log.
 */
esp_err_t ntc_history_erase_all(void);
