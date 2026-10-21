#include "conveyor_controller.hpp"

ConveyorController::ConveyorController(const ConveyorConfig& config)
    : config_(config),
      motor_(config.rpwmPin, config.enPin),
      encoder_(EncoderSlot::Conveyor, config.encPinA, config.encPinB, config.encoderPpr, true),
      integralScaled_(0),
      lastStallPulses_(0),
      lastPulseTimeMs_(0),
      running_(false),
      stalled_(false)
{
}

void ConveyorController::init()
{
    motor_.init();
    encoder_.init();
    stop();
}

void ConveyorController::start()
{
    if (running_ || stalled_)
    {
        return;
    }

    encoder_.reset();
    integralScaled_ = 0;
    lastStallPulses_ = encoder_.getPulses();
    lastPulseTimeMs_ = millis();
    running_ = true;
}

void ConveyorController::stop()
{
    running_ = false;
    integralScaled_ = 0;
    motor_.stop();
}

void ConveyorController::brake()
{
    running_ = false;
    integralScaled_ = 0;
    motor_.brake();
}

void ConveyorController::update()
{
    const uint16_t measuredRpm = encoder_.updateSpeed();

    if (!running_)
    {
        return;
    }

    // 1. Calculo del lazo PI en punto fijo (ganancias escaladas por PI_SCALE = 10)
    const int32_t errorRpm = static_cast<int32_t>(config_.targetRpm) - static_cast<int32_t>(measuredRpm);
    const int32_t pTermScaled = errorRpm * static_cast<int32_t>(config_.kpTenths);

    integralScaled_ += errorRpm * static_cast<int32_t>(config_.kiTenths);
    const int32_t maxIntegralScaled = MAX_PWM * PI_SCALE;
    if (integralScaled_ > maxIntegralScaled)
    {
        integralScaled_ = maxIntegralScaled;
    }
    else if (integralScaled_ < 0)
    {
        integralScaled_ = 0;
    }

    int32_t pwmOut = (pTermScaled + integralScaled_) / PI_SCALE;
    if (pwmOut > MAX_PWM)
    {
        pwmOut = MAX_PWM;
    }
    else if (pwmOut < MIN_ACTIVE_PWM)
    {
        pwmOut = MIN_ACTIVE_PWM;
    }

    motor_.driveForward(static_cast<uint8_t>(pwmOut));

    // 2. Vigilancia de atasco mecanico (ERR-303) cuando el PWM supera el umbral minimo
    const uint32_t nowMs = millis();
    const int32_t currentPulses = encoder_.getPulses();

    if (motor_.getCurrentPwm() >= config_.stallMinPwm)
    {
        if (currentPulses != lastStallPulses_)
        {
            lastStallPulses_ = currentPulses;
            lastPulseTimeMs_ = nowMs;
        }
        else if ((nowMs - lastPulseTimeMs_) >= config_.stallTimeMs)
        {
            stalled_ = true;
            stop();
        }
    }
    else
    {
        lastStallPulses_ = currentPulses;
        lastPulseTimeMs_ = nowMs;
    }
}

bool ConveyorController::isStalled() const
{
    return stalled_;
}

void ConveyorController::clearStall()
{
    stalled_ = false;
}

uint16_t ConveyorController::getSpeedRpm() const
{
    return encoder_.getSpeedRpm();
}

bool ConveyorController::isRunning() const
{
    return running_;
}
