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
1. **Sample at maximum frequency possible for the ESP32**
     Given the maximum possible sample rate we sample the signal for a total of 1024 samples. This has the goal to correctly careaterize the input signal.
2. **Compute FFT and apply window**
3. **Compute the major peak of the signal**
4. **Determine the optimal sampling frequency**

**Practical example**
- **Input**: `2*sin(2π*3*t) + 4*sin(2π*5*t)`
- **Initial sampling frequency**: 1KHz
- **Results:**

  ![OptSampl](https://github.com/user-attachments/assets/fbd8a22a-328e-493b-8013-8758ab61b851)

     
The experimental results are corrected, indeed the input signal is a sum of two sinusoid with frequency 3Hz and 5Hz. This can also be view simply by doing the Fourier traform of the function:

![FFT-colab](https://github.com/user-attachments/assets/f895f12e-f4c3-4fad-beab-c2f712c5756a)


**Code Reference**: [sampling.ino](/sampling/sampling.ino)
 
### Phase 3: Compute aggregates values

**Code Reference**: [sampling.ino](/sampling/sampling.ino)

### Phase 4: Transmit averages to Edge Server via TTN/MQTT/WIFI



#### Phase 4.1: Transmit averages to Edge Server via TTN/MQTT/WIFI

**Code Reference**: [sampling.ino](/sampling/sampling.ino)

#### Phase 4.2: Transmit averages to Edge Server via TTN/MQTT/WIFI

**Code Reference**: [sampling.ino](/sampling/sampling.ino)

### Energy consumption

### End-to-end latency

**Code Reference**: [sampling.ino](/sampling/sampling.ino)

### Data volume

**Code Reference**: [sampling.ino](/sampling/sampling.ino)



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
