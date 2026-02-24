/*
 * UBAC: Board-specific pin definitions and hardware configuration for UBAC v1.
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
 * @file ubac_board_v1.h
 * @brief Board-specific pin definitions and hardware configuration for UBAC v1.
 */

#pragma once

#include "driver/gpio.h"

#define I2C_SDA_PIN GPIO_NUM_21
#define I2C_SCL_PIN GPIO_NUM_22

#define MUX_S0_PIN GPIO_NUM_26
#define MUX_S1_PIN GPIO_NUM_27
#define MUX_S2_PIN GPIO_NUM_14
#define MUX_S3_PIN GPIO_NUM_12

#define FAN_PWM_PIN GPIO_NUM_5

#define DS18B20_PIN GPIO_NUM_18
#define DHT_PIN     GPIO_NUM_19

#define LED_RED_PIN   GPIO_NUM_33
#define LED_GREEN_PIN GPIO_NUM_32
#define LED_BLUE_PIN  GPIO_NUM_25
