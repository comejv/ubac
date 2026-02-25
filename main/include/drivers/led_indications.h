/*
 * UBAC: LED Indications Manager.
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
 * @file led_indications.h
 * @brief LED Indications Manager for status reporting.
 */

#pragma once

#include "esp_err.h"

/**
 * @brief LED Colors (Combinations of R, G, B)
 */
typedef enum
{
  LED_COLOR_OFF = 0,
  LED_COLOR_RED = (1 << 0),
  LED_COLOR_GREEN = (1 << 1),
  LED_COLOR_BLUE = (1 << 2),
  LED_COLOR_YELLOW = (LED_COLOR_RED | LED_COLOR_GREEN),
  LED_COLOR_CYAN = (LED_COLOR_GREEN | LED_COLOR_BLUE),
  LED_COLOR_MAGENTA = (LED_COLOR_RED | LED_COLOR_BLUE),
  LED_COLOR_WHITE = (LED_COLOR_RED | LED_COLOR_GREEN | LED_COLOR_BLUE)
} led_color_t;

/**
 * @brief LED Indication Modes
 */
typedef enum
{
  LED_MODE_OFF,
  LED_MODE_SOLID,
  LED_MODE_BLINK_SLOW,   // 1Hz
  LED_MODE_BLINK_FAST,   // 5Hz
  LED_MODE_PULSE         // Experimental/Future
} led_mode_t;

/**
 * @brief Initialize LED GPIOs and start the indication task.
 * @return ESP_OK on success.
 */
esp_err_t led_indications_init(void);

/**
 * @brief Set the current LED indication.
 * @param color The color combination to display.
 * @param mode The display mode (solid or blinking).
 */
void led_indications_set(led_color_t color, led_mode_t mode);
