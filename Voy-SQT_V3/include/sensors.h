#ifndef SENSORS_H
#define SENSORS_H

// #include "driver/gpio.h"  // ESP-IDF GPIO driver
// #include "driver/adc.h"  // ESP-IDF GPIO driver

typedef struct {
    int x;
    int y;
    int z;
} imu_data_t;

typedef struct {
    float temperature;
    float humidity;
} temp_hum_data_t;

// Function prototypes
imu_data_t read_imu();
temp_hum_data_t read_temp_hum_sensor();

#endif // SENSORS_H
