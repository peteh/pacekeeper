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
          m_device(composeClientID().c_str(), "PaceMaker", SYSTEM_NAME, "maker_pt"),
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

        m_speedFeedback.setCustomStateTopic(m_speed.getStateTopic());
        m_speedFeedback.setUnit("km/h");
        m_speedFeedback.setStateClass(MqttSensor::StateClass::MEASUREMENT);

        m_distance.setUnit("km");
        m_distance.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_calories.setUnit("kcal");
        m_calories.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);
        m_steps.setUnit("steps");
        m_steps.setStateClass(MqttSensor::StateClass::TOTAL_INCREASING);

        m_autoreconnectSwitch.setEntityType(EntityCategory::CONFIG);
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

    void publishSpeed(float speed)
    {
        char speed_str[16];
        snprintf(speed_str, sizeof(speed_str), "%.2f", speed);
        publishMqttState(m_speed, speed_str);
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
        publishSpeed(data.speedKph);
        publishMqttState(m_speedFeedback, String(data.speedKph, 2).c_str());
        publishMqttState(m_distance, String(data.distanceKm, 2).c_str());
        publishMqttCounterState(m_calories, data.calories);
        publishMqttCounterState(m_steps, data.steps);

        switch (data.status)
        {
        case TreadMillData::STARTING:
            publishMqttState(m_state, "starting");
            break;
        case TreadMillData::RUNNING:
            publishMqttState(m_state, "running");
            break;
        case TreadMillData::PAUSED:
            publishMqttState(m_state, "paused");
            break;
        case TreadMillData::STOPPED:
            publishMqttState(m_state, "stopped");
            break;
        default:
            publishMqttState(m_state, "unknown");
            break;
        }
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