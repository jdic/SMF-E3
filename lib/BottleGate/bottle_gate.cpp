#include "bottle_gate.hpp"

BottleGate::BottleGate(const BottleGateConfig& config)
    : config_(config),
      stopServo_(),
      clampServo_(),
      actionStartMs_(0),
      pendingStopArmUp_(false),
      settled_(true)
{
}

void BottleGate::init()
{
    // Precargar angulos de reposo antes de attach() para evitar barridos bruscos al energizar
    stopServo_.write(resolveStopAngle(config_.stopUpDeg));
    clampServo_.write(resolveClampAngle(config_.clampUpDeg));

    stopServo_.attach(config_.stopPin);
    clampServo_.attach(config_.clampPin);

    raiseArmsImmediate();
}

void BottleGate::lowerStopArm()
{
    pendingStopArmUp_ = false;
    stopServo_.write(resolveStopAngle(config_.stopDownDeg));
    actionStartMs_ = millis();
    settled_ = false;
}

void BottleGate::raiseStopArm()
{
    pendingStopArmUp_ = false;
    stopServo_.write(resolveStopAngle(config_.stopUpDeg));
    actionStartMs_ = millis();
    settled_ = false;
}

void BottleGate::lowerClampArm()
{
    pendingStopArmUp_ = false;
    clampServo_.write(resolveClampAngle(config_.clampDownDeg));
    actionStartMs_ = millis();
    settled_ = false;
}

void BottleGate::raiseClampArm()
{
    pendingStopArmUp_ = false;
    clampServo_.write(resolveClampAngle(config_.clampUpDeg));
    actionStartMs_ = millis();
    settled_ = false;
}

void BottleGate::releaseBottle()
{
    // Etapa 1: levantar el brazo de sujecion y aguardar releaseGapMs antes de levantar el tope
    clampServo_.write(resolveClampAngle(config_.clampUpDeg));
    actionStartMs_ = millis();
    pendingStopArmUp_ = true;
    settled_ = false;
}

void BottleGate::raiseArmsImmediate()
{
    pendingStopArmUp_ = false;
    clampServo_.write(resolveClampAngle(config_.clampUpDeg));
    stopServo_.write(resolveStopAngle(config_.stopUpDeg));
    actionStartMs_ = millis();
    settled_ = false;
}

void BottleGate::update()
{
    const uint32_t nowMs = millis();

    if (pendingStopArmUp_)
    {
        // Etapa 2 de releaseBottle: levantar el brazo de tope tras cumplirse releaseGapMs
        if ((nowMs - actionStartMs_) >= config_.releaseGapMs)
        {
            stopServo_.write(resolveStopAngle(config_.stopUpDeg));
            actionStartMs_ = nowMs;
            pendingStopArmUp_ = false;
        }
        return;
    }

    if (!settled_)
    {
        if ((nowMs - actionStartMs_) >= config_.armMoveMs)
        {
            settled_ = true;
        }
    }
}

bool BottleGate::isSettled() const
{
    return (!pendingStopArmUp_ && settled_);
}

uint8_t BottleGate::resolveStopAngle(uint8_t deg) const
{
    const uint8_t clamped = (deg > 180U) ? 180U : deg;
    return config_.stopReverse ? static_cast<uint8_t>(180U - clamped) : clamped;
}

uint8_t BottleGate::resolveClampAngle(uint8_t deg) const
{
    const uint8_t clamped = (deg > 180U) ? 180U : deg;
    return config_.clampReverse ? static_cast<uint8_t>(180U - clamped) : clamped;
}
