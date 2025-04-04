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
   float duration = finish_time - start_time;
   float total_bytes = 2*SIZE_AVG_ARRAY * sizeof(char)* MSG_BUFFER_SIZE;
   float throughput = total_bytes / duration;

    Serial.println("\n--- Communication Metrics ---");
    Serial.printf("  Samples collected: %d\n", SIZE_AVG_ARRAY);
    Serial.printf("       Start time ms: %.2f\n", start_time);
    Serial.printf("      Finish time ms: %.2f\n", finish_time);
    Serial.printf("  Communication rate: %.4f bytes/ms\n", throughput);
    Serial.println("-----------------------------");
}

/* WiFi Management ---------------------------------------------------------- */
/**
 * @brief Initializes and manages WiFi connection
 * @note Implements retry logic with status monitoring
 */
void wifi_init(){
  Serial.printf("\n[WiFi] Connecting to %s\n", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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

        xTaskCreate(connect_mqtt, "task_mqtt", 8192, NULL, 0, NULL);
        vTaskDelete(NULL); 
        break;

      default:
        printf("[WiFi] WiFi Status: %d\n", WiFi.status());
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

  printf("\n[MQTT] Connecting to %s\n", MQTT_SERVER);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  while (!client.connect(clientId)) {
    printf(".");
    vTaskDelay(RETRY_DELAY);
  }

  if (!client.connected()) {
    printf("[MQTT] Timeout\n");
    vTaskDelete(NULL);     
  }

  printf("[MQTT] Connected\n");

  printf("[MQTT] subscribe to topic: %s\n", SUBSCRIBE_TOPIC);
  client.subscribe(SUBSCRIBE_TOPIC);  
  xTaskCreate(communication_mqtt_task, "task_publish", 4096, NULL, 0, NULL);
  
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
  if (String(topic) == SUBSCRIBE_TOPIC) {
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

      printf("[MQTT] incoming topic = id: %d - avg: %f - timestamp %lu \n",id,val,timestamp);
      float current_time = millis();

      float rtt = (float)( current_time - timestamp); // RTT in seconds

      Serial.printf("RTT: %.1f ms\n", rtt);
      
      rtt_data_array[id] = {id,val,rtt};
      
      if(id >= SIZE_AVG_ARRAY-1){
        print_rtts();
        end_time_comunication();
        print_volume_of_communication();
      } 
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
}

/* Data Reporting ----------------------------------------------------------- */
/**
 * @brief Prints all collected RTT measurements
 */
void print_rtts(){
   Serial.println("\n--- RTT Values ---");
  for (int i = 0; i < SIZE_AVG_ARRAY; i++) {
    Serial.printf("ID: %d | RTT: %.1f ms\n", 
                 rtt_data_array[i].id, 
                 rtt_data_array[i].rtt);
  }
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
      if(xQueueReceive(xQueueAvgs, &val, (TickType_t)5)) {
          send_to_mqtt(val,i);
          i++;
          if(i >= SIZE_AVG_ARRAY){
              Serial.print("*************\n");
              Serial.print("Communication task finished\n");
              Serial.print("*************\n");
              break;
          }
      }
    }
  vTaskDelete(NULL); 
}