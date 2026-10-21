#include "fill_pump.hpp"

FillPump::FillPump(uint8_t pin, uint16_t maxOnTimeMs, bool activeHigh)
    : pin_(pin),
      maxOnTimeMs_(maxOnTimeMs),
      activeHigh_(activeHigh),
      isOn_(false),
      onStartTimeMs_(0)
{
}

void FillPump::init()
{
    // Escribir estado inactivo antes de habilitar la salida para evitar pulsos espurios
    writeOutput(false);
    pinMode(pin_, OUTPUT);
    off();
}

void FillPump::on()
{
    if (!isOn_)
    {
        onStartTimeMs_ = millis();
        isOn_ = true;
        writeOutput(true);
    }
}

void FillPump::off()
{
    isOn_ = false;
    writeOutput(false);
}

void FillPump::update()
{
    if (isOn_)
    {
        // Corte autonomo si el tiempo de bombeo supera el limite absoluto de seguridad
        if ((millis() - onStartTimeMs_) >= maxOnTimeMs_)
        {
            off();
        }
    }
}

bool FillPump::isOn() const
{
    return isOn_;
}

void FillPump::writeOutput(bool enable)
{
    const bool pinHigh = activeHigh_ ? enable : !enable;
    digitalWrite(pin_, pinHigh ? HIGH : LOW);
}
