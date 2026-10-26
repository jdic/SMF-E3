#include "alarm_buzzer.hpp"
#include "config/process_params.hpp"

AlarmBuzzer::AlarmBuzzer(uint8_t pin, bool activeHigh)
    : pin_(pin),
      activeHigh_(activeHigh),
      pattern_(BuzzerPattern::Silent),
      phaseStartMs_(0),
      remainingPulses_(0),
      outputOn_(false)
{
}

void AlarmBuzzer::init()
{
    pinMode(pin_, OUTPUT);
    writeOutput(false);
}

void AlarmBuzzer::setPattern(BuzzerPattern pattern, uint32_t nowMs)
{
    if (pattern == pattern_)
    {
        return;
    }

    pattern_ = pattern;
    phaseStartMs_ = nowMs;

    if (pattern_ == BuzzerPattern::Silent)
    {
        remainingPulses_ = 0;
        writeOutput(false);
    }
    else if (pattern_ == BuzzerPattern::BootReady)
    {
        remainingPulses_ = ProcessParams::BUZZER_BOOT_PULSES;
        writeOutput(true);
    }
    else
    {
        remainingPulses_ = 0;
        writeOutput(true);
    }
}

void AlarmBuzzer::triggerBootReady(uint32_t nowMs)
{
    pattern_ = BuzzerPattern::BootReady;
    phaseStartMs_ = nowMs;
    remainingPulses_ = ProcessParams::BUZZER_BOOT_PULSES;
    writeOutput(true);
}

void AlarmBuzzer::silence()
{
    pattern_ = BuzzerPattern::Silent;
    remainingPulses_ = 0;
    writeOutput(false);
}

void AlarmBuzzer::update(uint32_t nowMs)
{
    if (pattern_ == BuzzerPattern::Silent)
    {
        return;
    }

    uint16_t onDurationMs = 0;
    uint16_t offDurationMs = 0;

    switch (pattern_)
    {
        case BuzzerPattern::BootReady:
            onDurationMs = ProcessParams::BUZZER_BOOT_ON_MS;
            offDurationMs = ProcessParams::BUZZER_BOOT_OFF_MS;
            break;

        case BuzzerPattern::Warning:
            onDurationMs = ProcessParams::BUZZER_WARN_ON_MS;
            offDurationMs = ProcessParams::BUZZER_WARN_OFF_MS;
            break;

        case BuzzerPattern::Fault:
            onDurationMs = ProcessParams::BUZZER_FAULT_ON_MS;
            offDurationMs = ProcessParams::BUZZER_FAULT_OFF_MS;
            break;

        case BuzzerPattern::EmergencyStop:
            onDurationMs = ProcessParams::BUZZER_ESTOP_ON_MS;
            offDurationMs = ProcessParams::BUZZER_ESTOP_OFF_MS;
            break;

        case BuzzerPattern::Silent:
        default:
            writeOutput(false);
            return;
    }

    const uint32_t elapsedMs = nowMs - phaseStartMs_;

    if (outputOn_)
    {
        if (elapsedMs >= onDurationMs)
        {
            writeOutput(false);
            phaseStartMs_ = nowMs;

            if (pattern_ == BuzzerPattern::BootReady)
            {
                if (remainingPulses_ > 0)
                {
                    remainingPulses_--;
                }

                if (remainingPulses_ == 0)
                {
                    pattern_ = BuzzerPattern::Silent;
                }
            }
        }
    }
    else
    {
        if (elapsedMs >= offDurationMs)
        {
            writeOutput(true);
            phaseStartMs_ = nowMs;
        }
    }
}

BuzzerPattern AlarmBuzzer::getPattern() const
{
    return pattern_;
}

bool AlarmBuzzer::isSounding() const
{
    return outputOn_;
}

void AlarmBuzzer::writeOutput(bool on)
{
    outputOn_ = on;
    const uint8_t level = (on == activeHigh_) ? HIGH : LOW;
    digitalWrite(pin_, level);
}
