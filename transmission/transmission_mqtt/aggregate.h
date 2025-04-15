#pragma once
#include <Arduino.h>
#include "shared_defs.h"
#include "config.h"

// Global averages array
extern float avgs[NUM_OF_SAMPLES_AGGREGATE];

// Function declarations
void printAverages();
void average_task_handler(void *args);