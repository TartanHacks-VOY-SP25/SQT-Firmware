#ifndef WIFI_H
#define WIFI_H

#include "driver/gpio.h"
#include "esp_event.h"

// WiFi Details
#define WIFI_SSID        "Lumio-Dev"
#define WIFI_PASS        "omsohamom"
#define WIFI_HOSTNAME    "SQT-2808"
#define MAX_WIFI_RETRY   5

// Function Prototypes
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data);
void wifi_init_sta(void);

#endif // WIFI_H
