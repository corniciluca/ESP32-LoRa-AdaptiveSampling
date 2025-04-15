#include <Arduino.h>
#include "config.h"
// Configuration Constants
#define TASK_STACK_SIZE  4096       // Stack size for sampling task
#define TASK_PRIORITY    2       // Medium priority for sampling task
#define SERIAL_BAUD      115200
#define configTICK_RATE_HZ 240000000

// Signal type
typedef float (*signal_function)(float t);

/* Signal Generation ------------------------------------------------------- */
float signal_1(float t) {
  return 2*sin(2*PI*3*t) + 4*sin(2*PI*5*t);
}

/* Sampling Functions ------------------------------------------------------ */
float sample_signal(signal_function sig_func, int index, int sample_rate) {
    float t = (float)index / sample_rate;
    return sig_func(t);
}

float find_max_sampling_freq(){
  long start = millis();
  long finish = 0;
  float g_samples_real[NUM_SAMPLES] = {0};
  float g_samples_imag[NUM_SAMPLES] = {0};
  for(int i=0; i < NUM_SAMPLES;i++){
    g_samples_real[i] = signal_1(i);
    vTaskDelay(pdMS_TO_TICKS(portTICK_PERIOD_MS));
  }
  finish = millis();
  return (float) NUM_SAMPLES/((finish - start)/1000.0);
}

void max_sampling_freq(void *pvParameters) {
  Serial.println("#############");
  Serial.printf("Max frequency found: %.2fHz \n",find_max_sampling_freq());
  Serial.println("#############");
  vTaskDelete(NULL);
}

void setup() {
  // Serial communication initialization
  Serial.begin(SERIAL_BAUD);
  while (!Serial);  // Wait for serial connection
  Serial.println("\n[SYSTEM] FFT Analysis System Initialized");
  BaseType_t task_status = xTaskCreate(
    max_sampling_freq,   // Task function
    "max_Sampling",          // Task name
    TASK_STACK_SIZE,         // Stack size
    NULL,                    // Parameters
    TASK_PRIORITY,           // Priority
    NULL
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