# 🚨 ESP32 Commercial-Grade Fire & Environmental Safety Hub

An industrial-grade, IoT-connected fire alarm control panel simulator and environmental monitoring system built on the **ESP32**. This system implements official **NFPA 72 & ISO 8201 temporal audio alert signaling**, a **60 BPM (1 Hz) latched strobe visual engine**, direct HTTPS webhooks, dynamic captive Wi-Fi portal provisioning, and **Alexa / Arduino IoT Cloud integration** with custom color-coded alert routing.

Go to 

https://safety-pulse-panel.base44.app/login


and change urls after setting up VirtualSmartHome. Then click the edit URLs button and paste in your URLs.


## ✨ Features

* **🔊 NFPA Standard Audio Signaling (3 kHz Pitch):**
  * **Temporal 3 (T3):** Fire, Smoke, High Heat, and Flame alerts.
  * **Temporal 4 (T4):** Carbon Monoxide (CO) detection.
  * **Fast March Time:** Industrial combustible gas and alcohol vapor hazards.
  * **Supervisory Signal:** Low freeze condition warning.
* **⚡ 60 BPM Latched Strobe Engine:** Non-blocking 1 Hz flash cycle (20ms pulse duration) that stays active during an alarm condition until a full panel reset is performed.
* **🔕 Audible Silence & Panel Reset:** Mute the audible horn while maintaining active visual strobes, or trigger a full commercial-style 3-second panel reset tone.
* **📶 Dynamic Wi-Fi Portal (WiFiManager):** Automatically launches a captive setup portal (`ESP32_Safety_Portal`) if no saved network is found.
* **⌨️ Live Serial Monitor Commands:** Type `R` at any time into the Serial Monitor to wipe saved Wi-Fi credentials and instantly reboot into setup mode.
* **🌐 Alexa & Arduino IoT Cloud Integration:** Control and monitor the panel remotely via cloud variables, with color-hue routing for specific hazard simulations.
* **🚀 Direct HTTPS Webhooks:** Instantly notifies external services (via Virtual Smart Home) on critical threshold trips without needing intermediate servers.

---

## 🛠️ Hardware Requirements & Pinout

| Hardware Component | Function | ESP32 GPIO Pin |
| :--- | :--- | :--- |
| **IR Flame Sensor** | Infrared Flame Detector | `GPIO 27` |
| **MQ-2 Sensor** | Smoke & Combustible Gas | `GPIO 34` (Analog) |
| **MQ-3 Sensor** | Alcohol & Hazardous Vapors | `GPIO 35` (Analog) |
| **MQ-7 Sensor** | Carbon Monoxide (CO) | `GPIO 32` (Analog) |
| **DHT22 Sensor** | Temperature & Humidity | `GPIO 23` |
| **Piezo Buzzer** | 3 kHz NFPA Audio Horn | `GPIO 25` |
| **Strobe Light / LED** | 60 BPM Visual Indicator | `GPIO 26` |

---

## 🎨 Alexa Color Hue Alert Mapping

When triggering the system via the Arduino IoT Cloud `alarm_light` (`CloudColoredLight`) variable or Alexa Routines, setting specific light colors triggers dedicated safety patterns:

| Color | Hue Range | Associated Hazard Pattern |
| :--- | :--- | :--- |
| **Red** | `0° - 15°` / `345° - 360°` | **NFPA T3** (MQ-2 Smoke / Gas) |
| **Orange** | `20° - 40°` | **NFPA T4** (MQ-7 Carbon Monoxide) |
| **Yellow** | `50° - 70°` | **March Time** (MQ-3 Hazardous Vapors) |
| **Blue** | `220° - 260°` | **Supervisory Signal** (Freeze Warning) |
| **Purple / Pink** | `270° - 335°` | **NFPA T3** (Flame / High Heat Alert) |
| **Green** | `120° - 160°` | **Audible Silence Command** |

---

## 📂 Project Structure

```text
├── sketch_aug1a.ino     # Main logic, state machines, sensors, and strobe engine
├── thingProperties.h   # Arduino IoT Cloud variables and credentials setup
└── README.md            # System documentation
