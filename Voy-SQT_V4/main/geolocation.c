#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "geolocation.h"
#include "esp_crt_bundle.h"

static const char *TAG = "GEO";

QueueHandle_t wifi_json_queue = NULL;

// Define a structure to hold the response buffer and current offset.
typedef struct {
    char *buffer;    // Pointer to response buffer.
    int offset;      // Current offset in the buffer.
    int buffer_len;  // Total length of the buffer.
} response_data_t;

// Helper function to pretty-print a JSON string.
void pretty_print_json(const char *json_str) {
    if (json_str == NULL) {
        printf("No JSON string provided.\n");
        return;
    }

    // Parse the input JSON string into a cJSON object.
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        printf("Failed to parse JSON.\n");
        return;
    }

    // Generate a formatted (pretty-printed) JSON string.
    char *formatted = cJSON_Print(json);
    if (formatted == NULL) {
        printf("Failed to generate pretty JSON string.\n");
        cJSON_Delete(json);
        return;
    }

    // Print and free JSON string
    printf("%s\n", formatted);
    free(formatted);
    cJSON_Delete(json);
}

wifi_ap_t *create_wifi_aps_array(size_t *num_aps_out) {
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        printf("Error starting WiFi scan: %d\n", err);
        return NULL;
    }

    uint16_t ap_num = 0;
    err = esp_wifi_scan_get_ap_num(&ap_num);
    if (err != ESP_OK) {
        printf("Error getting AP number: %d\n", err);
        return NULL;
    }

    // Apply the cap, limit to MAX_APS
    if (ap_num > MAX_APS) {
        ap_num = MAX_APS;
    }

    if (ap_num == 0) {
        *num_aps_out = 0;
        return NULL;
    }

    // Allocate storage for the raw scan results
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (ap_records == NULL) {
        printf("Failed to allocate memory for AP records\n");
        return NULL;
    }

    err = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
    if (err != ESP_OK) {
        printf("Error getting AP records: %d\n", err);
        free(ap_records);
        return NULL;
    }

    wifi_ap_t *wifi_aps = malloc(sizeof(wifi_ap_t) * ap_num);
    if (wifi_aps == NULL) {
        printf("Failed to allocate memory for wifi_aps array\n");
        free(ap_records);
        return NULL;
    }

    for (int i = 0; i < ap_num; i++) {
        snprintf(wifi_aps[i].mac, sizeof(wifi_aps[i].mac), 
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 ap_records[i].bssid[0], ap_records[i].bssid[1],
                 ap_records[i].bssid[2], ap_records[i].bssid[3],
                 ap_records[i].bssid[4], ap_records[i].bssid[5]);
        wifi_aps[i].signal_strength = ap_records[i].rssi;
        wifi_aps[i].signal_to_noise_ratio = 0;
    }

    *num_aps_out = ap_num;
    free(ap_records);

    return wifi_aps;
}

char *generate_wifi_scan_json(const wifi_ap_t *wifi_aps, size_t num_aps) {
    // Create the root JSON object.
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    // Add the "considerIp" key with the string value "false".
    if (!cJSON_AddStringToObject(root, "considerIp", "false")) {
        cJSON_Delete(root);
        return NULL;
    }

    // Create the "wifiAccessPoints" array.
    cJSON *ap_array = cJSON_CreateArray();
    if (!ap_array) {
        cJSON_Delete(root);
        return NULL;
    }
    // Attach the array to the root object.
    cJSON_AddItemToObject(root, "wifiAccessPoints", ap_array);

    // Iterate through the provided WiFi access points.
    for (size_t i = 0; i < num_aps; i++) {
        // Create a JSON object for each access point.
        cJSON *ap_obj = cJSON_CreateObject();
        if (!ap_obj) {
            cJSON_Delete(root);
            return NULL;
        }

        // Add the MAC address.
        if (!cJSON_AddStringToObject(ap_obj, "macAddress", wifi_aps[i].mac)) {
            cJSON_Delete(root);
            return NULL;
        }
        // Add the signal strength.
        if (!cJSON_AddNumberToObject(ap_obj, "signalStrength", wifi_aps[i].signal_strength)) {
            cJSON_Delete(root);
            return NULL;
        }
        // Add the signal-to-noise ratio.
        if (!cJSON_AddNumberToObject(ap_obj, "signalToNoiseRatio", wifi_aps[i].signal_to_noise_ratio)) {
            cJSON_Delete(root);
            return NULL;
        }
        // Append the access point object to the array.
        cJSON_AddItemToArray(ap_array, ap_obj);
    }

    // Convert the entire JSON object to a string.
    // cJSON_PrintUnformatted allocates memory for the string.
    char *json_str = cJSON_PrintUnformatted(root);

    // Free the JSON object now that we have the string.
    cJSON_Delete(root);

    // Return the JSON string. The caller is responsible for calling free(json_str).
    return json_str;
}

// Updated event handler that uses our custom response_data_t for accumulating data.
esp_err_t _geolocation_event_handler(esp_http_client_event_t *evt) {
    response_data_t *res_data = (response_data_t *)evt->user_data;
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
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (res_data && res_data->buffer && (res_data->offset + evt->data_len < res_data->buffer_len)) {
                memcpy(res_data->buffer + res_data->offset, evt->data, evt->data_len);
                res_data->offset += evt->data_len;
                // Ensure buffer is null-terminated.
                res_data->buffer[res_data->offset] = '\0';
            } else {
                ESP_LOGE(TAG, "Response buffer overflow or not initialized");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Function to process the JSON by sending it to the Google Geolocation API,
// then printing the full response and extracting latitude and longitude.
long_lat_t process_geolocation_json(char *json_str) {
    ESP_LOGI(TAG, "Sending WiFi data to Google API");

    long_lat_t loc = {0};

    // Allocate a response buffer.
    char response_buffer[512] = {0};

    // Create our response data structure.
    response_data_t res_data = {
        .buffer = response_buffer,
        .offset = 0,
        .buffer_len = sizeof(response_buffer)
    };

    esp_http_client_config_t config = {
        .url = GEOLOCATION_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .buffer_size = sizeof(response_buffer),
        .user_data = &res_data,               
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _geolocation_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(json_str);
        return loc;
    }

    // Set the Content-Type header and assign the JSON payload.
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    // Perform the HTTP request.
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);

        // Use the accumulated response from our response_data_t structure.
        if (res_data.offset > 0) {

            // Parse the JSON response to extract latitude and longitude.
            cJSON *root = cJSON_Parse(res_data.buffer);
            if (root) {
                cJSON *location = cJSON_GetObjectItem(root, "location");
                if (location) {
                    cJSON *lat = cJSON_GetObjectItem(location, "lat");
                    cJSON *lng = cJSON_GetObjectItem(location, "lng");
                    if (lat && lng) {
                        ESP_LOGI(TAG, "Latitude: %f, Longitude: %f", lat->valuedouble, lng->valuedouble);
                        loc.longitude = lng->valuedouble;
                        loc.latitude = lat->valuedouble;
                    }
                    else {
                        ESP_LOGE(TAG, "Failed to retrieve coordinates from JSON response.");
                    }
                } 
                else {
                    ESP_LOGE(TAG, "No location field found in JSON response.");
                }
                cJSON_Delete(root);
            } 
            else {
                ESP_LOGE(TAG, "Failed to parse JSON response.");
            }
        } 
        else {
            ESP_LOGE(TAG, "No response data received in the buffer.");
        }
    } 
    else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_str);
    return loc;
}