#include "digital_sensor.hpp"

DigitalSensor::DigitalSensor(
    uint8_t pin,
    uint16_t debounceMs,
    bool activeHigh,
    bool usePullup)
    : pin_(pin),
      debounceMs_(debounceMs),
      activeHigh_(activeHigh),
      usePullup_(usePullup),
      stableState_(false),
      lastRawState_(false),
      changed_(false),
      risingEdge_(false),
      fallingEdge_(false),
      lastDebounceTimeMs_(0)
{
}

void DigitalSensor::init()
{
    pinMode(pin_, usePullup_ ? INPUT_PULLUP : INPUT);

    const bool initialTriggered = readRawTriggered();
    stableState_ = initialTriggered;
    lastRawState_ = initialTriggered;
    changed_ = false;
    risingEdge_ = false;
    fallingEdge_ = false;
    lastDebounceTimeMs_ = millis();
}

void DigitalSensor::update()
{
    changed_ = false;
    risingEdge_ = false;
    fallingEdge_ = false;

    const bool currentRaw = readRawTriggered();
    const uint32_t nowMs = millis();

    // Reiniciar ventana temporal si la lectura cruda oscila por rebote o ruido
    if (currentRaw != lastRawState_)
    {
        lastDebounceTimeMs_ = nowMs;
        lastRawState_ = currentRaw;
    }

    // Consolidar nuevo estado estable al cumplirse el tiempo de guarda
    if ((nowMs - lastDebounceTimeMs_) >= debounceMs_)
    {
        if (currentRaw != stableState_)
        {
            stableState_ = currentRaw;
            changed_ = true;
            risingEdge_ = stableState_;
            fallingEdge_ = !stableState_;
        }
    }
}

bool DigitalSensor::isTriggered() const
{
    return stableState_;
}

bool DigitalSensor::hasChanged() const
{
    return changed_;
}

bool DigitalSensor::onRisingEdge() const
{
    return risingEdge_;
}

bool DigitalSensor::onFallingEdge() const
{
    return fallingEdge_;
}

bool DigitalSensor::readRawTriggered() const
{
    const bool pinHigh = (digitalRead(pin_) == HIGH);
    return activeHigh_ ? pinHigh : !pinHigh;
}
