#include "fft_analysis.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "shared_defs.h"

// Configuration Constants
#define QUEUE_RECEIVE_DELAY  5       // ms to wait for queue items

// Global averages storage
float avgs[NUM_OF_SAMPLES_AGGREGATE] = {0};

/**
 * @brief Prints formatted averages list to serial output
 * @details Output format:
 * --- Averages List ---
 * Average [1]: 12.34
 * ...
 * ---------------------
 */
void printAverages() {
  Serial.println("\n--- Averages List ---");
  
  for (int i = 0; i < NUM_OF_SAMPLES_AGGREGATE; i++) {
    Serial.print("Average [");
    Serial.print(i + 1);
    Serial.print("]: ");
    Serial.println(avgs[i], 2); 
  }
  
  Serial.println("---------------------");
}

/**
 * @brief Moving average calculation task
 * @param pvParameters FreeRTOS task parameters (unused)
 * 
 * @implements
 * - Circular buffer for WINDOW_SIZE samples
 * - Moving average calculation
 * - Results storage in avgs[] array
 * 
 */
void average_task_handler(void *pvParameters) {
  float sum = 0;
  float average = 0;
  float value;
  
  // Circular buffer implementation
  float sampleReadings[WINDOW_SIZE] = {0};  // Storage for sliding window
  int num_of_samples = 0;   // Total processed samples counter
  int pos = 0;              // Current position in circular buffer
  int valid_samples = 0;    // Count of initialized buffer elements

  while (1) {
      if (xQueueReceive(xQueueSamples, &(value), (TickType_t)5)) {
        
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
        avgs[num_of_samples] = average;
        Serial.printf("[AGGREGATE] Window %d: %.2f\n", num_of_samples, average);
        
        xQueueSend(xQueueAvgs, &average, (TickType_t)portMAX_DELAY);

        num_of_samples++;

        if(num_of_samples >= NUM_OF_SAMPLES_AGGREGATE){
          Serial.print("*************\n");
          Serial.print("Average task finished\n");
          Serial.print("*************\n");
          printAverages();
          break;
        }
      }
  }
  vTaskDelete(NULL);
}