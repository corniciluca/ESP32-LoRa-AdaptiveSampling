# ESP32-LoRa-AdaptiveSampling  
**ESP32-based IoT system leveraging FreeRTOS to implement intelligent sensor sampling. The solution dynamically optimizes sampling rates using Nyquist-compliant FFT analysis, combined with efficient edge/cloud communication through hybrid LoRaWAN, MQTT and WIFI protocols.**  

---

## 📌 Overview  
ESP32-based IoT projects concerning:  

1. **Maximum Sampling Frequency Identification :** demonstrates the ESP32's capability to achieve high-frequency sampling.

2. **Optimal Sampling Frequency Adaptation :** dynamically adjusts sampling rates using FFT analysis, adhering to Nyquist principles with a safety margin (e.g., sampling at 2.5× the detected maximum frequency).
For instance:
      * Detects peak frequency 𝑓 = 5Hz → sets sampling rate to 12.5Hz (2.5×). 

3. **Computes local aggregates :** calculates aggregates of the sampled signal over a windows (e.g., data-driven sliding window).  
4. **Transmits data** via:  
   - **WiFi/MQTT :** transmits aggregate values to a nearby edge server in real-time using the lightweight MQTT protocol over WiFi, enabling low-latency monitoring and rapid response.  
   - **LoRaWAN + TTN + MQTT :** sends aggregate values to a edge server via LoRaWAN and The Things Network (TTN), leveraging TTN’s MQTT integration.  
---
## Detailed Phase Breakdown

### Phase 1: Determine Maximum Sampling Frequency of Heltec ESP32 WIFI LoRa V3

In this phase, we determine the maximum sampling frequency at which our type of ESP32 can operate. This frequency is greatly influenced by how we acquire the signal that as to be sampled. For instance, if we capure the signal using a UART, the maximum sampling frequency will be limited by the baud rate of the UART connection, .In our case, the signal is generate locally in the firmware and internally sampled, this means that the maximum sampling frequency is limited by the minimum delay between two sample. Which it's bounded by the period of freeRTOS's ticks, defined by the OS as
```
portTICK_PERIOD_MS = 1 / configTICK_RATE_HZ = 1 / 1000 = 1 ms
```
So we the maximum theoretical frequency is 1000Hz, specially if we assume that: 

* The sampling task has the highest priority.
* The sampling code itself adds negligible runtime overhead.

In the code the maximum theoretical frequency is used to determine the optimal one. Sampling that higher frequencies will produce unresonable value of the samples.

**Code Reference**: [sampling.ino](/sampling/sampling.ino)

---

### Phase 2: Compute Optimal sampling frequency

Choosing between higher or lower sampling frequencies involves critical engineering trade-offs:
**Trade offs**
- **Higher Sampling Rates** :
  
     - **Advantages**: Better signal reconstruction, reduced ISI.
       
     - **Drawbacks**: Increased CPU load, memory usage, and power consumption.
- **Lower Sampling Rates** :
  
     - **Advantages**: Resource-efficient.
       
     - **Drawbacks**: Risk of aliasing, ISI, and loss of high-frequency details.

To resolve these trade-offs, this phase focuses on computing the **optimal sampling frequency** that balances efficiency and signal integrity. The so called frequency is obtained levegering, the highest frequency of the signal and the Shannon-Nyquist theorem, to determine an efficient sampling rate capable of reconstructing the signal without aliasing or Inter-Symbol Interference (ISI). Specifically, the sampling theorem states that it's possible to reconstruct a continue signal by performing a sampling with a frequency higher that 2 * $\f_max$, eventhough a common engineering practice is to pick $\f_sample$ = 2.5 * $\f_max$ for safety margins.

**Program steps**
1. **Sample at maximum frequency possible for the ESP32**:
          Given the maximum possible sample rate we sample the signal for a total of 1024 samples. This has the goal to correctly careaterize the input signal.
2. **Compute FFT and apply window**:
        Calculate the frequency domain signal by apply a optimaze algoritm (Fast Fourier Trasform) that compute the Fourier Trasfomant of the signal. Then applies a Hamming window function to reduce the spectral leakage of the signal.
