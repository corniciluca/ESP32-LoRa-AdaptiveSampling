#include <Arduino.h>
#include "communication.h"
#include "fft_analysis.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "shared_defs.h"
#include "aggregate.h"

// Configuration Constants
#define TASK_STACK_SIZE      4096    // Bytes per task stack
#define SERIAL_BAUD_RATE     115200  // Serial monitor speed

/**
 * @brief WiFi/MQTT communication initialization task
 * @param pvParameters FreeRTOS task parameters (unused)
 * 
 * @operation
 * - Initializes WiFi connection
 * - Starts MQTT communication stack
 * - Sends averages to the edge server in background
 * 
 */
void comunication_task(void *pvParameters) {
  wifi_init();
}

/**
 * @brief System initialization task
 * @param pvParameters FreeRTOS task parameters (unused)
 * 
 * @sequence
 * 1. FFT module initialization
 * 2. Shared queue creation
 * 3. Worker task creation
 */
void startingTask(void *pvParameters) {
  
  // System calibration
  fft_init();

  // Queues initialization
  init_shared_queues();

  xTaskCreate(fft_sampling_task, "Acquisition", TASK_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(average_task_handler, "Averaging", TASK_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(comunication_task, "Comunication", TASK_STACK_SIZE, NULL, 1, NULL);

  vTaskDelete(NULL);
}



void setup() {
  // Serial interface initialization
  Serial.begin(SERIAL_BAUD_RATE);
  while(!Serial); // Wait for serial monitor
  Serial.println("[SYS] System initialized");

  xTaskCreate(startingTask, "Bootstrap", TASK_STACK_SIZE, NULL, 2, NULL);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
