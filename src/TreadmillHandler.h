#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

class TreadmillHandler : public NimBLEClientCallbacks
{
public:
    TreadmillHandler();
    ~TreadmillHandler();
    void begin(NimBLEAddress address);
    void setSpeed(uint16_t speed);
    void start();
    void pause();
    void stop();

    void handle();

    bool isConnected() const
    {
        return m_pClient && m_pClient->isConnected();
    }

    enum CommandType
    {
        CMD_START_SET_SPEED = 4,
        CMD_PAUSE = 2,
        CMD_STOP = 0
    };

private:
    bool sendCommand(const uint8_t *data, size_t length);
    void makePacket(CommandType command, uint16_t speed, uint8_t *outPacket);
    bool connectToServer();
    static void notifyCallback(
        NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
        uint8_t *pData,
        size_t length,
        bool isNotify);

    NimBLEClient *m_pClient = nullptr;
    NimBLERemoteCharacteristic *m_pNotifyCharacteristic = nullptr;
    NimBLERemoteCharacteristic *m_pWriteCharacteristic = nullptr;
    NimBLEAddress m_targetAddress;
    bool m_doConnect = false;

    void onConnect(BLEClient *pClient) override
    {
        Serial.println("Connected to device!");
    }

    void onDisconnect(BLEClient *pClient, int reason) override
    {
        Serial.println("Disconnected! Will attempt reconnect...");
        m_doConnect = true; // Trigger reconnect in loop
    }
};