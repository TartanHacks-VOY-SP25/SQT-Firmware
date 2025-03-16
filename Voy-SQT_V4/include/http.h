#ifndef HTTP_H
#define HTTP_H

#include "esp_http_client.h"

// DHT11 Temperature/Humidity Sensor Pins
#define SERVER_URL   "http://192.168.22.136:8000/sensors/sensor_data"

// Function prototypes
esp_err_t _backend_http_event_handler(esp_http_client_event_t *evt);
void send_post_request(int fall_events, int overtemp_events, int overhum_events, double longitude, double latitude);

#endif // HTTP_H
