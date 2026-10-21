#include "dc_motor_bts7960.hpp"

DcMotorBts7960::DcMotorBts7960(
    uint8_t rpwmPin,
    uint8_t enPin,
    uint8_t lpwmPin,
    uint8_t currentSensePin)
    : rpwmPin_(rpwmPin),
      enPin_(enPin),
      lpwmPin_(lpwmPin),
      currentSensePin_(currentSensePin),
      currentPwm_(0),
      enabled_(false),
      currentSamples_{0},
      sampleIndex_(0),
      filteredAdc_(0)
{
}

void DcMotorBts7960::init()
{
    // Asegurar salidas en nivel bajo antes de habilitar direccion OUTPUT
    digitalWrite(enPin_, LOW);
    digitalWrite(rpwmPin_, LOW);
    pinMode(enPin_, OUTPUT);
    pinMode(rpwmPin_, OUTPUT);

    if (lpwmPin_ != UNUSED_PIN)
    {
        digitalWrite(lpwmPin_, LOW);
        pinMode(lpwmPin_, OUTPUT);
    }

    if (currentSensePin_ != UNUSED_PIN)
    {
        pinMode(currentSensePin_, INPUT);
    }

    stop();
}

void DcMotorBts7960::driveForward(uint8_t pwm)
{
    if (pwm == 0)
    {
        brake();
        return;
    }

    // Garantizar que LPWM quede en cero antes de modular RPWM
    if (lpwmPin_ != UNUSED_PIN)
    {
        analogWrite(lpwmPin_, 0);
    }

    digitalWrite(enPin_, HIGH);
    analogWrite(rpwmPin_, pwm);
    currentPwm_ = pwm;
    enabled_ = true;
}

void DcMotorBts7960::driveReverse(uint8_t pwm)
{
    if (lpwmPin_ == UNUSED_PIN)
    {
        return;
    }

    if (pwm == 0)
    {
        brake();
        return;
    }

    // Garantizar que RPWM quede en cero antes de modular LPWM
    analogWrite(rpwmPin_, 0);
    digitalWrite(enPin_, HIGH);
    analogWrite(lpwmPin_, pwm);
    currentPwm_ = pwm;
    enabled_ = true;
}

void DcMotorBts7960::brake()
{
    analogWrite(rpwmPin_, 0);
    if (lpwmPin_ != UNUSED_PIN)
    {
        analogWrite(lpwmPin_, 0);
    }

    // En el BTS7960, EN = HIGH con ambos PWM en LOW activa el frenado por rama inferior
    digitalWrite(enPin_, HIGH);
    currentPwm_ = 0;
    enabled_ = true;
}

void DcMotorBts7960::stop()
{
    analogWrite(rpwmPin_, 0);
    if (lpwmPin_ != UNUSED_PIN)
    {
        analogWrite(lpwmPin_, 0);
    }

    digitalWrite(enPin_, LOW);
    currentPwm_ = 0;
    enabled_ = false;
}

uint16_t DcMotorBts7960::sampleCurrentAdc()
{
    if (currentSensePin_ == UNUSED_PIN)
    {
        return 0;
    }

    currentSamples_[sampleIndex_] = static_cast<uint16_t>(analogRead(currentSensePin_));
    sampleIndex_ = static_cast<uint8_t>((sampleIndex_ + 1U) % CURRENT_FILTER_SAMPLES);

    uint32_t sum = 0;
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; ++i)
    {
        sum += currentSamples_[i];
    }

    filteredAdc_ = static_cast<uint16_t>(sum / CURRENT_FILTER_SAMPLES);
    return filteredAdc_;
}

uint16_t DcMotorBts7960::getFilteredCurrentAdc() const
{
    return filteredAdc_;
}

uint8_t DcMotorBts7960::getCurrentPwm() const
{
    return currentPwm_;
}

bool DcMotorBts7960::isRunning() const
{
    return (enabled_ && (currentPwm_ > 0));
}
