/*
 * UBAC: DNS Hijacking Server.
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
 * @file dns_server.h
 * @brief DNS server to redirect all queries to the local IP (Captive Portal).
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Start the DNS hijacking server.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t dns_server_start(void);

/**
 * @brief Stop the DNS hijacking server.
 */
void dns_server_stop(void);
