#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "TreadmillHandler.h"

TreadmillHandler treadmill;

// --- Setup ---
void setup()
{
  Serial.begin(115200);
  NimBLEAddress targetAddress(std::string(TARGET_ADDRESS), BLE_ADDR_PUBLIC);
  treadmill.begin(targetAddress);
  Serial.println("Starting BLE Client...");
  NimBLEDevice::init("PaceKeeper");
}
int currentspeed = 1000;
// --- Loop ---
void loop()
{
  treadmill.handle();
  if (treadmill.isConnected())
  {
    log_d("Client connected, sending start command...");

    currentspeed += 100;
    if (currentspeed > 2500) {
      currentspeed = 900;
      treadmill.stop();
    }
    else 
    {
      treadmill.setSpeed(currentspeed);
    }
    delay(5000);
  }
  // Notifications are handled in the callback
  delay(1000);
}
