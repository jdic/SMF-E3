#include "dc_motor_bts7960.hpp"
#include <avr/interrupt.h>

namespace
{
    /**
     * @brief Canal de modulación PWM asistido por Timer4 (976 Hz) para pines cuyo temporizador
     *        nativo (Timer5 en pines 44, 45 y 46) está ocupado por la librería Servo.
     */
    struct Timer4PwmChannel
    {
        uint8_t           pin;
        volatile uint8_t* portOut;
        uint8_t           bitMask;
    };

    constexpr uint8_t MAX_TIMER4_CHANNELS = 2;
    Timer4PwmChannel  g_timer4Channels[MAX_TIMER4_CHANNELS] =
    {
        { DcMotorBts7960::UNUSED_PIN, nullptr, 0 },
        { DcMotorBts7960::UNUSED_PIN, nullptr, 0 }
    };
    bool g_timer4Initialized = false;

    bool usesTimer5Pin(uint8_t pin)
    {
        return (pin == 44U) || (pin == 45U) || (pin == 46U);
    }

    void ensureTimer4Init()
    {
        if (g_timer4Initialized)
        {
            return;
        }

        const uint8_t sreg = SREG;
        cli();

        // Timer4 en Fast PWM de 8 bits (WGM43:0 = 0101, TOP = 0x00FF) con prescaler 64 (~976.5 Hz)
        TCCR4A = _BV(WGM40);
        TCCR4B = _BV(WGM42) | _BV(CS41) | _BV(CS40);
        TCNT4  = 0;
        TIMSK4 = 0;
        TIFR4  = _BV(OCF4B) | _BV(OCF4C) | _BV(TOV4);

        g_timer4Initialized = true;
        SREG = sreg;
    }

    int8_t findOrAllocateTimer4Channel(uint8_t pin)
    {
        for (uint8_t i = 0; i < MAX_TIMER4_CHANNELS; ++i)
        {
            if (g_timer4Channels[i].pin == pin)
            {
                return static_cast<int8_t>(i);
            }
        }

        for (uint8_t i = 0; i < MAX_TIMER4_CHANNELS; ++i)
        {
            if (g_timer4Channels[i].pin == DcMotorBts7960::UNUSED_PIN)
            {
                const uint8_t port = digitalPinToPort(pin);
                if (port == NOT_A_PIN)
                {
                    return -1;
                }
                g_timer4Channels[i].pin     = pin;
                g_timer4Channels[i].portOut = portOutputRegister(port);
                g_timer4Channels[i].bitMask = digitalPinToBitMask(pin);
                return static_cast<int8_t>(i);
            }
        }

        return -1;
    }

    void writePwmPin(uint8_t pin, uint8_t pwm)
    {
        if (!usesTimer5Pin(pin))
        {
            analogWrite(pin, pwm);
            return;
        }

        ensureTimer4Init();
        const int8_t ch = findOrAllocateTimer4Channel(pin);
        if (ch < 0)
        {
            digitalWrite(pin, (pwm >= 128U) ? HIGH : LOW);
            return;
        }

        const uint8_t sreg = SREG;
        cli();

        // Desconectar salida OC5x del Timer5 por si analogWrite fue invocado previamente
        if (pin == 44U)
        {
            TCCR5A &= static_cast<uint8_t>(~_BV(COM5C1));
        }
        else if (pin == 45U)
        {
            TCCR5A &= static_cast<uint8_t>(~_BV(COM5B1));
        }
        else if (pin == 46U)
        {
            TCCR5A &= static_cast<uint8_t>(~_BV(COM5A1));
        }

        const uint8_t ieMask = (ch == 0) ? _BV(OCIE4B) : _BV(OCIE4C);

        if (pwm == 0U)
        {
            TIMSK4 &= static_cast<uint8_t>(~ieMask);
            *g_timer4Channels[ch].portOut &= static_cast<uint8_t>(~g_timer4Channels[ch].bitMask);
        }
        else if (pwm == 255U)
        {
            TIMSK4 &= static_cast<uint8_t>(~ieMask);
            *g_timer4Channels[ch].portOut |= g_timer4Channels[ch].bitMask;
        }
        else
        {
            if (ch == 0)
            {
                OCR4B = pwm;
            }
            else
            {
                OCR4C = pwm;
            }
            TIMSK4 |= static_cast<uint8_t>(_BV(TOIE4) | ieMask);
        }

        if ((TIMSK4 & (_BV(OCIE4B) | _BV(OCIE4C))) == 0U)
        {
            TIMSK4 &= static_cast<uint8_t>(~_BV(TOIE4));
        }

        SREG = sreg;
    }
}

