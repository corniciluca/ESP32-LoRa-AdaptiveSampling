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

### Setup  
1. Clone this repo:  
   ```bash
   git clone https://github.com/yourusername/ESP32-lora-adaptivesampling.git

---

## Contributing

Feel free to submit pull requests for improvements or open issues for any bugs or questions.

---
## 📝 License
This project is open-source. MIT License - See LICENSE.md.
