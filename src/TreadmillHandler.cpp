#include "TreadmillHandler.h"

// --- Helper functions to read uint16 and uint32 from bytes ---
uint16_t readU16(uint8_t *data, int offset)
{
    // TODO: really needed or can we just use memcpy?
    return ((uint16_t)data[offset] << 8) | data[offset + 1];
}

uint32_t readU32(uint8_t *data, int offset)
{
    // TODO: really needed or can we just use memcpy?
    return ((uint32_t)data[offset] << 24) |
           ((uint32_t)data[offset + 1] << 16) |
           ((uint32_t)data[offset + 2] << 8) |
           ((uint32_t)data[offset + 3]);
}

TreadmillHandler::TreadmillHandler()
{
    m_pClient = nullptr;
    m_pNotifyCharacteristic = nullptr;
    m_pWriteCharacteristic = nullptr;
    m_doConnect = false;
}

TreadmillHandler::~TreadmillHandler()
{
    if (m_pClient)
    {
        m_pClient->disconnect();
        // delete m_pClient;
        m_pClient = nullptr;
    }
}

void TreadmillHandler::setSpeed(uint16_t speed)
{
    uint8_t packet[23];
    this->makePacket(CMD_START_SET_SPEED, speed, packet);
    this->sendCommand(packet, sizeof(packet));
}

void TreadmillHandler::start()
{
    uint8_t packet[23];
    this->makePacket(CMD_START_SET_SPEED, 0, packet);
    this->sendCommand(packet, sizeof(packet));
}

void TreadmillHandler::stop()
{
    uint8_t packet[23];
    this->makePacket(CMD_STOP, 0, packet);
    this->sendCommand(packet, sizeof(packet));
}

void TreadmillHandler::pause()
{
    uint8_t packet[23];
    this->makePacket(CMD_PAUSE, 0, packet);
    this->sendCommand(packet, sizeof(packet));
}

void TreadmillHandler::begin(NimBLEAddress address)
{
    m_targetAddress = address;
    m_doConnect = true;
}

// Send data to the write characteristic
bool TreadmillHandler::sendCommand(const uint8_t *data, size_t length)
{
    if (!m_pWriteCharacteristic || !m_pClient->isConnected())
    {
        log_e("Cannot write, characteristic not ready or client disconnected");
        return false;
    }

    bool success = m_pWriteCharacteristic->writeValue(data, length, true); // false = write without response
    if (!success)
    {
        log_e("Failed to write to treadmill");
        return false;
    }

    log_d("Wrote %d bytes to treadmill", length);
    return true;
}

void TreadmillHandler::handle()
{
    // handles reconnection
    if (m_doConnect && (millis() - m_lastConnectAttempt > 5000) && m_autoReconnect)
    {
        m_lastConnectAttempt = millis();
        if (this->connectToServer())
        {
            log_i("Connection successful.");
            m_doConnect = false;
        }
        else
        {
            log_e("Failed to connect - Retrying in 5 seconds...");
        }
    }
}

bool TreadmillHandler::connectToServer()
{
    if (m_pClient == nullptr)
    {
        m_pClient = BLEDevice::createClient();
        m_pClient->setDataLen(64); // Set data length to maximum
        m_pClient->setClientCallbacks(this, false);
        m_pClient->setConnectionParams(12, 24, 0, 600); // 15ms to 30ms interval, no latency, 6s timeout
        // m_pClient->setConnectTimeout(20);
    }

    log_i("Connecting to %s", m_targetAddress.toString().c_str());

    if (!m_pClient->isConnected() && !m_pClient->connect(m_targetAddress, true, true))
    {
        log_e("Failed to connect to treadmill at %s", m_targetAddress.toString().c_str());
        return false;
    }

    NimBLERemoteService *pService = m_pClient->getService(SERVICE_PAD_UUID);
    if (!pService)
    {
        log_e("Failed to find treadmill service UUID: %s", SERVICE_PAD_UUID);
        m_pClient->disconnect();
        return false;
    }

    m_pWriteCharacteristic = pService->getCharacteristic(CHARACTERISTIC_WRITE_UUID);
    if (!m_pWriteCharacteristic || !m_pWriteCharacteristic->canWrite())
    {
        log_e("Write characteristic not found or not writable!");
        m_pClient->disconnect();
        return false;
    }
    log_i("Write characteristic initialized");

    m_pNotifyCharacteristic = pService->getCharacteristic(CHARACTERISTIC_NOTIFY_STATE_UUID);
    if (!m_pNotifyCharacteristic)
    {
        log_e("Notify characteristic not found!");
        m_pClient->disconnect();
        return false;
    }

    if (m_pNotifyCharacteristic->canNotify())
    {
        // m_pNotifyCharacteristic->subscribe(true, notifyCallback);
        m_pNotifyCharacteristic->subscribe(
            true,
            [this](NimBLERemoteCharacteristic *chr,
                   uint8_t *data,
                   size_t length,
                   bool isNotify)
            {
                this->notifyCallback(chr, data, length, isNotify);
            });
        log_i("Subscribed to notifications.");
    }
    else
    {
        log_e("Notify characteristic cannot notify!");
        m_pClient->disconnect();
        return false;
    }

    return true;
}

