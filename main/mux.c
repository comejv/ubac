/*
 * UBAC:mux.c for ESP32 to control a multiplexer.
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

#include "mux.h"

void mux_init(void)
{
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << MUX_S0_PIN) | (1ULL << MUX_S1_PIN) |
                      (1ULL << MUX_S2_PIN) | (1ULL << MUX_S3_PIN),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = 0,
      .pull_down_en = 0,
      .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);
}

void mux_set_channel(uint8_t channel)
{
  gpio_set_level(MUX_S0_PIN, (channel & 0x01));
  gpio_set_level(MUX_S1_PIN, (channel >> 1) & 0x01);
  gpio_set_level(MUX_S2_PIN, (channel >> 2) & 0x01);
  gpio_set_level(MUX_S3_PIN, (channel >> 3) & 0x01);
}
