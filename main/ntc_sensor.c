/*
 * UBAC:ntc_sensor.c for ESP32 to read temperatures from NTC sensors.
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

#include "ntc_sensor.h"
#include "ads1115.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mux.h"
#include <math.h>

// --- NTC Constants ---
#define R_DIVIDER 56000.0f
#define VREF_RAIL 3.3f   // Voltage supplying the NTC divider

// --- Steinhart-Hart Coefficients (R in ohms, T in kelvin) ---
#define SH_A 8.954641936e-4f
#define SH_B 2.034215141e-4f
#define SH_C 7.639241707e-8f

static float convert_to_celsius(int16_t raw_adc)
{
  // Single-ended should be >= 0. Allow 0 (near GND) as valid.
  if (raw_adc < 0)
  {
    return -999.0F;
  }

  float voltage = (float) raw_adc * ADS_LSB_4V;

  // Guard: divider math explodes near VREF_RAIL
  if (voltage <= 0.001F || voltage >= (VREF_RAIL - 0.01F))
  {
    return -999.0F;
  }

  float r_ntc = (voltage * R_DIVIDER) / (VREF_RAIL - voltage);
  if (r_ntc <= 0.0F)
  {
    return -999.0F;
  }

  float lnR = logf(r_ntc);
  float invT = SH_A + (SH_B * lnR) + (SH_C * lnR * lnR * lnR);
  if (invT <= 0.0F)
  {
    return -999.0F;
  }

  float temp_k = 1.0F / invT;
  return temp_k - 273.15F;
}

float ntc_get_temp_celsius(uint8_t channel)
{
  mux_set_channel(channel);

  // Short delay for voltage stabilization after mux switch
  vTaskDelay(pdMS_TO_TICKS(10));

  // Trigger and read single-shot
  int16_t raw_val = 0;
  if (ads1115_read_raw(&raw_val, ADS1115_MUX_AIN0) != ESP_OK)
  {
    return -999.0F;
  }

  return convert_to_celsius(raw_val);
}
