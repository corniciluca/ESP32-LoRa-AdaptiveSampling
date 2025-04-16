#include <cmath>
#include "fft_analysis_minimal.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"

#define NOISE_THRESHOLD  8

/// @brief Real component buffer for FFT input
float g_samples_real[NUM_SAMPLES] = {0};

/// @brief Imaginary component buffer for FFT input
float g_samples_imag[NUM_SAMPLES] = {0};

/// @brief Current system sampling frequency (Hz)
int g_sampling_frequency = INIT_SAMPLE_RATE;

/* Signal Generation ------------------------------------------------------- */
/**
 * @brief Generate signal containing 3Hz and 5Hz sine wave components
 * @param t Time value in seconds
 * @return Signal value at time t
 */
float signal_low_freq(float t) {
  return 2*sin(2*PI*3*t) + 4*sin(2*PI*5*t);
}

float signal_changed(float t) {
  return 10*sin(2*PI*2*t) + 6*sin(2*PI*9*t);
}

float signal_medium_freq(float t) {
  return 8 * sin(2 * PI * 100 * t) + 3 * sin(2 * PI * 150 * t);
}

float signal_high_freq(float t) {
  return 4 * sin(2 * PI * 350 * t) + 2 * sin(2 * PI * 300 * t);
}


/* Sampling Functions ------------------------------------------------------ */
/**
 * @brief Sample a signal function at specified rate
 * @param sig_func Signal generation function pointer
 * @param index Sample index
 * @param sample_rate Sampling frequency in Hz
 * @return Sampled value at time t = index/sample_rate
 */
float sample_signal(signal_function sig_func, int index, int sample_rate) {
    float t = (float)index / sample_rate;
    return sig_func(t);
}

/**
 * @brief Perform signal acquisition for FFT processing
 * @param sig_func Signal generation function pointer
 * @param num_samples Number of samples to acquire
 */
void fft_process_signal(signal_function sig_func,int num_samples) {
    for( int i= 0; i< num_samples;i++){
        g_samples_real[i] = 0;
        g_samples_imag[i] = 0;
    }
    for (int i = 0; i < num_samples; i++) {
        g_samples_real[i] = sample_signal(sig_func, i, g_sampling_frequency);
        //Serial.printf("[FFT] %.2f \n",g_samples_real[i]);
        uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
        esp_sleep_enable_timer_wakeup(1000*1000*1/g_sampling_frequency);
        esp_light_sleep_start();
    }
}

/* FFT Processing Core ----------------------------------------------------- */
/**
 * @brief Execute complete FFT processing chain
 * @details Performs:
 * 1. Hamming window application
 * 2. Forward FFT computation
 * 3. Complex-to-magnitude conversion
 * 4. Calc max frequency
 * @note Results stored in module buffers
 */
float fft_perform_analysis(void) {
    ArduinoFFT<float> FFT = ArduinoFFT<float>(
    g_samples_real, 
    g_samples_imag, 
    NUM_SAMPLES, 
    g_sampling_frequency);
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();
    return fft_get_max_frequency();
}

/**
 * @brief Identify max frequency component
 * @return Frequency (Hz) of highest frequency
 * @pre Requires prior call to fft_perform_analysis()
 */
float fft_get_max_frequency(void) {
  double maxFrequency = -1;

  // Loop through all bins (skip DC at i=0)
  for (uint16_t i = 1; i < (NUM_SAMPLES >> 1); i++) {
    // Check if the current bin is a local maximum and above the noise floor
    if (g_samples_real[i] > g_samples_real[i-1] && g_samples_real[i] > g_samples_real[i+1] && g_samples_real[i] > NOISE_THRESHOLD) {
      double currentFreq = (i * g_sampling_frequency) / NUM_SAMPLES;
      // Update maxFrequency if this peak has a higher frequency
      if (currentFreq > maxFrequency) {
        maxFrequency = currentFreq;
      }
    }
  }
  return maxFrequency;
}

/* System Configuration ---------------------------------------------------- */
/**
 * @brief Adapt sampling rate based on Nyquist-Shannon criteria
 * @param max_freq Max detected frequency component
 * @note Implements safety factor of 2.5× maximum frequency
 */
void fft_adjust_sampling_rate(float max_freq) {
  int new_rate = (int)(NYQUIST_MULTIPLIER * max_freq);
  g_sampling_frequency = (g_sampling_frequency > new_rate) ? new_rate : g_sampling_frequency;
}