// Generates a 23-byte command packet for the treadmill
void TreadmillHandler::makePacket(CommandType type, uint16_t speed, uint8_t *outPacket)
{
    // type: "start", "pause", "stop", "set_speed"
    // outPacket must be at least 23 bytes

    // --- START / HEADER ---
    outPacket[0] = 0x6A; // START_BYTE
    outPacket[1] = 0x17; // LENGTH

    // Bytes 2-5 are reserved (0)
    for (int i = 2; i <= 5; ++i)
        outPacket[i] = 0;

    // --- Speed ---
    outPacket[6] = (speed >> 8) & 0xFF;
    outPacket[7] = speed & 0xFF;

    // Magical byte: 5 for set_speed, 1 for others
    outPacket[8] = (speed != 0) ? 5 : 1;

    outPacket[9] = 0;   // incline
    outPacket[10] = 80; // weight default
    outPacket[11] = 0;  // reserved

    // Command byte
    uint8_t cmd = type; // default start/set_speed

    outPacket[12] = cmd & 0xF7; // kph mode (bit 3 = 0)

    // User ID 8 bytes (default 58965456623)
    uint64_t userId = 58965456623ULL;
    for (int i = 0; i < 8; ++i)
    {
        outPacket[13 + i] = (userId >> (56 - i * 8)) & 0xFF;
    }

    // --- Checksum ---
    uint8_t checksum = 0;
    for (int i = 1; i <= 20; ++i)
    {
        checksum ^= outPacket[i];
    }
    outPacket[21] = checksum;

    // END_BYTE
    outPacket[22] = 0x43;
}

// --- Notification callback ---
void TreadmillHandler::notifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    log_d("Notification received, length: %d", length);

    // Log payload as hex
    /**
    Serial.print("Payload (hex): ");
    for (size_t i = 0; i < length; i++)
    {
        if (pData[i] < 16)
            Serial.print("0");
        Serial.print(pData[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
     */
    if (length < 31)
    {
        log_e("Invalid treadmill packet (too short).");
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

    TreadMillData::Status running_state = TreadMillData::STOPPED; // Default stopped
    if (running_state_bits == 24)
        running_state = TreadMillData::STARTING;
    else if (running_state_bits == 8)
        running_state = TreadMillData::RUNNING;
    else if (running_state_bits == 16)
        running_state = TreadMillData::PAUSED;

    const char *statusArr[] = {"Starting", "Running", "Paused", "Stopped"};
    const char *speed_unit = (unit_mode == 1) ? "mph" : "kph";
    const char *distance_unit = (unit_mode == 1) ? "mi" : "km";

    // log_i("--- Treadmill Data ---");
    // log_i("Speed: %.2f %s", (float)current_speed / 1000.0, speed_unit);
    // log_i("Distance: %.2f %s", (float)distance / 1000.0, distance_unit);
    // log_i("Calories: %d kcal", calories);
    // log_i("Steps: %d", steps);
    // log_i("Duration (s): %d", duration / 1000);
    // log_i("Status: %s", statusArr[running_state]);
    // log_i("---------------------");

    TreadMillData data;
    data.speedKph = (float)current_speed / 1000.0;
    data.distanceKm = (float)distance / 1000.0;
    data.calories = calories;
    data.steps = steps;
    data.durationSec = duration / 1000;
    data.status = running_state;

    m_lastData = data;
    if (m_onDataUpdate)
    {
        m_onDataUpdate(data);
    }
}