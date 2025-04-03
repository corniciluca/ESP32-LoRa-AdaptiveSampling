#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "LoRaWan_APP.h"
#include <fft_analysis.h>
#include <aggregate.h>
#include <shared_defs.h>

/* LoRaWAN Configuration ---------------------------------------------------- */
#define LORA_JSON_BUFFER_SIZE 255
#define LORA_DEVICE_ID "ESP32_LoRa"
#define APP_TX_DUTYCYCLE_RND 1000

// OTAA Parameters (Over-the-Air Activation)
uint8_t devEui[] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x06, 0xF8, 0xCD };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x6B, 0x0F, 0x9C, 0x72, 0xFE, 0x9E, 0x27, 0x8D, 0xD1, 0x3F, 0xAD, 0x58, 0x4D, 0x06, 0xD6, 0x02 };

// ABP Parameters (Activation by Personalization)
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda,0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef,0x67 };
uint32_t devAddr =  ( uint32_t )0x007e6ae1;

/* Network Settings --------------------------------------------------------- */
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 }; // LoRa channel mask
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;  // Regulatory region
DeviceClass_t  loraWanClass = CLASS_A;  // Device class

/* Operational Parameters --------------------------------------------------- */
uint32_t appTxDutyCycle = 15000;       // Transmission interval (ms)
bool overTheAirActivation = true;      // Activation mode selector
bool loraWanAdr = true;                // Adaptive Data Rate enabled
bool isTxConfirmed = true;             // Confirmed messages
uint8_t appPort = 2;                   // Application port
uint8_t confirmedNbTrials = 4;         // Transmission retries

/* Persistent State --------------------------------------------------------- */
// RTC-retained variables (survive deep sleep)
RTC_DATA_ATTR int sample_i = 0;        // Sample index counter
RTC_DATA_ATTR int freq = INIT_SAMPLE_RATE; // Current sampling frequency
RTC_DATA_ATTR bool initialized = false;// Initialization flag
RTC_DATA_ATTR int w_pos = 0;           // Circular buffer position
RTC_DATA_ATTR int w_count = 0;         // Valid samples counter
RTC_DATA_ATTR float sampleReadings[WINDOW_SIZE] = {0}; // Sample buffer

/* Runtime Variables -------------------------------------------------------- */
float avg = 0.0;                       // Current moving average
TaskHandle_t main_task_handler = NULL; // Main task reference
QueueHandle_t xQueueSample = NULL;     // Inter-task sample queue


/* Signal Processing -------------------------------------------------------- */
/**
 * @brief Initializes FFT analysis and configures sampling rate
 * @details
 * - Performs initial signal analysis
 * - Calculates optimal sampling frequency
 * - Stores configuration in RTC memory
 */
void fft_inizialization(){
  Serial.println("[FFT] Initializing FFT module");
    
  // Initial analysis with default signal
  fft_process_signal(signal_1,NUM_SAMPLES);
  fft_perform_analysis();

  // Adaptive rate adjustment
  float max_freq = fft_get_max_frequency();

  Serial.printf("[FFT] Peak frequency: %.2f Hz\n", max_freq);
  freq = (g_sampling_frequency > 2.5 * max_freq) ? 2.5 * max_freq : g_sampling_frequency;
  Serial.printf("[FFT] Optimal sampling rate: %d Hz\n", freq);
}

/* Data Acquisition Task ---------------------------------------------------- */
/**
 * @brief Samples signal at configured rate
 * @param args Task parameters (unused)
 * @operation
 * - Generates samples using current frequency
 * - Pushes samples to inter-task queue
 */
void sampling_task(void *args) {
  float sample = 0.0;
  bool do_sampling = true;
  Serial.print("[SAMPLING] Starting to sampling at frequency: ");
  Serial.println(freq);
  Serial.println("**********************");
  vTaskDelay(pdMS_TO_TICKS(50));
  //if(do_sampling){
   // do_sampling = false;
    sample = sample_signal(signal_1, sample_i,freq);
    sample_i++;
    Serial.print("[SAMPLING] Sample: ");
    Serial.println(sample);
    xQueueSend(xQueueSample, &sample, (TickType_t)0);
    vTaskDelay(pdMS_TO_TICKS(1000/freq));
  //}
  vTaskDelete(NULL);
}

