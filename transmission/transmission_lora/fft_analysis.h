#pragma once
#include <arduinoFFT.h>
#include "config.h"

// Mathematical constants
#define PI 3.14159265358979323846f
#define NYQUIST_MULTIPLIER 2.5f

// FFT configuration
extern float g_samples_real[NUM_SAMPLES];
extern float g_samples_imag[NUM_SAMPLES];
extern int g_sampling_frequency;
extern ArduinoFFT<float> FFT;

// Signal type
typedef float (*signal_function)(float t);
extern signal_function curr_signal;
// Public API
float signal_1(float t);
float signal_medium_freq(float t);
float signal_low_freq(float t);
float signal_high_freq(float t);
float signal_1_changed(float t);
float sample_signal(signal_function sig_func, int index, int sample_rate);
void fft_init(void);
void fft_process_signal(signal_function sig_func, int num_samples);
float fft_get_max_frequency(void);
void fft_perform_analysis(void);
void fft_adjust_sampling_rate(float max_freq);
void fft_sampling_task(void *pvParameters);