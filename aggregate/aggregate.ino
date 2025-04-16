#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <fft_analysis.h>
#include <shared_defs.h>

// Configuration Constants
#define TASK_STACK_SIZE      4096    // Bytes per task stack
#define SERIAL_BAUD_RATE     115200  // Serial monitor speed
#define SIZE_AVG_ARRAY NUM_OF_SAMPLES_AGGREGATE-WINDOW_SIZE+1
// Global averages storage
float averages[SIZE_AVG_ARRAY] = {0};


/**
 * @brief Prints formatted averages list to serial output
 * @details Output format:
 * --- Averages List ---
 * Average [1]: 12.34
 * ...
 * ---------------------
 */
void print_averages() {
  Serial.println("\n--- Averages List ---");
  
  for (int i = 0; i < SIZE_AVG_ARRAY; i++) {
    Serial.print("Average [");
    Serial.print(i + 1);
    Serial.print("]: ");
    Serial.println(averages[i], 2); 
  }
  
  Serial.println("---------------------");
}

/**
 * @brief Main sampling task handler
 * @param pvParameters FreeRTOS task parameters (unused)
 * @details
 * - Generates signal samples at configured rate
 * - Pushes samples to processing queue
 * - Self-terminates after acquiring NUM_OF_SAMPLES_AGGREGATE
 * 
 * @warning Depends on initialized queue (xQueueSamples)
 */
void sampling_task(void *pvParameters) {
    float sample = 0.0f;
    
    Serial.printf("[SAMPLING] Starting sampling at %d Hz\n", g_sampling_frequency);
    Serial.println("--------------------------------");

    for (int i = 0; i < NUM_OF_SAMPLES_AGGREGATE; i++) {
        sample = sample_signal(curr_signal, i, g_sampling_frequency);
        
        xQueueSend(xQueueSamples, &sample, 0);

        Serial.printf("[SAMPLING] Sample %d: %.2f\n", i, sample);
        vTaskDelay(pdMS_TO_TICKS(1000/g_sampling_frequency));
    }

    Serial.println("--------------------------------");
    Serial.println("[SAMPLING] Sampling completed");
    vTaskDelete(NULL);
}


/**
 * @brief Moving average calculation task
 * @param pvParameters FreeRTOS task parameters (unused)
 * 
 * @implements
 * - Circular buffer for WINDOW_SIZE samples
 * - Moving average calculation
 * - Results storage in averages[] array
 * 
 */
void average_task(void *pvParameters) {
  float sum = 0;
  float average = 0;
  float value;
  
  // Circular buffer implementation
  float sampleReadings[WINDOW_SIZE] = {0};  // Storage for sliding window
  int num_of_avgs = 0;   // Total processed samples counter
  int pos = 0;              // Current position in circular buffer
  int valid_samples = 0;    // Count of initialized buffer elements

  while (1) {
    if (xQueueReceive(xQueueSamples, &value, (TickType_t)portMAX_DELAY)) {
      
      // Update circular buffer
      sampleReadings[pos] = value;
      pos = (pos + 1) % WINDOW_SIZE;

      Serial.printf("[AGGREGATE] Sample read: %.2f\n",value);

      if (valid_samples < WINDOW_SIZE) valid_samples++; // Ensure we don't exceed the array size
      // Calculate moving average
      sum = 0;
      for (int i = 0; i < valid_samples; i++) {
          sum += sampleReadings[i];
      }
      average = sum / WINDOW_SIZE;

      // Store and log results
      if(valid_samples == WINDOW_SIZE){
        averages[num_of_avgs] = average;
        Serial.printf("[AGGREGATE] Window %d: %.2f\n", num_of_avgs, average);
        num_of_avgs++;
      }
      
      if(num_of_avgs >= SIZE_AVG_ARRAY){
        Serial.print("*************\n");
        Serial.print("Average task finished\n");
        Serial.print("*************\n");
        print_averages();
        break;
      }
    }
  }
  vTaskDelete(NULL);
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

  xTaskCreate(sampling_task, "Acquisition", TASK_STACK_SIZE, NULL, 2, NULL);
  xTaskCreate(average_task, "Averaging", TASK_STACK_SIZE, NULL, 1, NULL);

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
