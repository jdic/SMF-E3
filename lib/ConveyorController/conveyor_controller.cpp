#include "conveyor_controller.hpp"

ConveyorController::ConveyorController(const ConveyorConfig& config)
    : config_(config),
      motor_(
          config.rpwmPin,
          config.enPin,
          DcMotorBts7960::UNUSED_PIN,
          config.currentSensePin),
      overcurrentStartMs_(0),
      inOvercurrent_(false),
      running_(false),
      stalled_(false)
{
}

void ConveyorController::init()
{
    motor_.init();
    overcurrentStartMs_ = 0;
    inOvercurrent_ = false;
    running_ = false;
    stalled_ = false;
}

void ConveyorController::start()
{
    if (stalled_)
    {
        return;
    }

    if (!running_)
    {
        running_ = true;
        inOvercurrent_ = false;
        overcurrentStartMs_ = millis();
        motor_.driveForward(config_.targetPwm);
    }
}

void ConveyorController::stop()
{
    running_ = false;
    inOvercurrent_ = false;
    motor_.stop();
}

void ConveyorController::brake()
{
    running_ = false;
    inOvercurrent_ = false;
    motor_.brake();
}

void ConveyorController::update()
{
    if (!running_ || stalled_)
    {
        return;
    }

    const uint16_t currentAdc = motor_.sampleCurrentAdc();
    const uint32_t nowMs = millis();

    if (currentAdc >= config_.currentLimitAdc)
    {
        if (!inOvercurrent_)
        {
            inOvercurrent_ = true;
            overcurrentStartMs_ = nowMs;
        }
        else if (nowMs - overcurrentStartMs_ >= config_.stallTimeMs)
        {
            stalled_ = true;
            running_ = false;
            inOvercurrent_ = false;
            motor_.stop();
        }
    }
    else
    {
        inOvercurrent_ = false;
    }
}

bool ConveyorController::isStalled() const
{
    return stalled_;
}

void ConveyorController::clearStall()
{
    stalled_ = false;
    inOvercurrent_ = false;
}

uint8_t ConveyorController::getCurrentPwm() const
{
    return motor_.getCurrentPwm();
}

uint16_t ConveyorController::getFilteredCurrentAdc() const
{
    return motor_.getFilteredCurrentAdc();
}

bool ConveyorController::isRunning() const
{
    return running_;
}
