#pragma once
#define WINDOW_SIZE 5

#define WIFI_MAX_RETRIES 10
#define MSG_BUFFER_SIZE 50
#define RETRY_DELAY 2000 / portTICK_PERIOD_MS
#define MQTT_LOOP 1000 / portTICK_PERIOD_MS
#define PUBLISH_TOPIC "luca/esp32/data"
#define SUBSCRIBE_TOPIC "luca/esp32/acks"

#define INIT_SAMPLE_RATE 1000 // Hz
#define NUM_SAMPLES 1024

#define NUM_OF_SAMPLES_AGGREGATE 10
