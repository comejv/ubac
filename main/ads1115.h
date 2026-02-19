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

#include "esp_err.h"
#include <stdint.h>

#define ADS1115_ADDR               0x48   // Addr connected to GND
#define ADS1115_REG_POINTER_CONV   0x00
#define ADS1115_REG_POINTER_CONFIG 0x01

#define ADS1115_MUX_AIN0 0b100
#define ADS1115_MUX_AIN1 0b101

typedef union
{
  uint16_t raw;
  struct
  {
    uint16_t lsb : 8;   // Lower byte
    uint16_t msb : 8;   // Upper byte
  } bytes;
  struct
  {
    uint16_t comp_que : 2;    // Comparator queue and disable
    uint16_t comp_lat : 1;    // Latching comparator
    uint16_t comp_pol : 1;    // Comparator polarity
    uint16_t comp_mode : 1;   // Comparator mode
    uint16_t dr : 3;          // Data rate
    uint16_t mode : 1;        // Operating mode
    uint16_t pga : 3;         // Programmable gain amplifier
    uint16_t mux : 3;         // Input multiplexer
    uint16_t os : 1;          // Operational status/start conversion
  } fields;
} ADS1115_Config_Register;

// PGA = 4.096 V => 4.096 / 32768 = 125uV per bit
#define ADS_LSB_4V 0.000125f

esp_err_t ads1115_read_raw(int16_t *out_raw, uint16_t mux);
