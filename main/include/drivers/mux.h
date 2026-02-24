/*
 * UBAC: Analog Multiplexer Control.
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
 * @file mux.h
 * @brief Control logic for the analog multiplexer (e.g., CD74HC4067).
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Initialize GPIOs for the multiplexer.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t mux_init(void);

/**
 * @brief Select a channel on the multiplexer.
 * @param channel Channel number (0-15).
 */
void mux_set_channel(uint8_t channel);
