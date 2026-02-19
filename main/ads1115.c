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

#include "ads1115.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_manager.h"

esp_err_t ads1115_read_raw(int16_t *out_raw, uint16_t mux)
{
  ADS1115_Config_Register config;
  config.raw = 0;
  config.fields.os = 1;            // Start conversion
  config.fields.mux = mux;         // Select channel
  config.fields.pga = 0b001;       // +/-4.096V
  config.fields.mode = 1;          // Single-shot
  config.fields.dr = 0b100;        // 128 SPS
  config.fields.comp_que = 0b11;   // Disable comparator

  // Write config (start single-shot)
  uint8_t config_data[3] = {
      ADS1115_REG_POINTER_CONFIG,
      config.bytes.msb,
      config.bytes.lsb,
  };

  esp_err_t err = i2c_master_write_to_device(
      I2C_MASTER_NUM,
      ADS1115_ADDR,
      config_data,
      sizeof(config_data),
      pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  if (err != ESP_OK)
  {
    return err;
  }

  // Wait for conversion: 128 SPS => 7.8125ms
  vTaskDelay(pdMS_TO_TICKS(10));

  // Point to conversion register
  uint8_t reg_ptr = ADS1115_REG_POINTER_CONV;
  err = i2c_master_write_to_device(
      I2C_MASTER_NUM,
      ADS1115_ADDR,
      &reg_ptr,
      1,
      pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  if (err != ESP_OK)
  {
    return err;
  }

  // Read conversion (2 bytes, big-endian)
  uint8_t buf[2] = {0};
  err = i2c_master_read_from_device(
      I2C_MASTER_NUM,
      ADS1115_ADDR,
      buf,
      sizeof(buf),
      pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  if (err != ESP_OK)
  {
    return err;
  }

  if (out_raw != NULL)
  {
    *out_raw = (int16_t) ((buf[0] << 8) | buf[1]);
  }
  return ESP_OK;
}
