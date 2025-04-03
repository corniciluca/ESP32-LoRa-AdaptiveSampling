#pragma once
#include <FreeRTOS.h>
#include <queue.h>
#include "config.h"

// Shared queues for inter-task communication
extern QueueHandle_t xQueueSamples;
extern QueueHandle_t xQueueAvgs;

// Initialization function
void init_shared_queues();