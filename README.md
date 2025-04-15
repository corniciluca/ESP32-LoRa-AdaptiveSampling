# ESP32-LoRa-AdaptiveSampling  
**ESP32-based IoT system leveraging FreeRTOS to implement intelligent sensor sampling. The solution dynamically optimizes sampling rates using Nyquist-compliant FFT analysis, combined with efficient edge/cloud communication through hybrid LoRaWAN, MQTT and Wi-Fi protocols.**  

---

# Table of Contents

1. [Overview](#-overview)
2. [Detailed Phase Breakdown](#detailed-phase-breakdown)
   - [Phase 1: Determine Maximum Sampling Frequency of Heltec ESP32 Wi-Fi LoRa V3](#phase-1-determine-maximum-sampling-frequency-of-heltec-esp32-wi-fi-lora-v3)
   - [Phase 2: Compute Optimal Sampling Frequency](#phase-2-compute-optimal-sampling-frequency)
   - [Phase 3: Compute Aggregates Values](#phase-3-compute-aggregates-values)
   - [Phase 4: MQTT Transmission to an Edge Server over Wi-Fi](#phase-4-mqtt-transmission-to-an-edge-server-over-wi-fi)
   - [Phase 5: LoRaWAN Uplink to The Things Network (TTN) and MQTT Transmission](#phase-5-lorawan-uplink-to-the-things-network-ttn-and-mqtt-transmission)
   - [Energy Consumption](#energy-consumption)
   - [End-to-End Latency](#end-to-end-latency)
   - [Data Volume](#data-volume)
   - [Bonus](#bonus)
4. [Setup](#setup)
5. [Contributing](#contributing)
6. [License](#-license)

---


## 📌 Overview  
ESP32-based IoT projects concerning:  

1. **Maximum Sampling Frequency Identification :** demonstrates the ESP32's capability to achieve high-frequency sampling.

2. **Optimal Sampling Frequency Adaptation :** dynamically adjusts sampling rates using FFT analysis, adhering to Nyquist principles with a safety margin (e.g., sampling at 2.5× the detected maximum frequency).
For instance:
      * Detects peak frequency 𝑓 = 5Hz → sets sampling rate to 12.5Hz (2.5×). 

3. **Computes local aggregates :** calculates aggregates of the sampled signal over a windows (e.g., data-driven sliding window).  
4. **Transmits data** via:  
   - **Wi-Fi/MQTT :** transmits aggregate values to a nearby edge server in real-time using the lightweight MQTT protocol over Wi-Fi.  
   - **LoRaWAN + TTN + MQTT :** sends aggregate values to a edge server via LoRaWAN and The Things Network (TTN), leveraging TTN’s MQTT integration.  
---
## Detailed Phase Breakdown

### Phase 1: Determine Maximum Sampling Frequency of Heltec ESP32 Wi-Fi LoRa V3

In this phase, we determine the maximum sampling frequency at which our type of ESP32 can operate. This frequency is greatly influenced by how we acquire the signal that has to be sampled. For instance, if we capture the signal using a UART, the maximum sampling frequency will be limited by the baud rate of the UART connection. In our case, the signal is generated locally in the firmware and internally sampled, this means that the maximum sampling frequency is limited by the minimum delay between two sample. Which it's bounded by the period of freeRTOS's ticks, defined by the OS as
```
portTICK_PERIOD_MS = 1 / configTICK_RATE_HZ = 1 / 1000 = 1 ms
```
So we get that the frequency is 1000Hz, specially if we assume that: 

* The sampling task has the highest priority.
* The sampling code itself adds negligible runtime overhead.

configTICK_RATE_HZ is a configuration constant that measure the frequency of the RTOS tick interrupt. Therefore a higher tick frequency means time can be measured to a higher resolution. However, it is not reccomended to use higher frequencies, since the RTOS kernel will use more CPU time so it would be less efficient. Moreover, a high tick rate will also reduce the 'time slice' given to each task. For these reasons in this project configTICK_RATE_HZ is set to 1000.

#### Implementation

In order to prove the prevuosly-made statements, i simulated the sampling of a signal giving a 1 tick delay between each samples and configTICK_RATE_HZ = 1000.

![max-freq-1](https://github.com/user-attachments/assets/8302a3cf-3048-49e3-9191-8def2c587a0f)

For configTICK_RATE_HZ = 240MHz, so equal to the clock frequency of the esp32's microprocessor, the results are the followings:

![max-freq-2](https://github.com/user-attachments/assets/057f4c6a-8edb-4c16-9063-724a4fe87829)

**Code Reference**: [max-frequency.ino](/max-frequency/max-frequency.ino)

#

### Phase 2: Compute Optimal sampling frequency

Choosing between higher or lower sampling frequencies involves critical engineering trade-offs:

**Trade offs**

- **Higher Sampling Rates** :
  
     - **Advantages**: Better signal reconstruction, reduced ISI.
       
     - **Drawbacks**: Increased CPU load, memory usage, and power consumption.
- **Lower Sampling Rates** :
  
     - **Advantages**: Resource-efficient.
       
     - **Drawbacks**: Risk of aliasing, ISI, and loss of high-frequency details.

To resolve these trade-offs, this phase focuses on computing the **optimal sampling frequency** that balances efficiency and signal integrity. The so called frequency is obtained leveraging the highest frequency of the signal and the Shannon-Nyquist theorem, to determine an efficient sampling rate capable of reconstructing the signal without aliasing or Inter-Symbol Interference (ISI). Specifically, the sampling theorem states that it's possible to reconstruct a continue signal by performing a sampling with a frequency higher that 2 * $f_{max}$, even though a common engineering practice is to pick $𝑓_{sample}$ = 2.5 * $𝑓_{max}$ for safety margins.

**Program steps**
1. **Sample at maximum frequency possible for the ESP32**:
          Given the maximum possible sample rate we sample the signal. This has the goal to correctly characterize the input signal.
2. **Compute FFT and apply window**:
        Calculate the frequency domain signal by applying an optimazed algorithm (Fast Fourier Trasform) that computes the Fourier Trasfomant of the signal. Then apply a Hamming window function to reduce the spectral leakage of the signal.
4. **Compute the max frequency of the signal**:
     This has been done using the arduinoFFT library to compute the magnitude values of the signal. Given then the magnitude of each frequency we can infer what is the maximum one by finding each peak and choicing the one with highest frequency.

   **Example**
   
   ```
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
   ```
   For the implementation I considered only the first half of g_samples_real since arduinoFFT stores the computed magnitutes in this place.
   Moreover, in this case it's important to adjust the correct noise floor level.
   
6. **Determine the optimal sampling frequency** To do so simply multy the obtained value by 2.5.
7. **Sampling at the new found frequency** Once computed the optimal frequency take samples based on this new found frequency.
8. **Restart if need** It's possible for certain real-world scenarios, when for example the observed phenomena changes, that the previously found frequency is not correct anymore. In these cases we need to detect the anomaly and restart the process in order to find a new optimal frequency. 

#

**Implementation**
- **Input 1**: `2*sin(2π*3*t) + 4*sin(2π*5*t)`
  
   ![FFT-colab](https://github.com/user-attachments/assets/f895f12e-f4c3-4fad-beab-c2f712c5756a)
  
- **Input 2**: `10*sin(2*PI*2*t) + 6*sin(2*PI*9*t)`

   ![FFT3-colab](https://github.com/user-attachments/assets/f76eee0f-a0b9-46ac-b284-531f89de8168)

- **Initial sampling frequency**: 1KHz



- **Diagram**
   <p align="center">
     <img src="https://github.com/user-attachments/assets/83a72765-aa38-41e6-973b-53ee93d940f2" width="80%" height="80%"/>
   </p>
      
  In this experiment first we simulate the signal of a phenomena using _input 1_, compute the FFT and the optimal frequency, then after 100 samples the program induces an anomaly, consisting in the change of the sampled signal into _input 2_.

- **Anomaly detection**
     
     - **Standard deviation-based method**:
     
       The firmware will detect this using a window of size SAMPLING_WINDOW_SIZE. Specifically if a given sample it's distant more than THRESHOLD_STD_DEV * standard deviation from the mean of the window, then an anomaly it's detected. Once an anomaly it's
       detected the esp32 will recompute the FFT and the new sampling frequency. This approach is real simple to apply, but has some downfalls: the standard deviation is highly sensitive to extreme values (outliers), it assumes a normal distribution of the data and this is
       generally not true so it will not accurately represent the data's variability.
     
     - **Hample filter**
   
       This other approach uses the median and median absolute deviation, that are much more stable to outliers and makes less assumptions on the distribution of the samples, making it able to effectively detect anomalies even in non-normally distributed data.

- **Results**
  <p float="left">
     <img src="https://github.com/user-attachments/assets/cb78a047-32dd-493e-a39d-7dbae4c88137" />
     &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
     <img src="https://github.com/user-attachments/assets/e26232ef-c754-4378-9525-b01833d20f4e"  />
  </p>

**Code Reference**: [sampling.ino](/sampling/sampling.ino)

#
### Phase 3: Compute aggregates values

In this phase, we aggregate the samples of the signal by computing an average of the last 5 samples using a rolling average over a 5-samples window. The implementation is done using tools given by **FreeRTOS** with the _goal_ of **parallelism** and so **efficency**. 

**Implementation:**

- **Sampling task:** This task will sample the signal using the optimal frequency and each sample will be added to **xQueue_samples**, a mechanism used for inter-task communication that allows tasks to send and receive data in a thread-safe manner, ensuring synchronization between tasks. This task will have the highest priority, otherwise the FreeRTOS scheduler could decide to schedule the **averaging task** and this could interfere with the chosen sampling frequency.
- **Averaging task:** This task will read the samples from **xQueue_samples** and compute the rolling average. To do so it uses a circular buffer of size 5, that each time recive a new sample it will compute the respective average.

A problem that may occur is that the sampling task is faster than the average task: in this case the sampling task will overwrite the oldest sample in the queue. In order to coordinate the removal of such sample from the queue i used a mutex that make the process of checking if the queue is empty and removing the oldest object atomic.

**Results**
  
  ![aggregate_results](https://github.com/user-attachments/assets/8a2ad38d-df65-405e-b05e-cca8d6fc8401)

**Code Reference**: [aggregate.ino](/aggregate/aggregate.ino)
#

### Phase 4: MQTT Transmission to an Edge Server over Wi-Fi

In this phase we comunicate with an edge server thorugh the **MQTT**, a lightweight publish-subscribe messaging protocol designed for resource-constrained IoT devices and low-bandwidth networks. The goal is to securely and efficiently transmit the **rolling average** windows from the device to the edge server over a Wi-Fi connection.

**Key Components for MQTT**
- **MQTT Broker:** A centralized server (e.g., Mosquitto or cloud-based brokers like AWS IoT Core) that manages messages routing between publishers (devices) and subscribers (edge servers).
- **Publisher:** The IoT device publishing sensor data (e.g., average) to a specific MQTT topic (e.g., `esp32/data`)
- **Subscriber:** The edge server subscribing to relevant topics to receive and process incoming data. In this case the edge server will also reply on the topic `esp32/data/acks`. 

**Diagram of tasks**

![Tasks diagram](https://github.com/user-attachments/assets/11d35d38-1605-4d2f-ab70-272228607f1c)

**Results**



|   **Heltec ESP32**             |  **Edge server**|
|:-------------------------:|:-------------------------:|
|  ![transimission_mqtt_results](https://github.com/user-attachments/assets/81cab811-99a4-428d-8345-20a54ea1df2f)  |    ![transimission_mqtt_edge_server_results](https://github.com/user-attachments/assets/7b8930fc-8173-49ca-9059-572931b121d3)|



**Code Reference**: [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)

#
#### Phase 5: LoRaWAN Uplink to The Things Network (TTN) and MQTT Transmission
In this phase, the esp32 computes the average over a certain number of seconds and instead of sending it to an edge server via **MQTT** + **Wi-Fi**, we will send it to the edge server via **LoRaWAN** + **MQTT**. Therefore, the device will transmit the values computed over **LoRaWAN** to **The Things Network (TTN)** and they will be recived by the edge server thanks to **MQTT** protocol.

**General approach**

The LoRa Transmission will use **OTAA (Over-The-Air Activation)** for device authentication and network joining. OTAA is a more secure and flexible method compared to **ABP (Activation By Personalization)**, as it  allows devices to rejoin the network after a reset or power cycle. The **payload** is composed by a single **float** value, that represents the average computed by the **ESP32** device. The float occupies **4 bytes**, also the esp32 uses the **little-endian** representation of floats, this has to be taken into account when receving the payload. Before trying to establish a connection is fundamental to:
* Set the **LoRaWAN** region(e.g. REGION_EU868)
* Store **DevEUI** - A unique device identifier (like a MAC address). 
* Store **AppEUI** - Identifies the application/provider (similar to a network ID). 
* Store **AppKey** - A root key used for secure session key generation.
* Load the **payload**
* Specify the **appPort**


**Details of implementation**

![lora diagram1](https://github.com/user-attachments/assets/5dd254b4-f82d-416f-84f9-f489749005ea)


In this project we have used **LoRaWAN Class A** which is the default and most **energy-efficient** LoRaWAN device class, optimized for battery-powered IoT devices. This class has an extremely **low power** consumption due to the fact that between the transmitting of uplink messages it will **sleep**. It's important to notice that once the esp32 wakes up from the sleep state, it will be **restared** and the values stored in the normal variables will be lost. To prevent this from appening, it's possible to save some data in **RTC registers**, registers that wont be wiped once the esp32 wakes up for the sleep state.
Using the key word **RTC_DATA_ATTR** will create a variable inside the **RTC** registers

* Example:
  
  ```
     /* Persistent State --------------------------------------------------------- */
      // RTC-retained variables (survive deep sleep)
      RTC_DATA_ATTR int sample_i = 0;        // Sample index counter
      RTC_DATA_ATTR int freq = INIT_SAMPLE_RATE; // Current sampling frequency
      RTC_DATA_ATTR bool initialized = false;// Initialization flag
      RTC_DATA_ATTR int num_of_restarts = 0; // number of restarts
  ```


**The Things Network (TTN)**

The Things Network (TTN) is an open, community-driven, and decentralized LoRaWAN network infrastructure that enables low-power, long-range IoT communications globally. To be able to use TTN follow i followed these steps:

1. **Register on TTN**

- Go to [TTN Console](https://console.thethingsnetwork.org/), select your region, and **create an account**.  
- Create an **Application**, then **Register a Device** to obtain your LoRaWAN credentials:  
     - `DevEUI`  
     - `AppEUI`  
     - `AppKey` 
          
2. **Register device information**

3. **Configure Payload Decoder**

     Since the payload recived by TTN will contains only bytes we need to convert them in some meangingful information. Based on the fact that ESP32 transmits floats of 4 bytes and with **little-endian** representation a simple payload decoder could be the following:
     
     
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

4. **Retransmit via MQTT**

   Transmit the payload to the edge server via MQTT. To do so I implemented an MQTT Client on the edge server.

   ![MQTT_LORA](https://github.com/user-attachments/assets/000c95c5-b156-4ea2-aeaa-715b404f3872)


**Code Reference**: [transmission_lora.ino](/transmission/transmission_mqtt/transmission_lora.ino)

---
### Energy consumption

This section provides an analysis of the energy consumption of the IoT system, focusing on the impact of different transmission strategies. Energy efficiency is critical for battery-powered IoT devices, and this part quantifies savings achieved through optimized communication protocols.
#
#### Over Sampling\Adaptive Sampling

In this part we discuss the power consumption differences between over-sampling a signal and adaptive sampling. In [max-frequency.ino](/max-frequency/max-frequency.ino) the esp32 firstly oversamples the given signal using 1 KHz then after it computed the optimal sample 
rate it goes in light sleep for 2 seconds, after that it start to sample at the new found frequency. In order to reduce the power consumption after each sample the esp32 goes in light sleep in which it consumes ~2mW whereas while sampling it consume almost 200mW.

This it's visible in the following image:

![FFT](https://github.com/user-attachments/assets/6fef8d8f-56ba-4978-a96e-dbf84a9c86f4)

When the anomaly is detected the esp32 will perform another oversampling in order to learn the new optimal frequency. In this case it will be higher than the first one.

![FFT (1)](https://github.com/user-attachments/assets/2dd7be9c-f6dd-44b6-8ae2-1a72b06ac592)

We can also stimate the difference of power consumption between oversampling at 1kHz and sampling at 10Hz. Assuming that time needed to take a sample it's 1ms, in the first case, since there are 1000 samples in 1 seconds, the esp32 will consume 200mW for 1000ms
consuming 200mW/s, whereas in the second case it will take 10 samples for each second consuming 200mW * 10/1000 + 2mW* 990/100 = 3.98mW/s. this means that using the optimal frequnecy for sampling we save up to 196 mW/s.

**Code Reference**: [max-frequency.ino](/max-frequency/max-frequency.ino)

#
#### LoRa transmission
**LoRa (Long Range)** is a wireless communication technology designed for **long-range**, **low-power** Internet of Things (IoT) applications. It operates in the sub-GHz bands (e.g., 868 MHz in Europe, 915 MHz in North America). This technology allows for data to be transmitted over distances of several kilometers (up to 15 km in rural areas) while consuming **minimal power**.
LoRa reduce the power consumption through a series of mechanisms:

- **Duty Cycle**: LoRaWAN adheres to regional regulations (e.g., 1% duty cycle in the EU). Devices transmit briefly and then sleep, minimizing active time. In this case LoRa **Class A** is used, this kind of operational class is the most energy efficent.
- **Adaptive Data Rate (ADR)**: Dynamically adjusts spreading factor (SF) and transmission power based on network conditions.
- **Minimal Payload Size**: The size of payload is very limited (~200 bytes), reducing transmission time and energy.

While being in deep sleep the ESP32 will only consume ~10 µA, giving energy only to **RTC & RTC Peripherals** and **ULP Coprocessor**. Having setted the Duty Cycle to 15 seconds, the device transmits a small payload over LoRa containing the computed average (a 4-byte float) every 15 seconds. According to the LoRaWAN Regional Parameters for the EU868 band and using DR3 the estimate time-on-air is ~164.9 ms (calculated with [TTN LoRaWAN airtime calculator](https://www.thethingsnetwork.org/airtime-calculator/).

A qualitative drawing of one trasmission attempt:

![Tx](https://github.com/user-attachments/assets/5d7316ab-74b0-47b3-8b84-2a884ff4f275)

**Practical analysis**

In this part i measured the energy consumption of the different phases and the communication with LoRa. As it possible to see in the following imgaes, firstly we compute the optimal frequency equal to 10Hz and start to sample. In this case we use an aggregate value based on a time-window. For this reason the size of the window and the number of samples are equal to SECONDS_WINDOW * freq. After the esp32 computes the average it will send it through LoRaWAN, TTN and MQTT to the edge server. Once the ESP32 sends the message via LoRaWAN it will go in to deep sleep for a given period bounded by the restrictions on the duty cycle in europe( 1% duty cycle). After the device wakes up it will resample the signal without re-computing the optimal rate.

As it can be seen in this image, the device after it initialized the LoRa module it connects to an nearby gateway. So it Transmits a message and waits the two recivings windows before retraing the connection.

![FFT ,sampling aggregate](https://github.com/user-attachments/assets/34794803-c83b-4f44-968b-dc2a30ca4305)


![Deep sleep](https://github.com/user-attachments/assets/c4237de8-d7c6-49e3-a3ca-54ed1b3c4892)


![Screenshot from 2025-04-14 18-26-18](https://github.com/user-attachments/assets/9ef3a5f3-b05b-447f-9f61-50c19450fee8)


**Code Reference**: [transmission_lora.ino](/transmission/transmission_mqtt/transmission_lora.ino)

#
#### Wi-Fi transmission

In the case of Wi-Fi transmission it can be difficult to estimate accuratly the power consumption, this is due to the fact that there are three tasks (**communication_task**,**average_task**,**sampling_task**) that run simultaneously. The [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)
 firmware waits until the esp32 connects to the MQTT broker before starting the sampling, aggregation and trasmission phases. For this reason, the main tasks could be delayed, and the Wi-Fi module might attempt multiple connection retries.

![1° try](https://github.com/user-attachments/assets/5385131b-f10e-4478-a189-afa4baac3554)

Once connected, it will start the previously mentioned phases. As shown, the ESP32, by default, uses a power-saving approach, entering modem-sleep mode after each transmission or reception, and it will wait until the DTIM (Delivery Traffic Indication Message) interval communicated by the access point during the connection phase.

![Wifi init](https://github.com/user-attachments/assets/882bb857-d2cd-4ce5-8cd7-ae3b6054e76c)

---

### End-to-end latency
#

In this phase we measure the Round Trip Time (RTT) between the ESP32 and the edge server while using the Wi-Fi/MQTT transmition mode. 

**Implementation**

To do so we create an edge server MQTT_Client.py that connects to an MQTT broker and subscribe to a specific topic(e.g luca/esp32/data). In the ESP32 after the **Sampling_task** inserted the samples in to the xQueue_samples they will be read by **Average_task** that computes the aggregated values and adds them to the xQueue_avgs. At the same time **Communication_task** reads from xQueue_Avgs and sends the averages to the edge server via MQTT over The Wi-Fi protocol. In order to compute the RTT each MQTT payload is made up of: ID, computed average and a timestamp. Once the edge server received the event on the topic to which it's subscribed it will publish an ack on another topic, to which the esp32 is subscribed. Therefore, the esp32 recomputes another timestamp and determines the RTT, by doing the difference between these two timestamps.

- Example :

   ![RTT](https://github.com/user-attachments/assets/aa74d6e5-be50-4600-8ebc-78083831587b)

By the mean and deviation standard we can infer :

- **Responsiveness**: A lower mean RTT indicates better responsiveness.

- **Baseline**: The mean can also be used as a baseline to detect possible network issues.

- **Stability**: The standard deviation rapresents the average deviation of an RTT form the mean. So it rapresents the variability of it. An high std suggest that RTT fluctuates significantly, which can lead to inconsistent performance, whereas a low standard deviation suggests a stable network connection with consistent latency.


**Code Reference**: [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)

#
### Data volume

Another important metric to calculate it's the data volume. Measuring the data volume (total bytes transferred) and data rate (throughput) between an IoT device and an edge server provides critical insights essential for system:

- **Storage requirements**: Estimate the amount of disk space needed on the edge server or in the cloud to store the data generated by the device(s) over time.
  
- **Comunication costs**: The total volume directly determines the communication costs. Higher volume means higher costs.
  
- **Cloud Costs**: Cloud platforms often charge based on the amount of data transferred in and out, and also for storage.
  
- **Scalability of the system**: By studying the data volume generated by a single device we can estimate the total volume when deploying multiple devices. This is crucial for planning server capacity and network infrastructure.


In this phase i computed the data volume estimating the numbers of bytes transmitted from the IoT device to the edge server and divided them by period of the transmition phase. This is an estimation since both Wi-Fi and MQTT could have an overhead on the number of bytes. This overhead can be estimated to be ~70 bytes for Wi-Fi and ~26 bytes for MQTT.

**Implementation**

   ```
   Volume of Data ≃ 2 * ((NUMBER_OF_AVGS * sizeof(char) * MSG_BUFFER_SIZE) + 70B + 26B)
   Data rate ≃ Size of Data / Duration of comunication
   ```


|   **Optimal frequency**             |  **Over-sampling**|
|:-------------------------:|:-------------------------:|
|![volume](https://github.com/user-attachments/assets/c88ef74d-03ee-40e9-a562-74370ff37d81)|![Volume_over](https://github.com/user-attachments/assets/6a3de787-40f8-4577-840a-14b392177baf)|



As the experiment proves the volume of data when over-sampling as if we where sampling at the optimal frequency, since in the code we set the same number of samples. Whereas the throughput is much higher (more than double of the optimal one). This means higher costs and less scalable. 

**Code Reference**: [transmission_mqtt.ino](/transmission/transmission_mqtt/transmission_mqtt.ino)

#
### Bonus

   - **Oversampling(Base line)**
   
   |   **Power consumption**             |  **Volume of data / data rate**| **RTT** |
   |:-------------------------:|:-------------------------:|:-------------------------:|
   | 200mW/s  | ![Volume_over](https://github.com/user-attachments/assets/6a3de787-40f8-4577-840a-14b392177baf) | ![RTT_over](https://github.com/user-attachments/assets/adef6c81-8dd3-4a6e-96e7-ecb75a3d7acb)|
   
   - **Low-Frequency Signal**
   
      Signal(t): 2*sin(2*PI*3*t) + 4*sin(2*PI*5*t)
      
      Adaptive Sampling (10Hz): 
      
      - **Active time**: 10 samples/second * 1ms/sample = 10ms active time
      
      - **Light sleep**: 990ms idle time
      
      - **Power**: (200mW * 10ms/1000ms) + (2mW * 990ms/1000ms) = 2mW + 1.98mW = 3.98mW/s
   
      **Power Savings**: 200mW/s - 3.98mW/s = 196.02mW/s
      
   |   **Power savings**             |  **Volume of data / data rate**| **RTT** |
   |:-------------------------:|:-------------------------:|:-------------------------:|
   | 196.02mW/s  | ![volume](https://github.com/user-attachments/assets/b985fd49-285a-4a56-a1dd-c15b5562dae8)| ![RTT](https://github.com/user-attachments/assets/267326fe-b649-469f-a675-cfe07d4037c0)|
      
   - **Medium-Frequency Signal**
   
      Signal(t): 8⋅sin(2π⋅100⋅t)+3⋅sin(2π⋅150⋅t)
   
      Adaptive Sampling (375Hz): 
      
      - **Active time**: 375 samples/second * 1ms/sample = 375ms active time
      
      - **Light sleep**: 625ms idle time
      
      - **Power**: (200mW * 375ms/1000ms) + (2mW * 625ms/1000ms) = 75mW + 1.25mW = 76.25mW/s
   
      **Power Savings**: 200mW/s - 76.25mW/s = 123.75mW/s
   
   |   **Power savings**             |  **Volume of data / data rate**| **RTT** |
   |:-------------------------:|:-------------------------:|:-------------------------:|
   | 123.75mW/s  | ![volume_med_opt](https://github.com/user-attachments/assets/a494fc4d-fbde-4bd5-ac1d-a87c5dd7e50c)| ![RTT_med_opt](https://github.com/user-attachments/assets/effe9db3-7e77-4e5b-b21e-fe5bf532b10f)|
      
   - **High-Frequency Signal**
   
      Signal(t): 4⋅sin(2π⋅350⋅t)+2⋅sin(2π⋅300⋅t)
   
      Adaptive Sampling (872Hz): 
   
      - **Active time**: 872 samples/second * 1ms/sample = 872ms active time
      
      - **Light sleep**: 125ms idle time
      
      - **Power**: (200mW * 872ms/1000ms) + (2mW * 125ms/1000ms) = 175mW + 0.25mW = 175.25mW/s
   
      **Power Savings**: 200mW/s - 175.25mW/s = 24,75mW/s
         
   
   |   **Power savings**             |  **Volume of data / data rate**| **RTT** |
   |:-------------------------:|:-------------------------:|:-------------------------:|
   | 24,75mW/  | ![volume_high_opt](https://github.com/user-attachments/assets/81075dbf-46f2-475a-b0b2-0ea57ef2f67a)| ![RTT_high_opt](https://github.com/user-attachments/assets/730d36c1-d6c9-488b-aed6-d54d83ab3f7f) |


**Conclusions**

As it possible to see the power savings decrease if the optimal frequency is closer to the maximal frequency and the throughtput differences of the system between over-sampling and adaptive sampling do the same. 
Adaptive sampling adds complexity and FFT processing overhead but can offer substantial savings when the signal characteristics allow for it. Fixed over-sampling is simpler but potentially wasteful.

---

## 🛠\⚙️ Hardware & Software requirements  
### Hardware
| Component | Specification/Purpose | Notes |
|:-------------|:--------------:|:--------------:|
| **Main Device**         | Heltec ESP32 Wi-Fi LoRa v3         | [Datasheet](https://heltec.org/project/Wi-Fi-lora-32-v3/) |
| **Current Sensor**         | INA219 High-Side DC Current Sensor         |  I²C interface         |
| **LoRaWAN Antenna**         | 868MHz (EU) / 915MHz (US)         | EU-Region frequency compliance         |

- **Heltec ESP32 Wi-Fi LoRa v3**   
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
     - [**Adafruit IO Arduino**](https://github.com/adafruit/Adafruit_IO_Arduino) by Adafruit (version **4.3.0** or later).
     - [**Adafruit GFX Library**](https://github.com/adafruit/Adafruit-GFX-Library) by Adafruit.
3. #### ArduinoIDE Configuration:
   3.1 Install ESP32 Boards:
     - **File** > **Preferences** > **Additional Boards Manager URLs**:
       
          ```
          https://resource.heltec.cn/download/package_heltec_esp32_index.json
          ```
     - Then go to **Tools → Board → Board Manager** and install **Heltec ESP32 Series Dev-Boards**.
     - Go to **Sketch → Include Library → Manage Libraries**, search for and install:
     - **Heltec ESP32 Dev-Boards** by Heltec.

4. #### Install Libraries:
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
     **For developers/modifiers or custom setups**  
     This method is ideal if you:  
     - Plan to modify the library code  
     - Need to place files in non-standard locations  
     - Want direct control over library files    

     **Step 1: Clone the Repository**
     
     ```bash
     git clone https://github.com/corniciluca/ESP32-LoRa-AdaptiveSampling
     ```
     
     **Step 2: Extracting the ZIP File via Command Line**
     **Windows 10+ or Linux**
     
     ```bash
     tar -xf ESP32_General_Libraries.zip -C output_folder
     ```
     
     **Step 3: Copy the libraries into your personal project**

     ```bash
     cp ESP32-LoRa-AdaptiveSampling/lib/src/*.h YourProjectName/src/
     cp ESP32-LoRa-AdaptiveSampling/lib/src/*.cpp YourProjectName/src/
     ```
     
5. #### Configuration of secrets.h & config.h:
   These files centralize critical settings and credentials for the IoT system, ensuring **modularity**, **security**, and **easy customization**.
   * #### secrets.h
     Stores sensitive **credentials** and **network configurations**
     
     | Parameter | Purpose | Example Value |
     |:-------------|:--------------:|:--------------:|
     | `**Wi-Fi_SSID**`         | Wi-Fi network name         | `"SSID"` |
     | `**Wi-Fi_PASSWORD**`         | Wi-Fi password         |  `"PASSWORD"`         |
     | `**MQTT_SERVER**`         | MQTT broker IP/hostname         | `"broker.hivemq.com"`         |
     | `**MQTT_PORT**`         | MQTT broker port         | `1883`         |

   * #### config.h
     Contains application-wide settings and parameters to **tune system behavior**
     
     | Parameter                  | Description                                                                 | Default Value              |
     |----------------------------|-----------------------------------------------------------------------------|----------------------------|
     | `WINDOW_SIZE`              | Number of samples in the moving average window                             | `5`                        |
     | `Wi-Fi_MAX_RETRIES`         | Maximum number of Wi-Fi connection retry attempts                           | `10`                       |
     | `MSG_BUFFER_SIZE`          | Size of the buffer for MQTT messages (in bytes)                            | `50`                       |
     | `RETRY_DELAY`              | Delay between connection retries (in FreeRTOS ticks)                       | `2000 / portTICK_PERIOD_MS` |
     | `MQTT_LOOP`                | Interval for MQTT client loop (in FreeRTOS ticks)                          | `1000 / portTICK_PERIOD_MS` |
     | `PUBLISH_TOPIC`            | MQTT topic for publishing sensor data                                      | `"luca/esp32/data"`        |
     | `SUBSCRIBE_TOPIC`          | MQTT topic for receiving acknowledgments                                   | `"luca/esp32/acks"`        |
     | `INIT_SAMPLE_RATE`         | Initial sampling frequency for sensors (Hz)                                | `1000`                     |
     | `NUM_SAMPLES`              | Number of samples collected for FFT analysis                               | `1024`                     |
     | `NUM_OF_SAMPLES_AGGREGATE` | Number of samples for which we have to compute aggregates values               | `10`                       |
---

## Contributing

Feel free to submit pull requests for improvements or open issues for any bugs or questions.

---
## 📝 License
This project is open-source. MIT License - See LICENSE.md.
