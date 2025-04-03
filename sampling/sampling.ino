#include <Arduino.h>
#include <fft_analysis.h>
#include "config.h"

// Configuration Constants
#define TASK_STACK_SIZE  4096    // Stack size for sampling task
#define TASK_PRIORITY    2       // Medium priority for sampling task
#define SERIAL_BAUD      115200

// Task handles
TaskHandle_t optimal_sampling_freq_task_handle = NULL;

/**
 * @brief Determines and applies optimal sampling frequency based on signal analysis
 * @param pvParameters FreeRTOS task parameters (unused)
 * 
 * @details 
 * - Performs initial signal sampling using default frequency
 * - Processes FFT to identify dominant frequency component
 * - Adjusts sampling rate according to Nyquist Theorem (2.5× peak frequency)
 * - Ensures sampling efficiency while preventing aliasing
 * 
 * @note 
 * - Uses global variable g_sampling_frequency for configuration
 * 
 * @see signal_1() Default signal function
 * @see get_max_frequency() For peak frequency detection
 * @see adjust_sampling_rate() For rate adaptation logic
 */
void optimal_sampling_freq(void *pvParameters) {
  Serial.println("[FFT] Signal sampling task started");
  
  // Initial FFT analysis with default signal and initial sample frequency
  sampling(signal_1, INIT_SAMPLE_RATE);
  process_fft();
  
  // Frequency analysis
  float max_frequency = get_max_frequency();
  Serial.printf("[FFT] Max frequency: %.2f Hz\n", max_frequency);

  // Adaptive sampling rate adjustment, write in g_sampling_frequency the new sampling frequency
  adjust_sampling_rate(max_frequency);
  Serial.printf("[FFT] Adjusted sampling rate: %d Hz\n", g_sampling_frequency);

  Serial.println("[FFT] Sampling task complete");
  vTaskDelete(NULL);
}

void setup() {
  // Serial communication initialization
  Serial.begin(SERIAL_BAUD);
  while (!Serial);  // Wait for serial connection
  Serial.println("\n[SYSTEM] FFT Analysis System Initialized");

  BaseType_t task_status = xTaskCreate(
    optimal_sampling_freq,   // Task function
    "OPT_Sampling",          // Task name
    TASK_STACK_SIZE,         // Stack size
    NULL,                    // Parameters
    TASK_PRIORITY,           // Priority
    &optimal_sampling_freq_task_handle    // Task handle
  );

  // Validate task creation
  if (task_status != pdPASS) {
    Serial.println("[ERROR] Failed to create sampling task!");
    while (1);
  }
}

void loop() {
 vTaskDelete(NULL);
}