#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include "communication.h"
#include <PubSubClient.h>
#include "shared_defs.h"
#include "secrets.h"
#include "fft_analysis.h"
#include <ArduinoJson.h>
#include "config.h"
#include "driver/uart.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <esp_pm.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
// Network Configuration
#define MSG_BUFFER_SIZE 50  // Maximum size for MQTT messages
#define SIZE_AVG_ARRAY NUM_OF_SAMPLES_AGGREGATE-WINDOW_SIZE+1

/* Global Variables --------------------------------------------------------- */
float start_time = 0.0;      // Timestamp when communication starts (ms)
float finish_time = 0.0;     // Timestamp when communication ends (ms)

WiFiClient espClient;        // WiFi client instance
PubSubClient client(espClient);  // MQTT client instance

char msg[MSG_BUFFER_SIZE];   // Buffer for MQTT messages

// Round-Trip Time (RTT) measurement storage
struct rtt_data rtt_data_array[SIZE_AVG_ARRAY];

/* Timing Functions --------------------------------------------------------- */
/**
 * @brief Records start timestamp for communication metrics
 */
void start_time_communication(){
    start_time = millis();
}

/**
 * @brief Records end timestamp for communication metrics
 */
void end_time_comunication(){
    finish_time = millis();
}

/**
 * @brief Calculates and prints communication performance metrics
 */
void print_volume_of_communication(){
    float duration_ms = finish_time - start_time;
    float duration_sec = duration_ms / 1000;
    float total_bytes = 2*SIZE_AVG_ARRAY * sizeof(char)* MSG_BUFFER_SIZE;

    float throughput_bps = total_bytes / duration_sec;

    Serial.println("\n--- Communication Metrics ---");
    Serial.printf("  Averages collected: %d\n", SIZE_AVG_ARRAY);
    Serial.printf("       Start time (ms): %.2f\n", start_time);
    Serial.printf("      Finish time (ms): %.2f\n", finish_time);
    Serial.printf("  Duration (ms): %.2f\n", duration_ms);
    Serial.println("-----------------------------");
    Serial.println("Data Volume:");
    Serial.printf("  Bytes Sent: %.2f\n", total_bytes/2);
    Serial.printf("  Bytes Received: %.2f\n", total_bytes/2);
    Serial.printf("  Total Volume: %.2f bytes\n", total_bytes);
    Serial.println("-----------------------------");
    Serial.println("Throughput:");
    Serial.printf("  %.2f bytes/sec\n", throughput_bps);
    Serial.printf("  %.4f bytes/ms\n", throughput_bps / 1000.0);
    Serial.println("-----------------------------");

    // uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
    // esp_sleep_enable_timer_wakeup(1000*1000*10);
    // esp_light_sleep_start();
}

/* WiFi Management ---------------------------------------------------------- */
/**
 * @brief Initializes and manages WiFi connection
 * @note Implements retry logic with status monitoring
 */
void wifi_init(){
  Serial.printf("\n[WiFi] Connecting to %s\n", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //WiFi.setSleep(false);
  int numberOfTries = WIFI_MAX_RETRIES;

  while (true) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL: 
        Serial.printf("[WiFi] SSID not found\n"); 
        break;

      case WL_CONNECT_FAILED:
        Serial.printf("[WiFi] Failed - WiFi not connected! \n");
        vTaskDelete(NULL); 
        break;

      case WL_CONNECTION_LOST: 
        Serial.printf("[WiFi] Connection was lost\n"); 
        break;

      case WL_SCAN_COMPLETED:  
        Serial.printf("[WiFi] Scan is completed\n"); 
        break;

      case WL_DISCONNECTED:    
        Serial.printf("[WiFi] WiFi is disconnected\n"); 
        break;

      case WL_CONNECTED:
        Serial.printf("[WiFi] WiFi is connected!\n");
        gpio_deep_sleep_hold_en(); // Retain GPIO state
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        // uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
        // esp_sleep_enable_timer_wakeup(1000*1000*1);
        // esp_light_sleep_start();
        xTaskCreate(connect_mqtt, "task_mqtt", 4096, NULL, 1, NULL);
        return;

      default:
        Serial.printf("[WiFi] WiFi Status: %d\n", WiFi.status());
        break;
    }
    vTaskDelay(RETRY_DELAY);

    if (numberOfTries <= 0) {
      Serial.printf("[WiFi] Max retries exceeded\n");
      WiFi.disconnect();
      vTaskDelete(NULL); 
    } else {
      numberOfTries--;
    }
  }
}

/* MQTT Functions ----------------------------------------------------------- */
/**
 * @brief Establishes and maintains MQTT connection
 * @param pvParameters FreeRTOS task parameters (unused)
 */
