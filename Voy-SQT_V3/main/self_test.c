#include "self_test.h"
#include "io_pins.h"
#include "sensors.h"
#include "http.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

bool test_temp_hum_sensor() {
    temp_hum_data_t data = read_temp_hum_sensor();
    return data.temperature != 0.0 && data.humidity != 0.0;
}

bool test_imu() {
    // Get initial reading
    imu_data_t data = read_imu();

    // Set to self-test mode (apply force to sensor)
    gpio_set_level(IMU_TEST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Get second reading
    imu_data_t data_new = read_imu();

    // Ensure readings have changed
    int eps = 10;
    bool x_working = abs(data.x - data_new.x) > eps;
    bool y_working = abs(data.y - data_new.y) > eps;
    bool z_working = abs(data.z - data_new.z) > eps;
    gpio_set_level(IMU_TEST_PIN, 0);

    // Ensure readings have changed
    return x_working && y_working && z_working;
}

// Run all system tests before starting tasks
bool run_self_test() {

    bool temp_hum_sensor_ok = test_temp_hum_sensor();
    bool imu_ok = test_imu();

    return imu_ok && temp_hum_sensor_ok;
}
