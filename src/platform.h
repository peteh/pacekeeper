#pragma once
#include <Arduino.h>

#define SYSTEM_NAME "PaceKeeper"
#define VERSION "2026.1.0"

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

// Declare strings as extern to avoid multiple-definition linker errors
extern const char* HOMEASSISTANT_STATUS_TOPIC;
extern const char* HOMEASSISTANT_STATUS_TOPIC_ALT;

class TreadMillData
{
public:
    enum Status
    {
        STARTING = 0,
        RUNNING = 1,
        PAUSED = 2,
        STOPPED = 3,
        DISCONNECTED = 100, // Internal state, don't use for treadmill communication
    };

    float speedCmd = 0.0;
    float speedFeedback = 0.0;
    float distanceKm = 0.0;
    uint16_t calories = 0;
    uint32_t steps = 0;
    uint32_t durationSec = 0;
    Status status = DISCONNECTED; // default to DISCONNECTED when we start up
};

