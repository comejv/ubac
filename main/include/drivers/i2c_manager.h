/*
 * UBAC: I2C Bus Manager.
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
 * @file i2c_manager.h
 * @brief I2C bus initialization and management.
 */

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "sdkconfig.h"

#define I2C_MASTER_NUM        0
#define I2C_MASTER_FREQ_HZ    CONFIG_I2C_MASTER_FREQ_HZ
#define I2C_MASTER_TIMEOUT_MS CONFIG_I2C_MASTER_TIMEOUT_MS

extern i2c_master_bus_handle_t i2c_manager_bus_handle;

esp_err_t i2c_manager_init(void);
