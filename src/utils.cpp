#include "utils.h"
#include <LittleFS.h>

String composeClientID()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String reducedMac;

    for (int i = 3; i < 6; ++i)
    {
        reducedMac += String(mac[i], 16);
    }
    String clientId = "pacekeeper-";
    clientId += reducedMac;
    return clientId;
}

