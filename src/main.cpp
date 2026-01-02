#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

// --- Configuration ---
#define TARGET_ADDRESS "C4:A9:B8:F8:47:D4"

// SERVICE: 00001800-0000-1000-8000-00805f9b34fb (Generic Access Profile)
//   CHARACTERISTIC: 00002a01-0000-1000-8000-00805f9b34fb [read]
//   CHARACTERISTIC: 00002a00-0000-1000-8000-00805f9b34fb [read]
// SERVICE: 00001801-0000-1000-8000-00805f9b34fb (Generic Attribute Profile)
//   CHARACTERISTIC: 00002a05-0000-1000-8000-00805f9b34fb [indicate]
// SERVICE: 00001910-0000-1000-8000-00805f9b34fb (Vendor specific)
//   CHARACTERISTIC: 00002b10-0000-1000-8000-00805f9b34fb [read,notify]
//   CHARACTERISTIC: 00002b11-0000-1000-8000-00805f9b34fb [read,write-without-response,write]
// SERVICE: 0000fba0-0000-1000-8000-00805f9b34fb (Vendor specific)
#define SERVICE_PAD_UUID "0000fba0-0000-1000-8000-00805f9b34fb"
//  CHARACTERISTIC: 0000fba1-0000-1000-8000-00805f9b34fb [read,write]
#define CHARACTERISTIC_WRITE_UUID "0000fba1-0000-1000-8000-00805f9b34fb"
//  CHARACTERISTIC: 0000fba2-0000-1000-8000-00805f9b34fb [read,notify]
#define CHARACTERISTIC_NOTIFY_STATE_UUID "0000fba2-0000-1000-8000-00805f9b34fb"

NimBLEClient *pClient = nullptr;
NimBLERemoteCharacteristic *pNotifyCharacteristic = nullptr;
NimBLEAddress targetAddress(std::string(TARGET_ADDRESS), BLE_ADDR_PUBLIC);
bool doConnect = true;

// --- Helper functions to read uint16 and uint32 from bytes ---
uint16_t readU16(uint8_t* data, int offset) 
{
  // TODO: really needed or can we just use memcpy?
  return ((uint16_t)data[offset] << 8) | data[offset + 1];
}

uint32_t readU32(uint8_t* data, int offset) 
{
  // TODO: really needed or can we just use memcpy?
  return ((uint32_t)data[offset] << 24) |
         ((uint32_t)data[offset + 1] << 16) |
         ((uint32_t)data[offset + 2] << 8) |
         ((uint32_t)data[offset + 3]);
}

