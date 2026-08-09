/* 
  ESP32 Environmental & Fire Safety System with Arduino Cloud & WiFiManager
  Features: 3 kHz NFPA Audio, 60 BPM Latched Strobe Engine, Dynamic WiFiManager, 
            Audible Silence Latch, Commercial Panel Reset Tone
  Sensors: IR Flame (Pin 27), MQ-3 (Pin 35), MQ-7 (Pin 32), MQ-2 (Pin 34), DHT22 (Pin 23)
  Outputs: Piezo Audio (Pin 25), Latched Strobe Indicator Light (Pin 26)
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>          // Required for WiFiManager portal
#include <DNSServer.h>          // Required for captive portal
#include <WiFiManager.h>        // Dynamic Wi-Fi Configuration
#include <thingProperties.h>
#include <DHT.h>

// ==========================
// Pin Definitions
// ==========================
const int FLAME_PIN  = 27;   // IR Flame Sensor Digital Output
const int MQ3_PIN    = 35;   // MQ-3 Sensor Analog Output
const int MQ7_PIN    = 32;   // MQ-7 Sensor Analog Output
const int MQ2_PIN    = 34;   // MQ-2 Sensor Analog Output
const int DHT_PIN    = 23;   // DHT22 Data Pin
const int BUZZER_PIN = 25;   // Piezo Buzzer Pin (3 kHz NFPA Audio)
const int LIGHT_PIN  = 26;   // Indicator Light Pin (60 BPM Latched Strobe)

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// ==========================
// Alert Webhook URLs
// ==========================
const char* alertURL_Flame     = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=71b3a560-fcb7-4b26-8e6a-7a80d3e31481&token=e32f1eb2-88d0-44dd-a196-4672e37d2052&response=html";
const char* alertURL_MQ7       = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=15ae597d-2ed0-48d4-b09b-41b128dce77f&token=1c6cfc22-60b0-4961-b2c3-533581ad50c5&response=html";
const char* alertURL_MQ3       = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=4b03217a-a163-4a11-87c4-b21bd1770793&token=781fa5c4-6bb0-4b76-8976-0c9e0e74c3d5&response=html";
const char* alertURL_MQ2       = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=975994d5-2dba-4aca-bc0b-451429d61ad8&token=5f905c50-5db8-4896-bcec-8bffbe59853f&response=html";
const char* alertURL_HighHeat  = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=d1d89ad1-6deb-494e-8c88-9b0c150d337f&token=0c636f6b-6245-425c-b099-3b9553b2a857&response=html";
const char* alertURL_LowFreeze = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=4f711e5a-36a6-4b59-9f1b-b35cff9034de&token=d827a700-d25c-47e5-9c52-afbe4633ecf6&response=html";
const char* alertURL_Silence   = "https://www.virtualsmarthome.xyz/url_routine_trigger/activate.php?trigger=0ee7ec1d-0851-4928-bee4-6be0c0dd0ceb&token=162639b6-2b63-42dd-af61-e9b2da37c07a&response=html"; // Paste silence webhook if using one

// ==========================
// Variables & Thresholds
// ==========================
bool alertSent = false;
unsigned long flameStartTime = 0;

const int MQ7_THRESHOLD   = 2000;
const int MQ3_THRESHOLD   = 2000;
const int MQ2_THRESHOLD   = 2000; 
const float HEAT_LIMIT_F  = 100.0;
const float FREEZE_LIMIT_F = 35.0;

// ==========================
// NFPA Standard Pitch Definition
// ==========================
const int NFPA_PITCH_HZ = 3100; // Standard 3 kHz evacuation pitch

// ==========================
// System State Latches
// ==========================
bool strobeLatched = false;   // True when any alarm trips until full reset
bool audioSilenced = false;   // True when silenced until reset or new alarm
int activeAlarmType = 0;      // 0=None, 1=Fire/T3, 2=CO/T4, 3=Hazard, 4=Supervisory

// ==========================
// Strobe Timing (60 BPM)
// ==========================
const int STROBE_CYCLE_MS = 1000; // 1000ms = 60 BPM
const int STROBE_FLASH_MS = 20;   // 20ms quick pop

// Function Declarations
void updateStrobe();
void smartDelay(unsigned long ms);
void playT3FireAlert();
void playT4CoAlert();
void playGasHazardAlert();
void playSupervisoryAlert();
void stopAudio();
void silenceAudible();
void resetSystem();
void sendAlert(const char* url, const char* alertType);
// ==========================
// Live Serial Command Monitor
// Type 'R' or 'r' anytime to wipe Wi-Fi and reboot to portal
// ==========================
void checkSerialCommands() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      Serial.println("\n⚠️ 'R' COMMAND RECEIVED!");
      Serial.println("Wiping saved Wi-Fi credentials and rebooting into ESP32_Safety_Portal...");
      
      WiFiManager wm;
      wm.resetSettings(); // Erases stored NVS Wi-Fi credentials
      delay(1000);
      ESP.restart();      // Reboots board straight into portal
    }
  }
}
// ==========================
// 60 BPM Strobe Engine
// ==========================
void updateStrobe() {
  if (!strobeLatched) {
    digitalWrite(LIGHT_PIN, LOW);
    return;
  }
  
  unsigned long timeInCycle = millis() % STROBE_CYCLE_MS;
  if (timeInCycle < STROBE_FLASH_MS) {
    digitalWrite(LIGHT_PIN, HIGH); // Flash ON 20ms
  } else {
    digitalWrite(LIGHT_PIN, LOW);  // OFF remaining time
  }
}

void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    updateStrobe();
    ArduinoCloud.update();
    checkSerialCommands(); // 👈 Checks for 'R' while waiting!
    if (audioSilenced) {
      stopAudio(); 
    }
    yield();
  }
}

// ==========================
// Audio & Panel Handlers
// ==========================
void stopAudio() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

// Mutes audio while keeping strobes active & fires silence webhook
void silenceAudible() {
  audioSilenced = true;
  stopAudio();
  Serial.println("🔕 AUDIBLE SILENCE: Buzzer muted, strobes active.");

  if (strlen(alertURL_Silence) > 0 && String(alertURL_Silence) != "YOUR_SILENCE_WEBHOOK_URL_HERE") {
    sendAlert(alertURL_Silence, "Audible Silence");
  }
}

// Panel Reset: Clears latches and plays 3-second 3 kHz panel tone
void resetSystem() {
  Serial.println("🔄 SYSTEM RESET INITIATED...");
  
  strobeLatched = false;
  audioSilenced = false;
  activeAlarmType = 0;
  digitalWrite(LIGHT_PIN, LOW);

  // Play 3-second 3 kHz Fire Alarm Panel Reset Tone
  tone(BUZZER_PIN, NFPA_PITCH_HZ);
  delay(2000); 
  stopAudio();

  flameStartTime = 0;
  alertSent = false;
  Serial.println("✅ SYSTEM RESET COMPLETE: Panel restored to normal standby.");
}

// ==========================
// Direct HTTPS Webhook Sender
// ==========================
void sendAlert(const char* url, const char* alertType) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    String fullURL = String(url);
    int hostEnd = fullURL.indexOf('/', 8);
    if (hostEnd == -1) return;

    String host = fullURL.substring(8, hostEnd);
    String path = fullURL.substring(hostEnd);

    Serial.print("[");
    Serial.print(alertType);
    Serial.print("] Sending trigger to ");
    Serial.println(host);

    if (client.connect(host.c_str(), 443)) {
      client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "User-Agent: ESP32-Safety-Hub\r\n" +
                   "Connection: close\r\n\r\n");

      Serial.print("[");
      Serial.print(alertType);
      Serial.println("] Webhook Sent Successfully!");
      client.stop();
    } else {
      Serial.println("Connection to webhook server failed.");
    }
  } else {
    Serial.println("Wi-Fi not connected. Cannot send alert.");
  }
}

// ==========================
// NFPA Standard Sound Patterns (3 kHz)
// ==========================

// NFPA 72 Temporal 3 (T3) - Fire / Smoke / Heat / Flame
void playT3FireAlert() {
  strobeLatched = true;
  activeAlarmType = 1;

  if (audioSilenced) return;

  for (int i = 0; i < 3; i++) {
    if (audioSilenced) break;
    tone(BUZZER_PIN, NFPA_PITCH_HZ);
    smartDelay(500);
    stopAudio();
    smartDelay(500);
  }
  
  if (!audioSilenced) {
    smartDelay(1000);
  }
}

// NFPA 720 / NFPA 72 Temporal 4 (T4) - Carbon Monoxide (MQ-7)
void playT4CoAlert() {
  strobeLatched = true;
  activeAlarmType = 2;

  if (audioSilenced) return;

  for (int i = 0; i < 4; i++) {
    if (audioSilenced) break;
    tone(BUZZER_PIN, NFPA_PITCH_HZ);
    smartDelay(100);
    stopAudio();
    smartDelay(100);
  }

  if (!audioSilenced) {
    smartDelay(2000);
  }
}

// Industrial Hazard Pattern - Alcohol / Vapors (MQ-3)
void playGasHazardAlert() {
  strobeLatched = true;
  activeAlarmType = 3;

  if (audioSilenced) return;

  
    tone(BUZZER_PIN, NFPA_PITCH_HZ);
    smartDelay(240);
    stopAudio();
    
  if (!audioSilenced) {
    smartDelay(125);
  }
}

// NFPA Supervisory Signal - Freeze Warning
void playSupervisoryAlert() {
  strobeLatched = true;
  activeAlarmType = 4;

  if (audioSilenced) return;

  tone(BUZZER_PIN, NFPA_PITCH_HZ);
  smartDelay(500);
  stopAudio();

  if (!audioSilenced) {
    smartDelay(400);
  }
}

// ==========================
// Alexa / Arduino Cloud Callback
// ==========================
void onAlarmLightChange() {
  if (alarm_light.getSwitch()) {
    
    audioSilenced = false; // Reset mute on new trigger
    
    float hue = alarm_light.getHue();
    int currentHue = (int)hue;

    Serial.print("Alexa Trigger Received! Hue Level: ");
    Serial.println(currentHue);

    if ((currentHue >= 0 && currentHue <= 15) || currentHue >= 345) {
      playT3FireAlert();      // Red = MQ-2 Smoke/Fire (T3)
    } else if (currentHue >= 50 && currentHue <= 70) {
      playGasHazardAlert();  // Yellow = MQ-3 Gas/Vapors
    } else if (currentHue >= 20 && currentHue <= 40) {
      playT4CoAlert();        // Orange = MQ-7 CO (T4)
    } else if (currentHue >= 270 && currentHue <= 290) {
      playSupervisoryAlert();    // Purple = Heat (T3)
    } else if (currentHue >= 220 && currentHue <= 260) {
      playSupervisoryAlert(); // Blue = Freeze (Supervisory)
    } else if (currentHue >= 310 && currentHue <= 335) {
      playT3FireAlert();      // Pink = IR Flame (T3)
    } else if (currentHue >= 120 && currentHue <= 160) {
      // Green = Silence Command via Alexa Color Selection
      silenceAudible();
    } else {
      playT3FireAlert();      // Default fallback
    }

    alarm_light.setSwitch(false);
    ArduinoCloud.update();
  } else {
    // Turning switch OFF triggers full System Reset
    resetSystem();
  }
}

// ==========================
// Setup & Main Loop
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(FLAME_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  
  // Clean initialization
  strobeLatched = false;
  audioSilenced = false;
  activeAlarmType = 0;
  stopAudio();
  digitalWrite(LIGHT_PIN, LOW);

  dht.begin();

  // ==========================
  // WiFiManager Setup
  // Spin up hotspot "ESP32_Safety_Portal" if no saved credentials exist
  // ==========================
  WiFiManager wm;
  Serial.println("Starting Wi-Fi Manager Portal...");
  
  bool res = wm.autoConnect("ESP32_Safety_Portal");

  if (!res) {
    Serial.println("Failed to connect or timed out configuring Wi-Fi.");
    ESP.restart();
  } else {
    Serial.println("Connected to Wi-Fi successfully!");
  }

  // Initialize Arduino IoT Cloud AFTER Wi-Fi connects
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  Serial.println("NFPA 3 kHz Commercial Safety Hub Active + WiFiManager Live.");
}

void loop() {
  ArduinoCloud.update();
  updateStrobe();
  checkSerialCommands(); // 👈 Checks for 'R' on every loop pass!

  // ... [keep the rest of your sensor reads and logic exact same] ...

  // Read Sensors
  int mq3Value   = analogRead(MQ3_PIN);
  int mq7Value   = analogRead(MQ7_PIN);
  int mq2Value   = analogRead(MQ2_PIN);
  int flameRaw   = digitalRead(FLAME_PIN);
  bool flameDetected = (flameRaw == LOW);

  float tempF = dht.readTemperature(true);

  // Serial Diagnostics
  Serial.print("Flame: ");
  Serial.print(flameRaw);
  Serial.print(" | MQ-2: ");
  Serial.print(mq2Value);
  Serial.print(" | MQ-3: ");
  Serial.print(mq3Value);
  Serial.print(" | MQ-7: ");
  Serial.print(mq7Value);
  Serial.print(" | Temp: ");
  Serial.print(tempF);
  Serial.println("°F");

  // ----------------------------------------------------
  // Sensor Evaluations & Webhook Triggers
  // ----------------------------------------------------
  if (!isnan(tempF)) {
    if (tempF >= HEAT_LIMIT_F) {
      if (activeAlarmType == 0) audioSilenced = false;
      playSupervisoryAlert();
      sendAlert(alertURL_HighHeat, "High Heat Alert");
    } else if (tempF <= FREEZE_LIMIT_F) {
      if (activeAlarmType == 0) audioSilenced = false;
      playSupervisoryAlert();
      sendAlert(alertURL_LowFreeze, "Low Freeze Alert");
    }
  }

  if (flameDetected) {
    if (activeAlarmType == 0) audioSilenced = false;
    playT3FireAlert();

    if (flameStartTime == 0) flameStartTime = millis();

    if ((millis() - flameStartTime >= 3000) && !alertSent) {
      sendAlert(alertURL_Flame, "Flame Alert");
      alertSent = true;
    }
  } else {
    flameStartTime = 0;
    alertSent = false;
  }

  if (mq2Value > MQ2_THRESHOLD) {
    if (activeAlarmType == 0) audioSilenced = false;
    playT3FireAlert();
    sendAlert(alertURL_MQ2, "MQ-2 Alert");
  }

  if (mq7Value > MQ7_THRESHOLD) {
    if (activeAlarmType == 0) audioSilenced = false;
    playT4CoAlert();
    sendAlert(alertURL_MQ7, "MQ-7 Alert");
  }

  if (mq3Value > MQ3_THRESHOLD) {
    if (activeAlarmType == 0) audioSilenced = false;
    playGasHazardAlert();
    sendAlert(alertURL_MQ3, "MQ-3 Alert");
  }

  // ----------------------------------------------------
  // Latching Engine: Keep audio pattern cycling if latched & not silenced
  // ----------------------------------------------------
  if (strobeLatched && !audioSilenced) {
    switch (activeAlarmType) {
      case 1: playT3FireAlert(); break;
      case 2: playT4CoAlert(); break;
      case 3: playGasHazardAlert(); break;
      case 4: playSupervisoryAlert(); break;
    }
  }

  smartDelay(100);
}