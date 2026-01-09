#include <Arduino.h>
#include <PubSubClient.h>
#include <MqttDevice.h>
#include "platform.h"
#include "settings.h"
#include "utils.h"

class MqttView
{
public:
    MqttView(PubSubClient *client)
        : m_client(client),
          m_device(composeClientID().c_str(), "PaceKeeper", SYSTEM_NAME, "maker_pt"),
          m_speed(&m_device, "speed", "Speed"),
          m_speedFeedback(&m_device, "speed-feedback", "Speed Feedback"),
          m_state(&m_device, "state", "State"),
          m_distance(&m_device, "distance", "Distance"),
          m_duration(&m_device, "duration", "Duration"),
          m_calories(&m_device, "calories", "Calories"),
          m_steps(&m_device, "steps", "Steps"),
          m_pauseBtn(&m_device, "pause", "Pause"),

          // Configuration Settings
          m_autoreconnectSwitch(&m_device, "auto-reconnect", "Auto Reconnect"),
          // Diagnostics Elements
          m_maxSpeed(&m_device, "max-speed", "Max Speed"),
          m_firmware(&m_device, "firmware", "Firmware Version")

    {

        m_device.setSWVersion(VERSION);

        // further mqtt device config
        m_speed.setCustomStateTopic(m_state.getStateTopic());
        m_speed.setUnit("km/h");
        m_speed.setDeviceClass("speed");
        m_speed.setMin(0.0f);
        m_speed.setMax(6.0f);
        m_speed.setStep(0.1f);
        m_speed.setMode(NumberMode::SLIDER);
        m_speed.setValueTemplate("{{ value_json.speed_cmd }}");

        m_speedFeedback.setCustomStateTopic(m_state.getStateTopic());
        m_speedFeedback.setUnit("km/h");
        m_speedFeedback.setDeviceClass("speed");
        m_speedFeedback.setStateClass(MqttSensor::StateClass::MEASUREMENT);
        m_speedFeedback.setValueTemplate("{{ value_json.speed_feedback }}");

        m_distance.setCustomStateTopic(m_state.getStateTopic());
        m_distance.setUnit("km");
        m_distance.setDeviceClass("distance");
        m_distance.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_distance.setValueTemplate("{{ value_json.distance_km }}");

        m_duration.setCustomStateTopic(m_state.getStateTopic());
        m_duration.setUnit("s");
        m_duration.setDeviceClass("duration");
        m_duration.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_duration.setValueTemplate("{{ value_json.duration_sec }}");

        m_calories.setCustomStateTopic(m_state.getStateTopic());
        m_calories.setUnit("cal");
        m_calories.setDeviceClass("energy");
        m_calories.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_calories.setValueTemplate("{{ value_json.calories }}");

        m_steps.setCustomStateTopic(m_state.getStateTopic());
        m_steps.setUnit("steps");
        m_steps.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_steps.setIcon("mdi:shoe-print");
        m_steps.setValueTemplate("{{ value_json.steps }}");

        m_state.setValueTemplate("{{ value_json.state }}");
        m_state.setIcon("mdi:state-machine");

        m_autoreconnectSwitch.setEntityType(EntityCategory::CONFIG);
        m_autoreconnectSwitch.setIcon("mdi:autorenew");

        m_maxSpeed.setCustomStateTopic(m_state.getStateTopic());
        m_maxSpeed.setEntityType(EntityCategory::DIAGNOSTIC);
        m_maxSpeed.setUnit("km/h");
        m_maxSpeed.setDeviceClass("speed");
        m_maxSpeed.setValueTemplate("{{ value_json.speed_max }}");

        m_firmware.setCustomStateTopic(m_state.getStateTopic());
        m_firmware.setEntityType(EntityCategory::DIAGNOSTIC);
        m_firmware.setValueTemplate("{{ value_json.fw }}");
        m_firmware.setIcon("mdi:chip");

        m_pauseBtn.setIcon("mdi:play-pause");
    }

    MqttDevice &getDevice()
    {
        return m_device;
    }

    const MqttNumber &getSpeed() const
    {
        return m_speed;
    }

    const MqttButton &getPauseButton() const
    {
        return m_pauseBtn;
    }

    const MqttSwitch &getAutoReconnectSwitch() const
    {
        return m_autoreconnectSwitch;
    }

    void publishAllConfigs()
    {
        // Controls
        publishConfig(m_pauseBtn);
        publishConfig(m_speed);

        // Sensors
        publishConfig(m_speedFeedback);
        publishConfig(m_state);
        publishConfig(m_distance);
        publishConfig(m_duration);
        publishConfig(m_calories);

        // TODO: steps are actually not implemented in this type of treadmill
        // publishConfig(m_steps);

        // Configuration
        publishConfig(m_autoreconnectSwitch);

        // Diagnostics
        publishConfig(m_maxSpeed);
        publishConfig(m_firmware);
    }

    void publishAutoReconnectSetting(bool enabled)
    {
        if (enabled)
        {
            publishMqttState(m_autoreconnectSwitch, m_autoreconnectSwitch.getOnState());
        }
        else
        {
            publishMqttState(m_autoreconnectSwitch, m_autoreconnectSwitch.getOffState());
        }
    }

    void publishState(TreadMillData data)
    {
        JsonDocument state;
        state["speed_cmd"] = data.speedCmd;
        state["speed_feedback"] = data.speedFeedback;
        state["speed_max"] = data.speedMax;
        state["distance_km"] = data.distanceKm;
        state["duration_sec"] = data.durationSec;
        state["calories"] = data.calories;
        state["steps"] = data.steps;
        state["fw"] = data.fwVersion;

        switch (data.status)
        {
        case TreadMillData::COUNTDOWN:
            state["state"] = "countdown";
            break;
        case TreadMillData::RUNNING:
            state["state"] = "running";
            break;
        case TreadMillData::PAUSED:
            state["state"] = "paused";
            break;
        case TreadMillData::STOPPED:
            state["state"] = "stopped";
            break;
        case TreadMillData::DISCONNECTED:
            state["state"] = "disconnected";
            break;
        default:
            state["state"] = "unknown";
            break;
        }
        String stateStr;
        serializeJson(state, stateStr);
        publishMqttState(m_state, stateStr.c_str());
    }

private:
    PubSubClient *m_client;

    MqttDevice m_device;

    // Controls
    MqttNumber m_speed;
    MqttButton m_pauseBtn;

    // Sensors
    MqttSensor m_calories;
    MqttSensor m_distance;
    MqttSensor m_duration;
    MqttSensor m_speedFeedback;
    MqttSensor m_state;

    MqttSensor m_steps;

    // Configuration

    MqttSwitch m_autoreconnectSwitch;

    // Diagnostics
    MqttSensor m_maxSpeed;
    MqttSensor m_firmware;

    void publishConfig(MqttEntity &entity)
    {
        String payload = entity.getHomeAssistantConfigPayload();
        char topic[255];
        entity.getHomeAssistantConfigTopic(topic, sizeof(topic));
        if (!m_client->publish(topic, payload.c_str()))
        {
            log_e("Failed to publish config to %s", entity.getStateTopic());
        }
        entity.getHomeAssistantConfigTopicAlt(topic, sizeof(topic));
        if (!m_client->publish(topic, payload.c_str()))
        {
            log_e("Failed to publish config to %s", entity.getStateTopic());
        }
    }

    void publishMqttState(const MqttEntity &entity, const char *state)
    {
        if (!m_client->publish(entity.getStateTopic(), state))
        {
            log_e("Failed to publish state to %s", entity.getStateTopic());
        }
    }
};