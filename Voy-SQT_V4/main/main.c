#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
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
#include "geolocation.h"
#include "display.h"
#include "ssd1306.h"

extern EventGroupHandle_t wifi_event_group;

static const char *TAG = "main";

static int fall_event_count;
static SemaphoreHandle_t fall_event_mutex;

static int temp_event_count;
static int hum_event_count;
static SemaphoreHandle_t temp_hum_event_mutex;

long_lat_t loc;
bool location_flag = false;
static SemaphoreHandle_t location_mutex;

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
    #define TEMP_H_THRESHOLD 40
    #define TEMP_L_THRESHOLD 22
    #define HUM_H_THRESHOLD 45
    #define HUM_L_THRESHOLD 20

    temp_hum_data_t data;
    data.temperature = 25.0;
    data.humidity = 25.0;

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

        // IMU data
        int fall_event_count_out = 0;

        // Temperature and humidity data
        int temp_event_count_out = 0;
        int hum_event_count_out = 0;

        // Location data
        double longitude_out = 0.0;
        double latitude_out = 0.0;
        bool location_flag_out = false;
        
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

        // Check for location updates
        if (xSemaphoreTake(location_mutex, 25)) {
            location_flag_out = location_flag;
            if (location_flag) {
                ESP_LOGI(__func__, "Location updated");
                location_flag = false;
                longitude_out = loc.longitude;
                latitude_out = loc.latitude;
            }
            xSemaphoreGive(location_mutex);
        }          
        
        // Send log if any events/updates detected
        if ((fall_event_count_out + temp_event_count_out + hum_event_count_out != 0) || location_flag_out) {
            send_post_request(fall_event_count_out, temp_event_count_out, hum_event_count_out, longitude_out, latitude_out);
        }

        // Check every 5s
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void geolocation_task(void *pvParameter) {

    // Create mutex for shock counter
    location_mutex = xSemaphoreCreateMutex();

    while (1) {
        
        char *received_json = NULL;
        if (xQueueReceive(wifi_json_queue, &received_json, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(__func__, "Received JSON data:");
            if (xSemaphoreTake(location_mutex, 25)) {
                location_flag = true;
                loc = process_geolocation_json(received_json);
                xSemaphoreGive(location_mutex);
            }          
        }

        // Check every 30s
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void wifi_scan_task(void *pvParameter) {
    // Wait until WiFi is connected.
    ESP_LOGI("wifi_scan_task", "Waiting for WiFi connection...");
    xEventGroupWaitBits(wifi_event_group, BIT0, false, true, portMAX_DELAY);
    ESP_LOGI("wifi_scan_task", "WiFi connected, starting WiFi scan task.");

    while (1) {
        size_t num_aps = 0;
        wifi_ap_t *wifi_aps = create_wifi_aps_array(&num_aps);

        if (wifi_aps && num_aps > 0) {
            char *json_str = generate_wifi_scan_json(wifi_aps, num_aps);
            if (json_str) {
                ESP_LOGI(__func__, "Generated scan JSON");

                // Send the JSON pointer via the queue.
                if (xQueueSend(wifi_json_queue, &json_str, pdMS_TO_TICKS(100)) != pdPASS) {
                    ESP_LOGE("wifi_scan_task", "Failed to send JSON to queue; freeing json_str");
                    free(json_str);
                }
            } 
            else {
                ESP_LOGE(__func__, "Failed to generate JSON payload.");
            }
            free(wifi_aps);
        } 
        else {
            ESP_LOGI(__func__, "No WiFi access points detected.");
        }

        // Scan every 120s
        vTaskDelay(pdMS_TO_TICKS(120000));
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

    wifi_json_queue = xQueueCreate(5, sizeof(char *));
    if (wifi_json_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create wifi_json_queue");
    }

    ESP_LOGI(TAG, "Initializing RTOS tasks");
    xTaskCreate(imu_task, "IMU_Task", 2048, NULL, 2, NULL);
    xTaskCreate(temp_hum_sensor_task, "Temp_Hum_Task", 4096, NULL, 3, NULL);
    xTaskCreate(wifi_scan_task, "WiFi_Scan_Task", 2048, NULL, 4, NULL);
    xTaskCreate(geolocation_task, "Geolocation_Task", 4096, NULL, 5, NULL);
    xTaskCreate(http_task, "HTTP_Task", 4096, NULL, 6, NULL);
    xTaskCreate(display_task, "Display_Task", 8196, NULL, 7, NULL);
}
