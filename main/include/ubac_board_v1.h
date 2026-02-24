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