// --- Notification callback ---
void notifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
  Serial.print("Notification, byteLength: ");
  Serial.println(length);

  // Log payload as hex
  Serial.print("Payload (hex): ");
  for (size_t i = 0; i < length; i++)
  {
    if (pData[i] < 16)
      Serial.print("0");
    Serial.print(pData[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (length < 31)
  {
    Serial.println("Invalid treadmill packet (too short).");
    // Here you could trigger a 'stopped/disconnected' state if needed
    return;
  }

  // Parse treadmill fields
  uint16_t current_speed = readU16(pData, 3);
  uint32_t distance = readU32(pData, 7);
  uint16_t calories = ((uint16_t)pData[18] << 8) | pData[19];
  uint32_t steps = readU32(pData, 14);
  uint32_t duration = readU32(pData, 20);
  uint8_t flags = pData[26];

  uint8_t unit_mode = (flags & 128) ? 1 : 0;
  uint8_t running_state_bits = flags & 24;

  uint8_t running_state = 3; // Default stopped
  if (running_state_bits == 24)
    running_state = 0;
  else if (running_state_bits == 8)
    running_state = 1;
  else if (running_state_bits == 16)
    running_state = 2;

  const char *statusArr[] = {"Starting", "Running", "Paused", "Stopped"};
  const char *speed_unit = (unit_mode == 1) ? "mph" : "kph";
  const char *distance_unit = (unit_mode == 1) ? "mi" : "km";

  log_i("--- Treadmill Data ---");
  log_i("Speed: %.2f %s", (float)current_speed / 1000.0, speed_unit);
  log_i("Distance: %.2f %s", (float)distance / 1000.0, distance_unit);
  log_i("Calories: %d kcal", calories);
  log_i("Steps: %d", steps);
  log_i("Duration (s): %d", duration / 1000);
  log_i("Status: %s", statusArr[running_state]);
  log_i("---------------------");
}

#include <stdint.h>
enum CommandType {
    CMD_START_SET_SPEED = 4,
    CMD_PAUSE = 2,
    CMD_STOP = 0
};

// Generates a 23-byte command packet for the treadmill
void makePacket(CommandType type, uint16_t speed, uint8_t* outPacket) {
    // type: "start", "pause", "stop", "set_speed"
    // outPacket must be at least 23 bytes

    // --- START / HEADER ---
    outPacket[0] = 0x6A; // START_BYTE
    outPacket[1] = 0x17; // LENGTH

    // Bytes 2-5 are reserved (0)
    for (int i = 2; i <= 5; ++i) outPacket[i] = 0;

    // --- Speed ---
    outPacket[6] = (speed >> 8) & 0xFF;
    outPacket[7] = speed & 0xFF;

    // Magical byte: 5 for set_speed, 1 for others
    outPacket[8] = (type == CMD_START_SET_SPEED) ? 5 : 1;

    outPacket[9]  = 0;   // incline
    outPacket[10] = 80;  // weight default
    outPacket[11] = 0;   // reserved

    // Command byte
    uint8_t cmd = type; // default start/set_speed

    outPacket[12] = cmd & 0xF7; // kph mode (bit 3 = 0)

    // User ID 8 bytes (default 58965456623)
    uint64_t userId = 58965456623ULL;
    for (int i = 0; i < 8; ++i) {
        outPacket[13 + i] = (userId >> (56 - i * 8)) & 0xFF;
    }

    // --- Checksum ---
    uint8_t checksum = 0;
    for (int i = 1; i <= 20; ++i) {
        checksum ^= outPacket[i];
    }
    outPacket[21] = checksum;

    // END_BYTE
    outPacket[22] = 0x43;
}


NimBLERemoteCharacteristic* pWriteCharacteristic = nullptr;

// Send data to the write characteristic
bool sendToTreadmill(const uint8_t* data, size_t length) {
    if (!pWriteCharacteristic || !pClient->isConnected()) {
        Serial.println("Cannot write, characteristic not ready or client disconnected");
        return false;
    }

    bool success = pWriteCharacteristic->writeValue(data, length, true); // false = write without response
    if (!success) {
        Serial.println("Write failed");
        return false;
    }

    Serial.print("Wrote ");
    Serial.print(length);
    Serial.println(" bytes to treadmill");
    return true;
}


// --- Client callbacks for connect/disconnect ---
class MyClientCallbacks : public NimBLEClientCallbacks
{
  void onConnect(BLEClient *pClient) override
  {
    Serial.println("Connected to device!");
  }

  void onDisconnect(BLEClient *pClient, int reason) override
  {
    Serial.println("Disconnected! Will attempt reconnect...");
    doConnect = true; // Trigger reconnect in loop
  }
};

// --- Connect and subscribe ---
bool connectToServer()
{
  if (pClient == nullptr)
  {
    pClient = BLEDevice::createClient();
    pClient->setDataLen(64); // Set data length to maximum
    pClient->setClientCallbacks(new MyClientCallbacks());
  }

  Serial.print("Connecting to ");
  Serial.println(targetAddress.toString().c_str());

  if (!pClient->connect(targetAddress))
  {
    Serial.println("Failed to connect!");
    return false;
  }

  BLERemoteService *pService = pClient->getService(SERVICE_PAD_UUID);
  if (!pService)
  {
    Serial.println("Service not found!");
    pClient->disconnect();
    return false;
  }

  pWriteCharacteristic = pService->getCharacteristic(CHARACTERISTIC_WRITE_UUID);
  if (!pWriteCharacteristic || !pWriteCharacteristic->canWrite()) {
      Serial.println("Write characteristic not available");
      pClient->disconnect();
      return false;
  }
  Serial.println("Write characteristic initialized");

  pNotifyCharacteristic = pService->getCharacteristic(CHARACTERISTIC_NOTIFY_STATE_UUID);
  if (!pNotifyCharacteristic)
  {
    Serial.println("Notify characteristic not found!");
    pClient->disconnect();
    return false;
  }

  if (pNotifyCharacteristic->canNotify())
  {
    pNotifyCharacteristic->subscribe(true, notifyCallback);
    Serial.println("Subscribed to notifications!");
  }
  else
  {
    Serial.println("Characteristic cannot notify.");
  }

  return true;
}

// --- Setup ---
void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE Client...");

  BLEDevice::init("");
}
int currentspeed = 1000;
// --- Loop ---
void loop()
{
  if (doConnect)
  {
    if (connectToServer())
    {
      log_i("Connection successful.");
      doConnect = false;
    }
    else
    {
      log_e("Failed to connect - Retrying in 5 seconds...");
      delay(5000);
    }
  }

  if (pClient->isConnected() && false)
  {
    log_d("Client connected, sending start command...");
    uint8_t packet[23];
    currentspeed += 100;
    CommandType command = CMD_START_SET_SPEED;
    if (currentspeed > 2500) {
      currentspeed = 900;
      command = CMD_STOP;
    }
    makePacket(command, currentspeed, packet);  // speed = 1200 -> 12.00 kph
    sendToTreadmill(packet, sizeof(packet)); // Uses the write function we made earlier

    delay(5000);
  }
  // Notifications are handled in the callback
  delay(1000);
}