/* Data Processing Task ----------------------------------------------------- */
/**
 * @brief Calculates moving average of samples
 * @param args Task parameters (unused)
 * @implements
 * - Circular buffer management
 * - Windowed average calculation
 * - Result storage in global 'avg' variable
 */
void average_task(void *args) {
  float sum = 0;
  float value;
  bool calc_avg = true;
  Serial.println("**********aggregate**********");
  vTaskDelay(pdMS_TO_TICKS(50));
  if (xQueueReceive(xQueueSample, &(value), (TickType_t) 5)) {
   // calc_avg = false;
    sampleReadings[w_pos] = value;
    Serial.print("[AGGREGATE] Sample read: ");
    Serial.println(value);
    w_pos = (w_pos + 1) % WINDOW_SIZE; // Circular buffer index
    if (w_count < WINDOW_SIZE) w_count++; // Ensure we don't exceed the array size
    sum = 0; // Reset sum for next calculation
    for (int i = 0; i < w_count; i++) 
        sum += sampleReadings[i];
    avg = sum / WINDOW_SIZE;
    Serial.print("[AGGREGATE] Average calculated: ");
    Serial.println(avg);
  }
  vTaskDelete(NULL); 
}

/* LoRaWAN Interface -------------------------------------------------------- */
/**
 * @brief Prepares LoRaWAN transmission frame
 * @param port Application port number
 * @details
 * - Packages current average value
 * - Configures payload size
 */
static void prepareTxFrame(uint8_t port){    
    appDataSize = sizeof(float);
    memcpy(appData, &avg, sizeof(float));
}

/* System Initialization ---------------------------------------------------- */
void setup() {
  Serial.begin(115200);
  if(sample_i >= NUM_OF_SAMPLES_AGGREGATE){
    Serial.println("---- Stop ----");
    vTaskDelete(NULL);
  }
  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
  xQueueSample = xQueueCreate(1, sizeof(float));
  Serial.println("Ready!");
  
  if(!initialized){
    fft_inizialization();
    initialized = true;
  }

  xTaskCreate(
      sampling_task,
      "SamplingTask",
      2048,
      NULL,
      3,
      NULL
  );

  xTaskCreate(
    average_task,
    "AverageTask",
    2048,
    NULL,
    2,
    NULL
  );

}

/* -------------------------
 * LoRaWAN State Machine
 * --------------------------
 * States:
 *   INIT: Stack configuration
 *   JOIN: Network authentication
 *   SEND: Data transmission
 *   CYCLE: Duty cycle management
 *   SLEEP: Low-power mode
 
 * Note:
 *   - Blocking while(1) required for FreeRTOS compatibility
 *   - Sleep duration controlled by LoRaWAN class
 */
void loop() {
  main_task_handler = xTaskGetCurrentTaskHandle();
  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
  while(1){
    switch( deviceState )
    {
      case DEVICE_STATE_INIT:
      {
        #if(LORAWAN_DEVEUI_AUTO)
          LoRaWAN.generateDeveuiByChipID();
        #endif
        LoRaWAN.init(loraWanClass,loraWanRegion);
        LoRaWAN.setDefaultDR(3);
        break;
      }
      case DEVICE_STATE_JOIN:
      {
        LoRaWAN.join();
        break;
      }
      case DEVICE_STATE_SEND:
      {
        vTaskDelay(pdMS_TO_TICKS(100));
        prepareTxFrame(appPort);
        LoRaWAN.send();
        deviceState = DEVICE_STATE_CYCLE;

        break;
      }
      case DEVICE_STATE_CYCLE:
      {
        txDutyCycleTime = appTxDutyCycle + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
        LoRaWAN.cycle(txDutyCycleTime);
        deviceState = DEVICE_STATE_SLEEP;
        break;
      }
      case DEVICE_STATE_SLEEP:
      {
        LoRaWAN.sleep(loraWanClass);
        break;
      }
      default:
      {
        deviceState = DEVICE_STATE_INIT;
        break;
      }
    }
  }
}
