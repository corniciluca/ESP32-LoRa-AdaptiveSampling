#include "shared_defs.h"
#include <Arduino.h>
#include "config.h"

QueueHandle_t xQueueSamples = NULL;
QueueHandle_t xQueueAvgs = NULL;
TaskHandle_t xCommunicationTaskHandle = NULL;

void init_shared_queues() {
    xQueueSamples = xQueueCreate(QUEUE_SIZE, sizeof(float));
    xQueueAvgs = xQueueCreate(QUEUE_SIZE, sizeof(float));
    if(xQueueSamples ==  NULL || xQueueAvgs ==  NULL ) {
        Serial.println("Queue creation failed!");
        while(1); // Halt on critical failure
    }
}