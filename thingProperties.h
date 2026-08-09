// Arduino IoT Cloud Properties Header Configuration
// File: thingProperties.h

#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

// ==========================
// Secret Credentials & Device Identifiers
// ==========================
const char DEVICE_LOGIN_NAME[]  = "CODE_HERE";       
const char SECRET_DEVICE_KEY[]  = "CODE_HERE";      

// WiFiManager handles Wi-Fi dynamically, so leave these blank!
const char SSID[]               = "";             
const char PASS[]               = "";         

// Callback Function Declaration
void onAlarmLightChange();

// ==========================
// Cloud Variables
// ==========================
CloudColoredLight alarm_light;

// ==========================
// Cloud Initialization
// ==========================
void initProperties() {
  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(SECRET_DEVICE_KEY);

  ArduinoCloud.addProperty(alarm_light, READWRITE, ON_CHANGE, onAlarmLightChange);
}

// Connection Handler
WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);
