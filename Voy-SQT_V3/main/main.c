#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "sensors.h"
#include "http.h"
#include "self_test.h"
#include "io_pins.h"
#include "wifi.h"
#include "display.h"
#include "ssd1306.h"

static const char *TAG = "main";

static int fall_event_count;
static SemaphoreHandle_t fall_event_mutex;

static int temp_event_count;
static int hum_event_count;
static SemaphoreHandle_t temp_hum_event_mutex;

void imu_task(void *pvParameter) {
    #define FALL_EVENT_COOLDOWN 2000

    imu_data_t data = read_imu();
    bool in_cooldown = false;
    TickType_t last_fall_event_time = 0;

    // Create mutex for shock counter
    fall_event_mutex = xSemaphoreCreateMutex();

    while (1) {
        // Measure current IMU data
        imu_data_t data_new = read_imu();

        // Check for significant change on all axes
        int eps = 500;
        bool x_shock = abs(data.x - data_new.x) > eps;
        bool y_shock = abs(data.y - data_new.y) > eps;
        bool z_shock = abs(data.z - data_new.z) > eps;

        // Check if fall detected outside of cooldown
        if (!in_cooldown && (x_shock || y_shock || z_shock)) {
            // Increment fall event counter
            if (xSemaphoreTake(fall_event_mutex, 25)) {
                ESP_LOGI(__func__, "Fall event detected!");
                fall_event_count++;
                xSemaphoreGive(fall_event_mutex);
            }
            in_cooldown = true;
            last_fall_event_time = xTaskGetTickCount();
        }

        // Check if cooldown period (5 seconds) has passed
        if (in_cooldown && (xTaskGetTickCount() - last_fall_event_time >= pdMS_TO_TICKS(FALL_EVENT_COOLDOWN))) {
            in_cooldown = false;
        }

        // Push back adc values for next poll
        data.x = data_new.x;
        data.y = data_new.y;
        data.z = data_new.z;

        // Poll every 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void temp_hum_sensor_task(void *pvParameter) {
    #define TEMP_H_THRESHOLD 35
    #define TEMP_L_THRESHOLD 22
    #define HUM_H_THRESHOLD 45
    #define HUM_L_THRESHOLD 20

    temp_hum_data_t data;
    data.temperature = 0.0;
    data.humidity = 0.0;

    // Create mutex for shock counter
    temp_hum_event_mutex = xSemaphoreCreateMutex();

    while (1) {
        // Measure temperature and humidity
        temp_hum_data_t data_new = read_temp_hum_sensor();

        // Check for temperature event
        if (data_new.temperature > TEMP_H_THRESHOLD && data.temperature < TEMP_L_THRESHOLD) {
            // Increment temperature event counter
            if (xSemaphoreTake(temp_hum_event_mutex, 25)) {
                ESP_LOGI(__func__, "Temperature event detected!");
                temp_event_count++;
                xSemaphoreGive(temp_hum_event_mutex);
            }
        }
        // Check for humidity event
        if (data_new.humidity > HUM_H_THRESHOLD && data.humidity < HUM_L_THRESHOLD) {
            // Increment humidity event counter
            if (xSemaphoreTake(temp_hum_event_mutex, 25)) {
                ESP_LOGI(__func__, "Humidity event detected!");
                hum_event_count++;
                xSemaphoreGive(temp_hum_event_mutex);
            }
        }

        // Push back values for next poll
        data.temperature = data_new.temperature;
        data.humidity = data_new.humidity;

        // Poll every 2s
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void http_task(void *pvParameter) {
    while (1) {
        int fall_event_count_out = 0;
        int temp_event_count_out = 0;
        int hum_event_count_out = 0;
        
        // Check for fall events
        if (xSemaphoreTake(fall_event_mutex, 25)) {
            if (fall_event_count != 0) {
                ESP_LOGI(__func__, "Detected %i fall events", fall_event_count);
                fall_event_count_out = fall_event_count;
                fall_event_count = 0;
            }
            xSemaphoreGive(fall_event_mutex);
        }
        // Check for temperature/humidity events
        if (xSemaphoreTake(temp_hum_event_mutex, 25)) {
            if (temp_event_count != 0) {
                ESP_LOGI(__func__, "Detected %i overtemp events", temp_event_count);
                temp_event_count_out = temp_event_count;
                temp_event_count = 0;
            }
            if (hum_event_count != 0) {
                ESP_LOGI(__func__, "Detected %i overtemp events", hum_event_count);
                hum_event_count_out = hum_event_count;
                hum_event_count = 0;
            }
            xSemaphoreGive(temp_hum_event_mutex);
        }

        // Send log if any events detected
        if (fall_event_count_out + temp_event_count_out + hum_event_count_out != 0) {
            send_post_request(fall_event_count_out, temp_event_count_out, hum_event_count_out);
        }

        // Check every 5s
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void display_task(void *pvParameter) {
    SSD1306_t disp = init_display();

    ssd1306_clear_screen(&disp, false);
    ssd1306_contrast(&disp, 0xff);
    ssd1306_display_text_x3(&disp, 0, "SQT-2808", 8, false);
    vTaskDelay(pdMS_TO_TICKS(3000));
    ssd1306_clear_screen(&disp, false);

    while (1) {
        // Check for fall events
        if (xSemaphoreTake(fall_event_mutex, 25)) {
            if (fall_event_count != 0) {
                ssd1306_display_text_x3(&disp, 0, "Fall!", 5, false);
            }
            xSemaphoreGive(fall_event_mutex);
            vTaskDelay(pdMS_TO_TICKS(3000));
            ssd1306_clear_screen(&disp, false);
        }
        // Check for temperature/humidity events
        if (xSemaphoreTake(temp_hum_event_mutex, 25)) {
            if (temp_event_count != 0) {
                ssd1306_display_text_x3(&disp, 0, "Hot!", 5, false);
            }
            else if (hum_event_count != 0) {
                ssd1306_display_text_x3(&disp, 0, "Wet!", 5, false);
            }
            xSemaphoreGive(temp_hum_event_mutex);
            vTaskDelay(pdMS_TO_TICKS(3000));
            ssd1306_clear_screen(&disp, false);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main() {
    io_pins_init();
    vTaskDelay(pdMS_TO_TICKS(250));

    // Run self-test before starting tasks
    if (!run_self_test()) {
        ESP_LOGI(TAG, "Self test failed!");
    }
    ESP_LOGI(TAG, "Self test passed");

    // Initialize NVS (WiFi credentials)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi");
    wifi_init_sta();

    ESP_LOGI(TAG, "Initializing RTOS tasks");
    xTaskCreate(imu_task, "IMU_Task", 2048, NULL, 2, NULL);
    xTaskCreate(temp_hum_sensor_task, "Temp_Hum_Task", 4096, NULL, 3, NULL);
    xTaskCreate(http_task, "HTTP_Task", 4096, NULL, 4, NULL);
    xTaskCreate(display_task, "Display_Task", 8196, NULL, 5, NULL);
}
