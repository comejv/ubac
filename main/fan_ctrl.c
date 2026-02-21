/*
 * UBAC:fan_ctrl.c for ESP32 to control a fan via PWM.
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

#include "fan_ctrl.h"
#include "esp_log.h"

static const char *TAG = "FAN_CTRL";

esp_err_t fan_ctrl_init(void)
{
  ESP_LOGI(TAG, "Initializing fan PWM control (skeleton)...");
  // TODO: Implement PWM initialization
  return ESP_OK;
}

esp_err_t fan_ctrl_set_speed(float duty_cycle)
{
  ESP_LOGI(TAG, "Setting fan speed to %.2f (skeleton)...", duty_cycle);
  // TODO: Implement duty cycle update
  // Because PWM is open drain, we need to invert the duty cycle
  return ESP_OK;
}
