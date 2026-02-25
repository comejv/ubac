/*
 * UBAC: LED Indications Manager implementation.
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
 * @file led_indications.c
 * @brief LED Indications Manager implementation.
 */

#include "drivers/led_indications.h"
#include "driver/gpio.h"
#include "drivers/ubac_board_v1.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_INDICATIONS";

// Internal state
static led_color_t s_current_color = LED_COLOR_OFF;
static led_mode_t s_current_mode = LED_MODE_OFF;

static void set_gpios(led_color_t color)
{
  gpio_set_level(LED_RED_PIN, (color & LED_COLOR_RED) ? 1 : 0);
  gpio_set_level(LED_GREEN_PIN, (color & LED_COLOR_GREEN) ? 1 : 0);
  gpio_set_level(LED_BLUE_PIN, (color & LED_COLOR_BLUE) ? 1 : 0);
}

static void led_task(void *pvParameters)
{
  bool toggle = false;
  while (1)
  {
    uint32_t delay_ms = 500;

    switch (s_current_mode)
    {
    case LED_MODE_OFF:
      set_gpios(LED_COLOR_OFF);
      delay_ms = 1000;
      break;
    case LED_MODE_SOLID:
      set_gpios(s_current_color);
      delay_ms = 1000;
      break;
    case LED_MODE_BLINK_SLOW:
      set_gpios(toggle ? s_current_color : LED_COLOR_OFF);
      toggle = !toggle;
      delay_ms = 500;
      break;
    case LED_MODE_BLINK_FAST:
      set_gpios(toggle ? s_current_color : LED_COLOR_OFF);
      toggle = !toggle;
      delay_ms = 100;
      break;
    case LED_MODE_PULSE:
      // Pulse implementation (simple for now: fast blink with low duty cycle)
      set_gpios(s_current_color);
      vTaskDelay(pdMS_TO_TICKS(50));
      set_gpios(LED_COLOR_OFF);
      delay_ms = 950;
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

esp_err_t led_indications_init(void)
{
  gpio_config_t io_conf = {
      .pin_bit_mask = ((1ULL << LED_RED_PIN) | (1ULL << LED_GREEN_PIN) | (1ULL << LED_BLUE_PIN)),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t err = gpio_config(&io_conf);
  if (err != ESP_OK)
    return err;

  xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);
  ESP_LOGI(TAG, "LED Indications Initialized");
  return ESP_OK;
}

void led_indications_set(led_color_t color, led_mode_t mode)
{
  s_current_color = color;
  s_current_mode = mode;
}
