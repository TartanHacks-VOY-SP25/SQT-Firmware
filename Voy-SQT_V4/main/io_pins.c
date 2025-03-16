#include "io_pins.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_err.h"

static const char *TAG = "io_pins";

void io_pins_init() {

    // Configure IMU_X, IMU_Y, IMU_Z as Analog Inputs
    adc1_config_width(ADC_WIDTH_BIT_12);

    // Assign each pin to an ADC channel
    adc1_config_channel_atten(IMU_X_PIN, ADC_ATTEN_DB_12); 
    adc1_config_channel_atten(IMU_Y_PIN, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(IMU_Z_PIN, ADC_ATTEN_DB_12);

    // Configure IMU Test Pin as Digital Output
    gpio_reset_pin(IMU_TEST_PIN);
    gpio_set_direction(IMU_TEST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(IMU_TEST_PIN, 0);
}
