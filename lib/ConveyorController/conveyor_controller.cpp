#include "conveyor_controller.hpp"

ConveyorController::ConveyorController(const ConveyorConfig& config)
    : config_(config),
      motor_(
          config.rpwmPin,
          config.enPin,
          DcMotorBts7960::UNUSED_PIN,
          config.currentSensePin),
      currentPwm_(0),
      desiredPwm_(0),
      overcurrentStartMs_(0),
      inOvercurrent_(false),
      running_(false),
      stalled_(false)
{
}

void ConveyorController::init()
{
    motor_.init();
    currentPwm_         = 0;
    desiredPwm_         = 0;
    overcurrentStartMs_ = 0;
    inOvercurrent_      = false;
    running_            = false;
    stalled_            = false;
}

void ConveyorController::start()
{
    if (stalled_)
    {
        return;
    }

    running_    = true;
    desiredPwm_ = config_.targetPwm;
}

void ConveyorController::pauseSmooth()
{
    running_       = false;
    desiredPwm_    = 0;
    inOvercurrent_ = false;
}

void ConveyorController::stop()
{
    running_       = false;
    desiredPwm_    = 0;
    currentPwm_    = 0;
    inOvercurrent_ = false;
    motor_.stop();
}

void ConveyorController::brake()
{
    running_       = false;
    desiredPwm_    = 0;
    currentPwm_    = 0;
    inOvercurrent_ = false;
    motor_.brake();
}

void ConveyorController::update()
{
    if (stalled_)
    {
        return;
    }

    // 1. Generador de perfil trapezoidal de velocidad (rampa de aceleración / crucero / desaceleración)
    if (currentPwm_ < desiredPwm_)
    {
        const uint8_t step = (config_.accelStepPwm > 0U) ? config_.accelStepPwm : 1U;
        const uint16_t nextPwm = static_cast<uint16_t>(currentPwm_) + step;
        currentPwm_ = (nextPwm >= desiredPwm_) ? desiredPwm_ : static_cast<uint8_t>(nextPwm);
        motor_.driveForward(currentPwm_);
    }
    else if (currentPwm_ > desiredPwm_)
    {
        const uint8_t step = (config_.decelStepPwm > 0U) ? config_.decelStepPwm : 1U;
        if (currentPwm_ > static_cast<uint8_t>(desiredPwm_ + step))
        {
            currentPwm_ = static_cast<uint8_t>(currentPwm_ - step);
            motor_.driveForward(currentPwm_);
        }
        else
        {
            currentPwm_ = desiredPwm_;
            if (currentPwm_ == 0U)
            {
                motor_.brake();
            }
            else
            {
                motor_.driveForward(currentPwm_);
            }
        }
    }

    // 2. Supervisión de sobrecorriente (IS) mientras el motor está energizado
    if (currentPwm_ == 0U)
    {
        inOvercurrent_ = false;
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
            stalled_       = true;
            running_       = false;
            desiredPwm_    = 0;
            currentPwm_    = 0;
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
    stalled_       = false;
    inOvercurrent_ = false;
}

uint8_t ConveyorController::getCurrentPwm() const
{
    return currentPwm_;
}

uint16_t ConveyorController::getFilteredCurrentAdc() const
{
    return motor_.getFilteredCurrentAdc();
}

bool ConveyorController::isRunning() const
{
    return running_;
}