4. **Compute the major peak of the signal**:
     Assuming that the major peak correspond to the max frequency of the signal, as it is in this case, it's possible to determin it using the MajorPeak() method of the FFTArduino library. Generally this is not the case since the frequenzy componenet with the highest magnitude doesen't always correspond to the highest frequency(e.g. `20*sin(2π*3*t) + 4*sin(2π*5*t)`).
     
     ![FFT2-colab](https://github.com/user-attachments/assets/e171f215-6c02-46e4-aa84-b7a7f796dab2)
     
     So usually a more secure approch would be this one:

     ```
     float get_max_freq(){
       float noiseFloor = 0.1;
       float lastValidFreq = 0;
       
       for (int i = 0; i < NUM_SAMPLES / 2; i++) {
         if (g_samples_real[i] > noiseFloor) {
           lastValidFreq = i;
         }
       }
       return lastValidFreq * (g_sampling_frequency / (float)NUM_SAMPLES);
     }
     ```
     In this case it's important to adjust the correct noise floor level.
   
5. **Determine the optimal sampling frequency** To do so simply multy the obtained value by 2.5.

**Practical example**
- **Input**: `2*sin(2π*3*t) + 4*sin(2π*5*t)`
- **Initial sampling frequency**: 1KHz
- **Results:**

![OptSampl](https://github.com/user-attachments/assets/fbd8a22a-328e-493b-8013-8758ab61b851)

     
The experimental results are corrected, indeed the input signal is a sum of two sinusoid with frequency 3Hz and 5Hz. This can also be view simply by doing the Fourier traform of the function:

![FFT-colab](https://github.com/user-attachments/assets/f895f12e-f4c3-4fad-beab-c2f712c5756a)


**Code Reference**: [sampling.ino](/sampling/sampling.ino)
 
### Phase 3: Compute aggregates values

In this phase, we aggregate the samples of the signal by computing an average of the last 5 samples so an rolling average over a 5-samples window. The implementation is done using tools given by **FreeRTOS** with the _goal_ of **parallelism** and so **efficency**. 

**Implmentation:**

- **Task_A:** This task will sample the signal using the optimal frequency and each sample will be added to **xQueue_samples**, a mechanism used for inter-task communication that allows tasks to send and receive data in a thread-safe manner, ensuring synchronization between tasks.
- **Task_B:** This task will read the samples from **xQueue_samples** and compute the rolling average. To do it uses a circular buffer of size 5, that each time recive a new sample it will compute the respective average.

**Results**
  
  ![aggregate_results](https://github.com/user-attachments/assets/8a2ad38d-df65-405e-b05e-cca8d6fc8401)

**Code Reference**: [aggregate.ino](/aggregate/aggregate.ino)

### Phase 4: MQTT Transmission to an Edge Server over WiFi

In this phase we comunicate with an edge server thorugh the **MQTT** a lightweight publish-subscribe messaging protocol designed for resource-constrained IoT devices and low-bandwidth networks. The goal is to securely and efficiently transmit the **rolling average** windows from the device to the edge server over a WiFi connection.

**Key Components for MQTT**
- **MQTT Broker:** A centralized server (e.g., Mosquitto or cloud-based brokers like AWS IoT Core) that manages message routing between publishers (devices) and subscribers (edge servers).
- **Publisher:** The IoT device publishing sensor data (e.g., average) to a specific MQTT topic (e.g., `esp32/data`)
- **Subscriber:** The edge server subscribing to relevant topics to receive and process incoming data. In this case the edge server will also reply on the topic `esp32/data/acks`. 

**Results**
- **Heltec ESP32:**

  ![transimission_mqtt_results](https://github.com/user-attachments/assets/81cab811-99a4-428d-8345-20a54ea1df2f)
   
- **Edge server:**
  
  ![transimission_mqtt_edge_server_results](https://github.com/user-attachments/assets/7b8930fc-8173-49ca-9059-572931b121d3)

**Code Reference**: [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)

#### Phase 5: LoRaWAN Uplink to The Things Network (TTN) and MQTT Transmission
In this phase, after computed the **rolling average**, instead of sending it to an edge server via **MQTT** + **WIFI**, we will send it to the edge server via **LoRaWAN** + **MQTT**. Therefore, the device will trasmit the values computed in Phase 3 over **LoRaWAN** to **The Things Network (TTN)** and they will be recived by the edge server thanks to **MQTT** protocol.

**General approach**

The LoRa Transmission will use **OTAA (Over-The-Air Activation)** for device authentication and network joining. OTAA is a more secure and flexible method compared to **ABP (Activation By Personalization)**  allows devices to rejoin the network after a reset or power cycle. The **payload** is composed by a single **float** value, thata reppresent the average computed by the **ESP32** device. The float occupy **4 bytes**, also the esp32 uses the **little-edian** representation of floats, this has to be taken into account when receving the payload. Before trying to establish a connection is fundamental to:
* Set the **LoRaWAN** region(e.g. REGION_EU868)
* Store **DevEUI** - A unique device identifier (like a MAC address). 
* Store **AppEUI** - Identifies the application/provider (similar to a network ID). 
* Store **AppKey** - A root key used for secure session key generation.
* Load the **payload**
* Specify the **appPort**

**Details of implementation**

In this project we have used **LoRaWAN Class A** which is the default and most **energy-efficient** LoRaWAN device class, optimized for battery-powered IoT devices. This class has an extremely **low power** consumption due to the fact between the trasmitting of uplink messages it will **sleep**. It's important to notice that once the esp32 wakes up from the sleep state, it will be **restared** and the values stored in the normal variables will be lost. To prevent this from appening, it's possible to save some data in **RTC registers**, registers that wont be wiped once the esp32 wakes up for the sleep state.
Using the key word **RTC_DATA_ATTR** will create a variable inside the **RTC** registers

* Example:
  
  ```
     /* Persistent State --------------------------------------------------------- */
     // RTC-retained variables (survive deep sleep)
     RTC_DATA_ATTR int sample_i = 0;        // Sample index counter
     RTC_DATA_ATTR int freq = INIT_SAMPLE_RATE; // Current sampling frequency
     RTC_DATA_ATTR bool initialized = false;// Initialization flag
     RTC_DATA_ATTR int w_pos = 0;           // Circular buffer position
     RTC_DATA_ATTR int w_count = 0;         // Valid samples counter
     RTC_DATA_ATTR float sampleReadings[WINDOW_SIZE] = {0}; // Sample buffer
     
  ```

**The Things Network (TTN)**

The Things Network (TTN) is an open, community-driven, and decentralized LoRaWAN network infrastructure that enables low-power, long-range IoT communications globally. To be able to use TTN follow i followed these steps:

1.**Register on TTN**

- Go to [TTN Console](https://console.thethingsnetwork.org/), select your region, and **create an account**.  
- Create an **Application**, then **Register a Device** to obtain your LoRaWAN credentials:  
     - `DevEUI`  
     - `AppEUI`  
     - `AppKey` 
          
2.**Register device information**

3.**Configure Payload Decoder**

Since the payload recived by TTN will contains only bytes we need to convert them in some meangingfull information. Based on the fact that ESP32 transmit float of 4 bytes and with **little-edian** rappresentation as simple payload decoder could be the following:
     
     
          function bytesToFloat(bytes, isLittleEndian = true) {
            if (bytes.length < 4) {
              throw new Error("At least 4 bytes are required to convert to a float.");
            }
          
            // Create a DataView to read the bytes
            const buffer = new ArrayBuffer(4);
            const view = new DataView(buffer);
          
            // Copy bytes into the buffer
            for (let i = 0; i < 4; i++) {
              view.setUint8(i, bytes[i]);
            }
          
            // Read as 32-bit float
            return view.getFloat32(0, isLittleEndian);
          }
          
          function decodeUplink(input) {
            return {
              data: {
                Average: bytesToFloat(input.bytes)
              },
              warnings: [],
              errors: []
            };
          }

Once done that each message sent uplink will be decode in to a float value.
* Example:
  
  ![TTN_Comunication](https://github.com/user-attachments/assets/0f168664-4790-4a55-889f-d58ce8cbff8d)

4. **Retrasmit via MQTT**

   Trasmit the payload to the edge server via MQTT. To do so i implemented an MQTT Client on the edge server.



**Code Reference**: [transmission_lora.ino](/transmission/transmission_mqtt/transmission_lora.ino)

### Energy consumption

### End-to-end latency

![RTT_val](https://github.com/user-attachments/assets/b6f90cfe-04c4-4c5f-a201-c09a0fd08d56)

**Code Reference**: [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)

### Data volume

![Volume_val](https://github.com/user-attachments/assets/71985b13-32af-4ba9-a3e0-60b834ccc697)


**Code Reference**: [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)



---

## 🛠\⚙️ Hardware & Software requirements  
### Hardware
| Component | Specification/Purpose | Notes |
|:-------------|:--------------:|:--------------:|
| **Main Device**         | Heltec ESP32 WiFi LoRa v3         | [Datasheet](https://heltec.org/project/wifi-lora-32-v3/) |
| **Current Sensor**         | INA219 High-Side DC Current Sensor         |  I²C interface         |
| **LoRaWAN Antenna**         | 868MHz (EU) / 915MHz (US)         | EU-Region frequency compliance         |

- **Heltec ESP32 WiFi LoRa v3**   
- **Current Sensor** INA219 High-Side DC Current Sensor, used for power consumption measurements.  
- **LoRaWAN Antenna** used for connecting via LoRa protocol to **The Things Network**(TTN) via OTAA activation.
- **Arduino IDE** (with Heltec packages installed)
- **FreeRTOS** (included with ESP-IDF)  
- **MQTT Broker** (e.g., Moquitto)
---

## Setup  
1. #### Clone this repo:  
   ```bash
   git clone https://github.com/corniciluca/ESP32-LoRa-AdaptiveSampling.git
   cd ESP32-LoRa-AdaptiveSampling
2. #### Install dependencies:
     - [**ArduinoFFT**](https://github.com/kosme/arduinoFFT) (Signal processing)
     - [**PubSubClient**](https://github.com/knolleary/pubsubclient) (MQTT client)
     - [**ArduinoJSON**](https://github.com/bblanchon/ArduinoJson) (v6.x recommended)
     - **Adafruit IO Arduino** by Adafruit (version **4.3.0** or later).
     - **Adafruit GFX Library** by Adafruit.
3. #### ArduinoIDE Configuration:
   3.1 Install ESP32 Boards:
     - **File** > **Preferences** > **Additional Boards Manager URLs**:
       
          ```
          https://resource.heltec.cn/download/package_heltec_esp32_index.json
          ```
     - Then go to **Tools → Board → Board Manager** and install **Heltec ESP32 Series Dev-Boards**.
     - Go to **Sketch → Include Library → Manage Libraries**, search for and install:
     - **Heltec ESP32 Dev-Boards** by Heltec.

5. #### Install Libraries:
   * #### Method 1: Install via Library Manager (ZIP File)
     **Recommended for most users**  
     1. **Download the Library**  
        - Go to the [GitHub Releases](https://github.com/corniciluca/ESP32-LoRa-AdaptiveSampling) page.  
        - Download `ESP32_General_Libraries.zip` file.  
     
     2. **Install in Arduino IDE**  
        - Open Arduino IDE.  
        - Navigate to:  
          ```arduino
          Sketch > Include Library > Add .ZIP Library...
          ```  
        - Select the downloaded `.zip` file.  
     
     3. **Verify Installation**  
        - Check if the library appears in:  
          ```arduino
          Sketch > Include Library > ESP32_General_Libraries
          ```
   * #### Method 2: Manual Library Installation
6. #### Create secrets.h and config.h:
   * #### secrets.h: Library Installation via .ZIP
   * #### config.h: Manual Library Installation
7. #### Run:

---

## Contributing

Feel free to submit pull requests for improvements or open issues for any bugs or questions.

---
## 📝 License
This project is open-source. MIT License - See LICENSE.md.