void connect_mqtt(void *pvParameters) {
  char clientId[50];
  long r = random(1000);
  sprintf(clientId, "clientId-%ld", r);

  Serial.printf("\n[MQTT] Connecting to %s\n", MQTT_SERVER);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setSocketTimeout(60);

  while (!client.connect(clientId)) {
    Serial.printf(".");
    vTaskDelay(RETRY_DELAY);
  }
  // uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
  // esp_sleep_enable_timer_wakeup(1000*1000*1);
  // esp_light_sleep_start();

  if (!client.connected()) {
    Serial.printf("[MQTT] Timeout\n");
    vTaskDelete(NULL);     
  }

  Serial.printf("[MQTT] Connected\n");

  Serial.printf("[MQTT] subscribe to topic: %s\n", SUBSCRIBE_TOPIC);
  client.subscribe(SUBSCRIBE_TOPIC,1);
  // uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
  // esp_sleep_enable_timer_wakeup(1000*1000*0.5);
  // esp_light_sleep_start();
  xTaskNotifyGive(xCommunicationTaskHandle);
  xTaskCreate(communication_mqtt_task, "task_publish", 4096, NULL, 1, NULL);
  
  // Main MQTT maintenance loop
  while (1) {
    client.loop();
    vTaskDelay(MQTT_LOOP);
  }

  vTaskDelete(NULL); 
}

/**
 * @brief Handles incoming MQTT messages
 * @param topic Message topic
 * @param message Message payload
 * @param length Message length
 */
void callback(char* topic, byte* message, unsigned int length) {
  //     uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
  // esp_sleep_enable_timer_wakeup(1000*1000*0.5);
  // esp_light_sleep_start();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        vTaskDelete(NULL);
    }
    
    int id = doc["id"];
    float val = doc["value"];
    unsigned long timestamp = doc["time"];

    Serial.printf("[MQTT] incoming topic = id: %d - avg: %f - timestamp %lu \n",id,val,timestamp);
    float current_time = millis();

    float rtt = (float)( current_time - timestamp); // RTT in seconds

    Serial.printf("RTT: %.1f ms\n", rtt);
    
    rtt_data_array[id] = {id,val,rtt};
    
    if(id >= SIZE_AVG_ARRAY-1){
      // uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
      // esp_sleep_enable_timer_wakeup(1000*1000*1);
      // esp_light_sleep_start();
      print_rtts();
      end_time_comunication();
      print_volume_of_communication();
    } 
  }

/**
 * @brief Publishes data to MQTT broker
 * @param val Value to publish
 * @param i Sample index
 */
void send_to_mqtt(float val, int i){
    unsigned long timestamp = millis();
    snprintf(msg, MSG_BUFFER_SIZE, "{\"id\":%d,\"value\":%.2f,\"time\":%lu}",i,val, timestamp);    
    
    if(client.publish(PUBLISH_TOPIC, msg)){
      Serial.printf("[MQTT] Publishing average: %s\n", msg);
    }else{
      Serial.printf("[MQTT] ERROR while publishing average: %s\n", msg);
      if (!client.connected()) {
        vTaskDelete(NULL); 
      }
    }
  //   uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
  // esp_sleep_enable_timer_wakeup(1000*1000*0.5);
  // esp_light_sleep_start();
}

/* Data Reporting ----------------------------------------------------------- */
/**
 * @brief Prints all collected RTT measurements
 */
void print_rtts(){
   Serial.println("\n--- RTT Values ---");
    
    float sum = 0.0;
    int valid_samples = 0;

    for (int i = 0; i < SIZE_AVG_ARRAY; i++) {
        if (rtt_data_array[i].id >= 0) {
            Serial.printf("ID: %d | RTT: %.1f ms\n", 
                         rtt_data_array[i].id, 
                         rtt_data_array[i].rtt);
            sum += rtt_data_array[i].rtt;
            valid_samples++;
        }
    }
    float mean = sum / valid_samples;

    float sum_squared_diff = 0.0;
    for (int i = 0; i < SIZE_AVG_ARRAY; i++) {
        if (rtt_data_array[i].id >= 0) {
            float diff = rtt_data_array[i].rtt - mean;
            sum_squared_diff += diff * diff;
        }
    }
    float std_dev = sqrt(sum_squared_diff / valid_samples);

    Serial.println("------------------");
    Serial.printf("Averages: %d\n", valid_samples);
    Serial.printf("Mean RTT: %.2f ms\n", mean);
    Serial.printf("Std Dev: %.2f ms\n", std_dev);
    Serial.println("------------------");
}

/* Main Communication Task -------------------------------------------------- */
/**
 * @brief Handles outgoing MQTT communications
 * @param pvParameters FreeRTOS task parameters (unused)
 */
void communication_mqtt_task(void *pvParameters){
    float val = 0.0;
    int i = 0;
    start_time_communication();
    while(1){
      if(xQueueReceive(xQueueAvgs, &val, (TickType_t)portMAX_DELAY)) {
        send_to_mqtt(val,i);
        i++;
        if(i >= SIZE_AVG_ARRAY){
            Serial.print("*************\n");
            Serial.print("Communication task finished\n");
            Serial.print("*************\n");
            // uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
            // esp_sleep_enable_timer_wakeup(1000*1000*2);
            // esp_light_sleep_start();
            break;
        }
      }
    }
  vTaskDelete(NULL); 
}