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
          m_calories(&m_device, "calories", "Calories"),
          m_steps(&m_device, "steps", "Steps"),
          m_pauseBtn(&m_device, "pause", "Pause"),
          // Configuration Settings

          m_autoreconnectSwitch(&m_device, "auto-reconnect", "Auto Reconnect")
    // Diagnostics Elements
    {

        m_device.setSWVersion(VERSION);

        // further mqtt device config
        m_speed.setUnit("km/h");
        m_speed.setMin(0.0f);
        m_speed.setMax(6.0f);
        m_speed.setStep(0.1f);
        m_speed.setMode(NumberMode::SLIDER);
        m_speed.setCustomStateTopic(m_state.getStateTopic());
        m_speed.setIcon("mdi:speedometer");
        m_speed.setValueTemplate("{{ value_json.speed_cmd }}");

        m_speedFeedback.setUnit("km/h");
        m_speedFeedback.setStateClass(MqttSensor::StateClass::MEASUREMENT);
        m_speedFeedback.setCustomStateTopic(m_state.getStateTopic());
        m_speedFeedback.setIcon("mdi:walk");
        m_speedFeedback.setValueTemplate("{{ value_json.speed_feedback }}");

        m_distance.setUnit("km");
        m_distance.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_distance.setCustomStateTopic(m_state.getStateTopic());
        m_distance.setIcon("mdi:map-marker-distance");
        m_distance.setValueTemplate("{{ value_json.distance_km }}");

        m_calories.setUnit("kcal");
        m_calories.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_calories.setCustomStateTopic(m_state.getStateTopic());
        m_calories.setIcon("mdi:fire");
        m_calories.setValueTemplate("{{ value_json.calories }}");

        m_steps.setUnit("steps");
        m_steps.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_steps.setCustomStateTopic(m_state.getStateTopic());
        m_steps.setIcon("mdi:shoe-print");
        m_steps.setValueTemplate("{{ value_json.steps }}");

        m_state.setValueTemplate("{{ value_json.state }}");
        m_state.setIcon("mdi:state-machine");

        m_autoreconnectSwitch.setEntityType(EntityCategory::CONFIG);
        m_autoreconnectSwitch.setIcon("mdi:autorenew");

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
        publishConfig(m_speed);
        publishConfig(m_speedFeedback);
        publishConfig(m_state);

        publishConfig(m_distance);
        publishConfig(m_calories);
        publishConfig(m_steps);

        publishConfig(m_pauseBtn);
        publishConfig(m_autoreconnectSwitch);
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
        state["distance_km"] = data.distanceKm;
        state["calories"] = data.calories;
        state["steps"] = data.steps;


        switch (data.status)
        {
        case TreadMillData::STARTING:
            state["state"] = "starting";
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
    MqttNumber m_speed;
    MqttSensor m_speedFeedback;

    MqttSensor m_distance;
    MqttSensor m_calories;
    MqttSensor m_steps;

    MqttSensor m_state;
    MqttButton m_pauseBtn;
    MqttSwitch m_autoreconnectSwitch;

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

    void publishMqttConfigState(MqttEntity &entity, const uint32_t value)
    {
        char state[9];
        snprintf(state, sizeof(state), "%08x", value);
        if (!m_client->publish(entity.getStateTopic(), state))
        {
            log_e("Failed to publish state to %s", entity.getStateTopic());
        }
    }

    void publishMqttFloat(const MqttEntity &entity, const float value)
    {
        char state[16];
        snprintf(state, sizeof(state), "%.3f", value);
        publishMqttState(entity, state);
    }

    void publishMqttCounterState(MqttEntity &entity, const uint32_t value)
    {
        char state[9];
        snprintf(state, sizeof(state), "%u", value);
        m_client->publish(entity.getStateTopic(), state);
    }

    void publishMqttState(const MqttEntity &entity, const char *state)
    {
        if (!m_client->publish(entity.getStateTopic(), state))
        {
            log_e("Failed to publish state to %s", entity.getStateTopic());
        }
    }
};