ISR(TIMER4_OVF_vect)
{
    if ((TIMSK4 & _BV(OCIE4B)) != 0U && g_timer4Channels[0].portOut != nullptr)
    {
        *g_timer4Channels[0].portOut |= g_timer4Channels[0].bitMask;
    }
    if ((TIMSK4 & _BV(OCIE4C)) != 0U && g_timer4Channels[1].portOut != nullptr)
    {
        *g_timer4Channels[1].portOut |= g_timer4Channels[1].bitMask;
    }
}

ISR(TIMER4_COMPB_vect)
{
    if (g_timer4Channels[0].portOut != nullptr)
    {
        *g_timer4Channels[0].portOut &= static_cast<uint8_t>(~g_timer4Channels[0].bitMask);
    }
}

ISR(TIMER4_COMPC_vect)
{
    if (g_timer4Channels[1].portOut != nullptr)
    {
        *g_timer4Channels[1].portOut &= static_cast<uint8_t>(~g_timer4Channels[1].bitMask);
    }
}

DcMotorBts7960::DcMotorBts7960(
    uint8_t rpwmPin,
    uint8_t enPin,
    uint8_t lpwmPin,
    uint8_t currentSensePin)
    : rpwmPin_(rpwmPin),
      enPin_(enPin),
      lpwmPin_(lpwmPin),
      currentSensePin_(currentSensePin),
      activePwmPin_(UNUSED_PIN),
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
        writePwmPin(lpwmPin_, 0);
    }

    digitalWrite(enPin_, HIGH);
    writePwmPin(rpwmPin_, pwm);
    activePwmPin_ = rpwmPin_;
    currentPwm_   = pwm;
    enabled_      = true;
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
    writePwmPin(rpwmPin_, 0);
    digitalWrite(enPin_, HIGH);
    writePwmPin(lpwmPin_, pwm);
    activePwmPin_ = lpwmPin_;
    currentPwm_   = pwm;
    enabled_      = true;
}

void DcMotorBts7960::brake()
{
    writePwmPin(rpwmPin_, 0);
    if (lpwmPin_ != UNUSED_PIN)
    {
        writePwmPin(lpwmPin_, 0);
    }

    // En el BTS7960, EN = HIGH con ambos PWM en LOW activa el frenado por rama inferior
    digitalWrite(enPin_, HIGH);
    activePwmPin_ = UNUSED_PIN;
    currentPwm_   = 0;
    enabled_      = true;
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; ++i)
    {
        currentSamples_[i] = 0;
    }
    filteredAdc_ = 0;
}

void DcMotorBts7960::stop()
{
    writePwmPin(rpwmPin_, 0);
    if (lpwmPin_ != UNUSED_PIN)
    {
        writePwmPin(lpwmPin_, 0);
    }

    digitalWrite(enPin_, LOW);
    activePwmPin_ = UNUSED_PIN;
    currentPwm_   = 0;
    enabled_      = false;
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; ++i)
    {
        currentSamples_[i] = 0;
    }
    filteredAdc_ = 0;
}

uint16_t DcMotorBts7960::sampleCurrentAdc()
{
    if (currentSensePin_ == UNUSED_PIN || !enabled_ || currentPwm_ == 0U || activePwmPin_ == UNUSED_PIN)
    {
        return 0;
    }

    // IMPORTANTE (AVR): No usar digitalRead() sobre un pin con PWM activo, ya que wiring_digital.c
    // invoca internamente turnOffPWM(timer) y apaga la salida PWM por hardware. Se lee directamente
    // el registro PINx del puerto para sincronizar el muestreo ADC con el pulso HIGH del MOSFET superior.
    if (currentPwm_ < 255U)
    {
        const uint8_t port = digitalPinToPort(activePwmPin_);
        if (port != NOT_A_PIN)
        {
            volatile uint8_t* const pinReg  = portInputRegister(port);
            const uint8_t           bitMask = digitalPinToBitMask(activePwmPin_);

            if ((*pinReg & bitMask) == 0U)
            {
                const uint32_t waitStartUs = micros();
                while ((*pinReg & bitMask) == 0U)
                {
                    if ((micros() - waitStartUs) >= 2200UL)
                    {
                        break;
                    }
                }
                // Breve asentamiento del espejo de corriente IS del BTS7960 (~15 us tras flanco de subida)
                delayMicroseconds(15);
            }
        }
    }

    currentSamples_[sampleIndex_] = static_cast<uint16_t>(analogRead(currentSensePin_));
    sampleIndex_ = static_cast<uint8_t>((sampleIndex_ + 1U) % CURRENT_FILTER_SAMPLES);

    uint32_t sum = 0;
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; ++i)
    {
        sum += currentSamples_[i];
    }

    filteredAdc_ = static_cast<uint16_t>((sum + (CURRENT_FILTER_SAMPLES - 1U)) / CURRENT_FILTER_SAMPLES);
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
