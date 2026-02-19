/*
 * UBAC: Firmware for ESP32 to monitor NTC sensors and control a fan via PWM.
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

#include "driver/gpio.h"
#include <stdint.h>

#define MUX_S0_PIN GPIO_NUM_26
#define MUX_S1_PIN GPIO_NUM_27
#define MUX_S2_PIN GPIO_NUM_14
#define MUX_S3_PIN GPIO_NUM_12

void mux_init(void);
void mux_set_channel(uint8_t channel);
