#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "http.h"

static const char *TAG = "HTTP";

esp_err_t _backend_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_DATA:
            if (!evt->data_len) {
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, no data in response");
            } else {
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

void send_post_request(int fall_events, int overtemp_events, int overhum_events, double longitude, double latitude) {
    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = _backend_http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Prepare POST data
    char post_data[128];
    snprintf(post_data, sizeof(post_data), "{\"uid\":%d, \"long\":%f, \"lat\":%f, \"fall\":%d, \"temp\":%d, \"hum\":%d}", 2808, longitude, latitude, fall_events, overtemp_events, overhum_events);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Perform the POST request
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "POST request successful, status code: %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "POST request failed, error: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "Sending POST request to %s with data: %s", SERVER_URL, post_data);

    // Cleanup
    esp_http_client_cleanup(client);
}