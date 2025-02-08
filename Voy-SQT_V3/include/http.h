#ifndef HTTP_H
#define HTTP_H

#include "esp_http_client.h"

// DHT11 Temperature/Humidity Sensor Pins
#define SERVER_URL   "http://172.26.34.44:8080"

// Function prototypes
esp_err_t _http_event_handler(esp_http_client_event_t *evt);
void send_post_request(int fall_events, int overtemp_events, int overhum_events);

#endif // HTTP_H
