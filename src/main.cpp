#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

#include <WiFi.h>
#include <mdns.h>
// watch dog
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <MqttDevice.h>

// #include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "platform.h"
#include "TreadmillHandler.h"
#include "mqttview.h"

const uint WATCHDOG_TIMEOUT_S = 300;
const uint WIFI_DISCONNECT_FORCED_RESTART_S = 60;

WiFiClient net;
PubSubClient client(net);
MqttView g_mqttView(&client);

bool g_wifiConnected = false;
bool g_mqttConnected = false;
unsigned long g_lastWifiConnect = 0;

String g_bssid = "";

TreadmillHandler treadmill;
bool connectToMqtt()
{
  if (client.connected())
  {
    return true;
  }

  log_i("Connecting to MQTT...");
  if (strlen(MQTT_USER) == 0)
  {
    if (!client.connect(composeClientID().c_str()))
    {
      return false;
    }
  }
  else
  {
    if (!client.connect(composeClientID().c_str(), MQTT_USER, MQTT_PASS))
    {
      return false;
    }
  }

  client.subscribe(g_mqttView.getSpeed().getCommandTopic(), 1);
  client.subscribe(g_mqttView.getPauseButton().getCommandTopic(), 1);
  client.subscribe(g_mqttView.getAutoReconnectSwitch().getCommandTopic(), 1);

  client.subscribe(HOMEASSISTANT_STATUS_TOPIC);
  client.subscribe(HOMEASSISTANT_STATUS_TOPIC_ALT);

  g_mqttView.publishAllConfigs();
  g_mqttView.publishState(treadmill.getLastData());
  g_mqttView.publishAutoReconnectSetting(treadmill.getAutoReconnect());

  return true;
}

bool connectToWifi()
{
  return WiFi.status() == WL_CONNECTED;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  log_i("Message arrived [%s]", topic);
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, g_mqttView.getSpeed().getCommandTopic()) == 0)
  {
    float data = atof((char *)payload);
    uint16_t speed = (uint16_t)(data * 1000);
    log_i("Setting speed to %.2f km/h (%u)", data, speed);

    if (speed <= 100)
    {
      treadmill.stop();
      return;
    }
    if (speed > 6000)
    {
      speed = 6000;
    }
    treadmill.setSpeed(speed);
  }
  else if (strcmp(topic, g_mqttView.getPauseButton().getCommandTopic()) == 0)
  {
    String command = String((char *)payload).substring(0, length);
    command.trim();
    log_i("Pause command received: %s", command.c_str());
    if (command.equalsIgnoreCase("press"))
    {
      if (treadmill.getLastData().status == TreadMillData::RUNNING)
        treadmill.pause();
      else if (treadmill.getLastData().status == TreadMillData::PAUSED)
        treadmill.start();
    }
  }
  else if (strcmp(topic, g_mqttView.getAutoReconnectSwitch().getCommandTopic()) == 0)
  {
    String command = String((char *)payload).substring(0, length);
    command.trim();
    log_i("Auto Reconnect command received: %s", command.c_str());
    if (command.equalsIgnoreCase(g_mqttView.getAutoReconnectSwitch().getOnState()))
    {
      treadmill.setAutoReconnect(true);
      g_mqttView.publishAutoReconnectSetting(true);
    }
    else if (command.equalsIgnoreCase(g_mqttView.getAutoReconnectSwitch().getOffState()))
    {
      treadmill.setAutoReconnect(false);
      g_mqttView.publishAutoReconnectSetting(false);
    }
  }

  // publish config when homeassistant comes online and needs the configuration again
  else if (strcmp(topic, HOMEASSISTANT_STATUS_TOPIC) == 0 ||
           strcmp(topic, HOMEASSISTANT_STATUS_TOPIC_ALT) == 0)
  {
    if (strncmp((char *)payload, "online", length) == 0)
    {
      g_mqttView.publishAllConfigs();
      g_mqttView.publishState(treadmill.getLastData());
      g_mqttView.publishAutoReconnectSetting(treadmill.getAutoReconnect());
    }
  }
}

// --- Setup ---
void setup()
{
  // initialize watchdog
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                      // add current thread to WDT watch

  Serial.begin(115200);

  WiFi.setHostname(composeClientID().c_str());
  WiFi.mode(WIFI_STA);

  // select the AP with the strongest signal
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  WiFi.begin(DEFAULT_STA_WIFI_SSID, DEFAULT_STA_WIFI_PASS);

  log_i("Connecting to wifi...");
  while (!connectToWifi())
  {
    log_d(".");
    delay(500);
  }
  g_wifiConnected = true;
  g_lastWifiConnect = millis();
  log_i("Connected to SSID: %s", DEFAULT_STA_WIFI_SSID);
  log_i("IP address: %s", WiFi.localIP().toString().c_str());

  char configUrl[256];
  snprintf(configUrl, sizeof(configUrl), "http://%s/", WiFi.localIP().toString().c_str());
  g_mqttView.getDevice().setConfigurationUrl(configUrl);
  client.setBufferSize(1024);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  NimBLEAddress targetAddress(std::string(TARGET_ADDRESS), BLE_ADDR_PUBLIC);
  treadmill.begin(targetAddress);

  treadmill.setCallback([](const TreadMillData &data)
                        {
    log_d("Speed: %.2f km/h, Distance: %.2f %d", data.speedCmd, data.distanceKm, data.status);
    g_mqttView.publishState(data); });

  log_i("Starting BLE Client...");
  NimBLEDevice::init("PaceKeeper");

  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    log_i("Start updating %s", type.c_str()); });
  ArduinoOTA.onEnd([]()
                   { log_i("End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
// reset watchdog during update
#ifdef ESP32
            esp_task_wdt_reset();
#endif
        log_i("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    log_e("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      log_e("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      log_e("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      log_e("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      log_e("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      log_e("End Failed");
    } });
  ArduinoOTA.begin();
}
int currentspeed = 1000;
// --- Loop ---
void loop()
{
  // reset watchdog, important to be called once each loop.
  esp_task_wdt_reset();

  bool wifiConnected = connectToWifi();
  if (!wifiConnected)
  {
    if (g_wifiConnected)
    {
    }
    if (millis() - g_lastWifiConnect > WIFI_DISCONNECT_FORCED_RESTART_S * 1000)
    {
      log_w("Wifi could not connect in time, will force a restart");
      ESP.restart();
    }
    g_wifiConnected = false;
    g_mqttConnected = false;
    delay(1000);
    return;
  }
  g_wifiConnected = true;
  g_lastWifiConnect = millis();

  ArduinoOTA.handle();

  bool mqttConnected = connectToMqtt();
  if (!mqttConnected)
  {
    if (g_mqttConnected)
    {
      // we switched to disconnected
    }
    g_mqttConnected = false;
    delay(1000);
    return;
  }
  if (!g_mqttConnected)
  {
    // now we are successfully reconnected and publish our counters
    g_bssid = WiFi.BSSIDstr();
    // g_mqttView.publishDiagnostics(g_settings, g_bssid.c_str());
  }
  g_mqttConnected = true;

  client.loop();
  treadmill.handle();
  if (treadmill.isConnected() && false)
  {
    log_d("Client connected, sending start command...");

    currentspeed += 100;
    if (currentspeed > 2500)
    {
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
