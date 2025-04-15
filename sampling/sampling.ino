#pragma GCC optimize ("O0")

#include <Arduino.h>
#include "fft_analysis_minimal.h"
#include "config.h"
#include "driver/uart.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"

// Configuration Constants
#define TASK_STACK_SIZE     4096
#define TASK_PRIORITY       2
#define SERIAL_BAUD         115200

#define AMPLITUDE_THRESHOLD_STD_DEV 3.0f
#define MIN_SAMPLES_FOR_ANOMALY 10
#define SAMPLING_WINDOW_SIZE 10

// Task handles
TaskHandle_t optimal_sampling_freq_task_handle = NULL;

float sample_window[SAMPLING_WINDOW_SIZE];
int window_index = 0;
int sample_count = 0;

bool anomaly(float sample) {
    if (sample_count < MIN_SAMPLES_FOR_ANOMALY) {
        return false;
    }

    float mean = 0.0f;
    float variance = 0.0f;
    int size = min(sample_count, SAMPLING_WINDOW_SIZE);

    for (int i = 0; i < size; i++) {
        mean += sample_window[(window_index - size + i + SAMPLING_WINDOW_SIZE) % SAMPLING_WINDOW_SIZE];
    }
    mean /= size;

    for (int i = 0; i < size; i++) {
        variance += powf(sample_window[(window_index - size + i + SAMPLING_WINDOW_SIZE) % SAMPLING_WINDOW_SIZE] - mean, 2);
    }
    variance /= size;

    float std_dev = sqrtf(variance);
    float diff = fabsf(sample - mean);

    if(diff > (AMPLITUDE_THRESHOLD_STD_DEV * std_dev)){
      Serial.printf("Mean: %.2f - Variance: %2.f\n",mean,variance);
      return true;
    }
    return false;
}

void optimal_sampling_freq(signal_function signal) {
    Serial.println("[FFT] Signal sampling task started");

    g_sampling_frequency = INIT_SAMPLE_RATE;

    fft_process_signal(signal, NUM_SAMPLES);
    float max_frequency = fft_perform_analysis();
    
    Serial.printf("[FFT] Max frequency: %.2f Hz\n", max_frequency);
    fft_adjust_sampling_rate(max_frequency);
    Serial.printf("[FFT] Adjusted sampling rate: %d Hz\n", g_sampling_frequency);
    Serial.println("[FFT] Sampling task complete");

    uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
    esp_sleep_enable_timer_wakeup(1000*1000*2);
    esp_light_sleep_start();
}

void sampling_task(void *pvParameters) {
    float sample = 0.0f;
    signal_function signal = signal_high_freq;
    optimal_sampling_freq(signal);
    Serial.printf("[SAMPLING] Starting sampling at %d Hz\n", g_sampling_frequency);
    Serial.println("--------------------------------");
    while (1) {
        for (int i = 0; i < 200; i++) {
            if (i == 100)
                signal = signal_medium_freq;
            if (i == 150 )
                signal = signal_high_freq;

            sample = sample_signal(signal, i, g_sampling_frequency);
            Serial.printf("[SAMPLING] Sample %d: %.2f\n", i, sample);

            //vTaskDelay(pdMS_TO_TICKS(1000 / g_sampling_frequency));
        
            uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
            esp_sleep_enable_timer_wakeup(1000*1000*1/g_sampling_frequency);
            esp_light_sleep_start();
            
            if (anomaly(sample)) {
                Serial.printf("[ANOMALY] Anomaly detected: Amp: %.2f\n", sample);
                optimal_sampling_freq(signal);
                window_index = 0;
                sample_count = 0;
            }
            
            sample_window[window_index] = sample;
            window_index = (window_index + 1) % SAMPLING_WINDOW_SIZE;
            sample_count++;
        }
        Serial.println("--------------------------------");
        Serial.println("[SAMPLING] Sampling completed");
    }
    vTaskDelete(NULL);
}

void setup() {
  // Serial communication initialization
  Serial.begin(SERIAL_BAUD);
  while (!Serial);  // Wait for serial connection
  Serial.println("\n[SYSTEM] FFT Analysis System Initialized");

  BaseType_t task_status = xTaskCreate(
    sampling_task,   // Task function
    "sampling_task",          // Task name
    TASK_STACK_SIZE,         // Stack size
    NULL,                    // Parameters
    TASK_PRIORITY,           // Priority
    NULL    // Task handle
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