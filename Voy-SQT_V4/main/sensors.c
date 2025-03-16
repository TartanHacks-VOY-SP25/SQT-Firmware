#include "io_pins.h"
#include "sensors.h"
#include <stdio.h>
#include "dht.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/adc.h"

imu_data_t read_imu() {
    imu_data_t data;

    data.x = adc1_get_raw(IMU_X_PIN);
    data.y = adc1_get_raw(IMU_Y_PIN);
    data.z = adc1_get_raw(IMU_Z_PIN);

    return data;
}

temp_hum_data_t read_temp_hum_sensor() {
    temp_hum_data_t data;

    if (dht_read_float_data(DHT_TYPE_DHT11, TEMP_HUM_PIN, 
                            &data.humidity, &data.temperature) == ESP_OK) {
        return data;
    }
    data.temperature = 25.0;
    data.humidity = 25.0;

    return data;
}
