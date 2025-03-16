#ifndef IO_PINS_H
#define IO_PINS_H

#include "driver/gpio.h"  // ESP-IDF GPIO driver
#include "driver/adc.h"  // ESP-IDF GPIO driver

// DHT11 Temperature/Humidity Sensor Pins
#define TEMP_HUM_PIN   GPIO_NUM_33

// ADXL327 Accelerometer Pins
#define IMU_X_PIN       ADC1_CHANNEL_4
#define IMU_Y_PIN       ADC1_CHANNEL_6
#define IMU_Z_PIN       ADC1_CHANNEL_7
#define IMU_TEST_PIN    GPIO_NUM_14

// I2C Pins
#define I2C_SDA         GPIO_NUM_21
#define I2C_SCL         GPIO_NUM_22

// Function prototypes
void io_pins_init();

#endif // IO_PINS_H
