#ifndef GEOLOCATION_H
#define GEOLOCATION_H

#include <stdio.h>
#include "freertos/queue.h"

#ifdef USE_PRIVATE_CONFIG
#include "private_config.h"
#else
#define GEOLOCATION_URL "https://www.googleapis.com/geolocation/v1/geolocate?key=YOUR_API_KEY_HERE"
#endif

#define MAX_APS 8

extern QueueHandle_t wifi_json_queue;

typedef struct {
    double longitude;
    double latitude;
} long_lat_t;

extern long_lat_t global_location; // Declaration of the global variable

// Define a structure to hold WiFi access point details.
typedef struct {
    char mac[18];             
    int signal_strength;      
    int signal_to_noise_ratio;
} wifi_ap_t;

// Function prototypes
esp_err_t _geolocation_event_handler(esp_http_client_event_t *evt);
void pretty_print_json(const char *json_str);
wifi_ap_t *create_wifi_aps_array(size_t *num_aps_out);
char *generate_wifi_scan_json(const wifi_ap_t *wifi_aps, size_t num_aps);
long_lat_t process_geolocation_json(char *json_str);

#endif // GEOLOCATION_H