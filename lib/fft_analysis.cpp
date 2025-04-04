#include "fft_analysis.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "shared_defs.h"

/// @brief Real component buffer for FFT input
float g_samples_real[NUM_SAMPLES] = {0};

/// @brief Imaginary component buffer for FFT input
float g_samples_imag[NUM_SAMPLES] = {0};

/// @brief Current system sampling frequency (Hz)
int g_sampling_frequency = INIT_SAMPLE_RATE;

/// @brief ArduionFFT instance configured with buffers
ArduinoFFT<float> FFT = ArduinoFFT<float>(
    g_samples_real, 
    g_samples_imag, 
    NUM_SAMPLES, 
    g_sampling_frequency
);

/* Signal Generation ------------------------------------------------------- */
/**
 * @brief Generate signal containing 3Hz and 5Hz sine wave components
 * @param t Time value in seconds
 * @return Signal value at time t
 */
float signal_1(float t) {
  return 2*sin(2*PI*3*t) + 4*sin(2*PI*5*t);
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
    const float t = (float)index / sample_rate;
    return sig_func(t);
}

/**
 * @brief Perform signal acquisition for FFT processing
 * @param sig_func Signal generation function pointer
 * @param num_samples Number of samples to acquire
 */
void fft_process_signal(signal_function sig_func,int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        g_samples_real[i] = sample_signal(sig_func, i, g_sampling_frequency);
        vTaskDelay(pdMS_TO_TICKS(1000/g_sampling_frequency));
    }
}

/* FFT Processing Core ----------------------------------------------------- */
/**
 * @brief Execute complete FFT processing chain
 * @details Performs:
 * 1. Hamming window application
 * 2. Forward FFT computation
 * 3. Complex-to-magnitude conversion
 * @note Results stored in module buffers
 */
void fft_perform_analysis(void) {
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();
}

/**
 * @brief Identify max frequency component
 * @return Frequency (Hz) of highest frequency
 * @pre Requires prior call to fft_perform_analysis()
 */
float fft_get_max_frequency(void) {
    return FFT.majorPeak();
}

/* System Configuration ---------------------------------------------------- */
/**
 * @brief Adapt sampling rate based on Nyquist-Shannon criteria
 * @param max_freq Max detected frequency component
 * @note Implements safety factor of 2.5× maximum frequency
 */
void fft_adjust_sampling_rate(float max_freq) {
    const int new_rate = (int)(NYQUIST_MULTIPLIER * max_freq);
    g_sampling_frequency = (g_sampling_frequency > new_rate) ? new_rate : g_sampling_frequency;
}

/**
 * @brief Initialize FFT processing module
 * @details Performs:
 * 1. Initial signal acquisition
 * 2. Frequency analysis
 * 3. Adaptive rate configuration
 * @note Must be called before starting sampling tasks
 */
void fft_init(void) {
    Serial.println("[FFT] Initializing FFT module");
    
    // Initial analysis with default signal
    fft_process_signal(signal_1,NUM_SAMPLES);
    fft_perform_analysis();
    
    // Adaptive rate adjustment
    const float peak_freq = fft_get_max_frequency();
    Serial.printf("[FFT] Peak frequency: %.2f Hz\n", peak_freq);

    fft_adjust_sampling_rate(peak_freq);
    Serial.printf("[FFT] Optimal sampling rate: %d Hz\n", g_sampling_frequency);
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
void fft_sampling_task(void *pvParameters) {
    float sample = 0.0f;
    
    Serial.printf("[FFT] Starting sampling at %d Hz\n", g_sampling_frequency);
    Serial.println("--------------------------------");

    for (int i = 0; i < NUM_OF_SAMPLES_AGGREGATE; i++) {
        sample = sample_signal(signal_1, i, g_sampling_frequency);
        
        if (xQueueSend(xQueueSamples, &sample, portMAX_DELAY) != pdPASS) {
            Serial.println("[ERROR] Failed to send sample to queue");
            continue;
        }

        Serial.printf("[FFT] Sample %d: %.2f\n", i, sample);
        vTaskDelay(pdMS_TO_TICKS(1000/g_sampling_frequency));
    }

    Serial.println("--------------------------------");
    Serial.println("[FFT] Sampling completed");
    vTaskDelete(NULL);
}