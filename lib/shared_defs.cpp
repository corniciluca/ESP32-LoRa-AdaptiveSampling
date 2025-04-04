#include "shared_defs.h"
#include <Arduino.h>
#include "config.h"

QueueHandle_t xQueueSamples = NULL;
QueueHandle_t xQueueAvgs = NULL;

void init_shared_queues() {
    xQueueSamples = xQueueCreate(NUM_OF_SAMPLES_AGGREGATE, sizeof(float));
    xQueueAvgs = xQueueCreate(NUM_OF_SAMPLES_AGGREGATE, sizeof(float));
    
    if(xQueueSamples == NULL) {
        Serial.println("Queue creation failed!");
        while(1); // Halt on critical failure
    }
}