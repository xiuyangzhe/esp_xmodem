#include <stdio.h>
#include "esp_event.h"
#include "freertos/event_groups.h"

void wifiConnect(char *sid, char *password, esp_event_handler_t evet_handler);

void get_wifi_signal_strength();
