#include "interrupt_encoder.hpp"
#include <util/atomic.h>

InterruptEncoder* InterruptEncoder::instances_[InterruptEncoder::MAX_SLOTS] = { nullptr, nullptr };

InterruptEncoder::InterruptEncoder(
    EncoderSlot slot,
    uint8_t pinA,
    uint8_t pinB,
    uint16_t pulsesPerRev,
    bool usePullup)
    : slot_(slot),
      pinA_(pinA),
      pinB_(pinB),
      maskA_(0),
      maskB_(0),
      pulsesPerRev_(pulsesPerRev),
      usePullup_(usePullup),
      pulseCount_(0),
      lastPulseSample_(0),
      lastSpeedTimeMs_(0),
      currentRpm_(0)
{
}

void InterruptEncoder::init()
{
    const uint8_t mode = usePullup_ ? INPUT_PULLUP : INPUT;
    pinMode(pinA_, mode);
    pinMode(pinB_, mode);

    maskA_ = digitalPinToBitMask(pinA_);
    maskB_ = digitalPinToBitMask(pinB_);

    reset();

    const uint8_t slotIndex = static_cast<uint8_t>(slot_);
    if (slotIndex < MAX_SLOTS)
    {
        instances_[slotIndex] = this;
    }

    const int intNum = digitalPinToInterrupt(pinA_);
    if (intNum != NOT_AN_INTERRUPT)
    {
        if (slot_ == EncoderSlot::Chuck)
        {
            attachInterrupt(intNum, isrSlot0, CHANGE);
        }
        else
        {
            attachInterrupt(intNum, isrSlot1, CHANGE);
        }
    }
}

int32_t InterruptEncoder::getPulses() const
{
    int32_t snapshot = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        snapshot = pulseCount_;
    }
    return snapshot;
}

void InterruptEncoder::reset()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        pulseCount_ = 0;
    }
    lastPulseSample_ = 0;
    lastSpeedTimeMs_ = millis();
    currentRpm_ = 0;
}

uint16_t InterruptEncoder::updateSpeed()
{
    const uint32_t nowMs = millis();
    const uint32_t elapsedMs = nowMs - lastSpeedTimeMs_;

    if ((elapsedMs == 0U) || (pulsesPerRev_ == 0U))
    {
        return currentRpm_;
    }

    const int32_t currentPulses = getPulses();
    const int32_t deltaSigned = currentPulses - lastPulseSample_;
    const uint32_t deltaAbs = static_cast<uint32_t>((deltaSigned >= 0) ? deltaSigned : -deltaSigned);

    lastPulseSample_ = currentPulses;
    lastSpeedTimeMs_ = nowMs;

    const uint32_t countsPerRev = static_cast<uint32_t>(pulsesPerRev_) * EDGES_PER_PULSE;
    const uint32_t denominator = elapsedMs * countsPerRev;

    if (denominator == 0U)
    {
        currentRpm_ = 0;
        return currentRpm_;
    }

    currentRpm_ = static_cast<uint16_t>((deltaAbs * MS_PER_MINUTE) / denominator);
    return currentRpm_;
}

uint16_t InterruptEncoder::getSpeedRpm() const
{
    return currentRpm_;
}

void InterruptEncoder::handleIsr()
{
    // Lectura directa del puerto D (pines 18..21 en ATmega2560) sin pasar por digitalRead()
    const uint8_t portState = PIND;
    const bool aHigh = ((portState & maskA_) != 0U);
    const bool bHigh = ((portState & maskB_) != 0U);

    if (aHigh == bHigh)
    {
        ++pulseCount_;
    }
    else
    {
        --pulseCount_;
    }
}

void InterruptEncoder::isrSlot0()
{
    if (instances_[0] != nullptr)
    {
        instances_[0]->handleIsr();
    }
}

void InterruptEncoder::isrSlot1()
{
    if (instances_[1] != nullptr)
    {
        instances_[1]->handleIsr();
    }
}
