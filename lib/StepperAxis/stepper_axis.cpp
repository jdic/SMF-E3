#include "stepper_axis.hpp"
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

StepperAxis* StepperAxis::instances_[StepperAxis::MAX_SLOTS] = { nullptr, nullptr };
bool StepperAxis::timer3Initialized_ = false;

StepperAxis::StepperAxis(const StepperAxisConfig& config)
    : config_(config),
      stepOutReg_(nullptr),
      limitInReg_(nullptr),
      stepBitMask_(0),
      limitBitMask_(0),
      currentSteps_(0),
      targetSteps_(0),
      maxTravelSteps_(config.maxTravelSteps),
      currentSpeedStepsPerSec_(0),
      cruiseSpeedStepsPerSec_(0),
      phaseAccumulator_(0),
      moving_(false),
      dirDown_(true),
      ignoreSoftLimit_(false),
      softLimitTripped_(false),
      homingPhase_(HomingPhase::Unhomed)
{
}

void StepperAxis::init()
{
    // Inicializar pines de salida en reposo antes de habilitar OUTPUT
    digitalWrite(config_.stepPin, LOW);
    digitalWrite(config_.dirPin, LOW);
    pinMode(config_.stepPin, OUTPUT);
    pinMode(config_.dirPin, OUTPUT);
    pinMode(config_.enPin, OUTPUT);
    setDriverEnabled(false);

    pinMode(config_.limitSwitchPin, INPUT_PULLUP);

    const uint8_t stepPort  = digitalPinToPort(config_.stepPin);
    const uint8_t limitPort = digitalPinToPort(config_.limitSwitchPin);

    stepOutReg_   = (stepPort != NOT_A_PIN) ? portOutputRegister(stepPort) : nullptr;
    limitInReg_   = (limitPort != NOT_A_PIN) ? portInputRegister(limitPort) : nullptr;
    stepBitMask_  = digitalPinToBitMask(config_.stepPin);
    limitBitMask_ = digitalPinToBitMask(config_.limitSwitchPin);

    const uint8_t slotIndex = static_cast<uint8_t>(config_.slot);
    if (slotIndex < MAX_SLOTS)
    {
        instances_[slotIndex] = this;
    }

    initTimer3Once();
}

void StepperAxis::setSoftLimits(int32_t maxTravelSteps)
{
    if (maxTravelSteps > 0)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            maxTravelSteps_ = maxTravelSteps;
        }
    }
}

void StepperAxis::home(uint16_t homingSpeed)
{
    const uint16_t clampedSpeed = (homingSpeed > MAX_ALLOWED_SPEED) ? MAX_ALLOWED_SPEED : homingSpeed;

    setDriverEnabled(true);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        softLimitTripped_ = false;
        phaseAccumulator_ = 0;

        // Si el fin de carrera ya esta pulsado al iniciar, pasar directo al retroceso (backoff)
        if (isLimitSwitchTriggeredIsr())
        {
            currentSteps_ = 0;
            targetSteps_ = static_cast<int32_t>(config_.homeBackoffSteps);
            setDirectionDown(true);
            ignoreSoftLimit_ = false;
            cruiseSpeedStepsPerSec_ = clampedSpeed;
            currentSpeedStepsPerSec_ = clampedSpeed;
            homingPhase_ = HomingPhase::BackingOff;
            moving_ = (targetSteps_ > 0);
        }
        else
        {
            // Subir buscando el fin de carrera superior
            currentSteps_ = maxTravelSteps_;
            targetSteps_ = -maxTravelSteps_;
            setDirectionDown(false);
            ignoreSoftLimit_ = true;
            cruiseSpeedStepsPerSec_ = clampedSpeed;
            currentSpeedStepsPerSec_ = clampedSpeed;
            homingPhase_ = HomingPhase::SeekingSwitch;
            moving_ = true;
        }
    }
}

bool StepperAxis::moveToTarget(int32_t targetSteps, uint16_t maxSpeed)
{
    int32_t currentPos = 0;
    int32_t limitPos = 0;
    HomingPhase phase = HomingPhase::Unhomed;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        currentPos = currentSteps_;
        limitPos = maxTravelSteps_;
        phase = homingPhase_;
    }

    if ((phase != HomingPhase::Homed) || (targetSteps < 0) || (targetSteps > limitPos))
    {
        return false;
    }

    if (targetSteps == currentPos)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            moving_ = false;
            currentSpeedStepsPerSec_ = 0;
        }
        return true;
    }

    const uint16_t clampedSpeed = (maxSpeed > MAX_ALLOWED_SPEED) ? MAX_ALLOWED_SPEED : maxSpeed;
    const uint16_t startSpeed = (clampedSpeed < MIN_START_SPEED) ? clampedSpeed : MIN_START_SPEED;
    const bool goDown = (targetSteps > currentPos);

    setDriverEnabled(true);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        setDirectionDown(goDown);
        targetSteps_ = targetSteps;
        cruiseSpeedStepsPerSec_ = clampedSpeed;
        currentSpeedStepsPerSec_ = startSpeed;
        phaseAccumulator_ = 0;
        ignoreSoftLimit_ = false;
        moving_ = true;
    }

    return true;
}

void StepperAxis::setCruiseSpeed(uint16_t maxSpeed)
{
    uint16_t clampedSpeed = (maxSpeed > MAX_ALLOWED_SPEED) ? MAX_ALLOWED_SPEED : maxSpeed;
    if (clampedSpeed == 0U)
    {
        clampedSpeed = 1U;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        cruiseSpeedStepsPerSec_ = clampedSpeed;
    }
}

void StepperAxis::moveTowardHomeSwitch(uint16_t maxSpeed)
{
    const uint16_t clampedSpeed = (maxSpeed > MAX_ALLOWED_SPEED) ? MAX_ALLOWED_SPEED : maxSpeed;
    const uint16_t startSpeed = (clampedSpeed < MIN_START_SPEED) ? clampedSpeed : MIN_START_SPEED;

    setDriverEnabled(true);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (isLimitSwitchTriggeredIsr())
        {
            currentSteps_ = 0;
            targetSteps_ = 0;
            moving_ = false;
            currentSpeedStepsPerSec_ = 0;
            return;
        }

        setDirectionDown(false);
        targetSteps_ = -maxTravelSteps_;
        cruiseSpeedStepsPerSec_ = clampedSpeed;
        currentSpeedStepsPerSec_ = startSpeed;
        phaseAccumulator_ = 0;
        ignoreSoftLimit_ = true;
        moving_ = true;
    }
}

void StepperAxis::jogUp(uint16_t steps, uint16_t speed)
{
    if (steps == 0U)
    {
        return;
    }

    const uint16_t clampedSpeed = (speed > MAX_ALLOWED_SPEED) ? MAX_ALLOWED_SPEED : speed;
    setDriverEnabled(true);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (isLimitSwitchTriggeredIsr())
        {
            currentSteps_ = 0;
            targetSteps_ = 0;
            moving_ = false;
            currentSpeedStepsPerSec_ = 0;
            return;
        }

        setDirectionDown(false);
        targetSteps_ = currentSteps_ - static_cast<int32_t>(steps);
        cruiseSpeedStepsPerSec_ = clampedSpeed;
        currentSpeedStepsPerSec_ = clampedSpeed;
        phaseAccumulator_ = 0;
        ignoreSoftLimit_ = true;
        moving_ = true;
    }
}

void StepperAxis::jogDown(uint16_t steps, uint16_t speed)
{
    if (steps == 0U)
    {
        return;
    }

    const uint16_t clampedSpeed = (speed > MAX_ALLOWED_SPEED) ? MAX_ALLOWED_SPEED : speed;
    setDriverEnabled(true);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        int32_t desired = currentSteps_ + static_cast<int32_t>(steps);
        if (desired > maxTravelSteps_)
        {
            desired = maxTravelSteps_;
        }

        if (desired <= currentSteps_)
        {
            moving_ = false;
            currentSpeedStepsPerSec_ = 0;
            return;
        }

        setDirectionDown(true);
        targetSteps_ = desired;
        cruiseSpeedStepsPerSec_ = clampedSpeed;
        currentSpeedStepsPerSec_ = clampedSpeed;
        phaseAccumulator_ = 0;
        ignoreSoftLimit_ = false;
        moving_ = true;
    }
}

void StepperAxis::stop()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        moving_ = false;
        currentSpeedStepsPerSec_ = 0;
    }
}

void StepperAxis::setDriverEnabled(bool enable)
{
    const bool pinHigh = config_.enableActiveLow ? !enable : enable;
    digitalWrite(config_.enPin, pinHigh ? HIGH : LOW);
}

void StepperAxis::updateMotionProfile()
{
    HomingPhase phase = HomingPhase::Unhomed;
    bool busy = false;
    int32_t currentPos = 0;
    int32_t targetPos = 0;
    uint16_t speed = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        phase = homingPhase_;
        busy = moving_;
        currentPos = currentSteps_;
        targetPos = targetSteps_;
        speed = currentSpeedStepsPerSec_;
    }

    // Transicion de Homing: al tocar el fin de carrera superior, retroceder homeBackoffSteps
    if (phase == HomingPhase::HitSwitch)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            currentSteps_ = 0;
            targetSteps_ = static_cast<int32_t>(config_.homeBackoffSteps);
            setDirectionDown(true);
            ignoreSoftLimit_ = false;
            currentSpeedStepsPerSec_ = cruiseSpeedStepsPerSec_;
            phaseAccumulator_ = 0;
            homingPhase_ = HomingPhase::BackingOff;
            moving_ = (targetSteps_ > 0);
        }
        return;
    }

    if (phase == HomingPhase::BackingOff && !busy)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            currentSteps_ = 0;
            targetSteps_ = 0;
            homingPhase_ = HomingPhase::Homed;
        }
        return;
    }

    if (!busy || (phase == HomingPhase::SeekingSwitch) || (phase == HomingPhase::BackingOff))
    {
        return;
    }

    // Rampa trapezoidal para movimientos de proceso
    const int32_t remainingSigned = targetPos - currentPos;
    const int32_t remainingSteps = (remainingSigned >= 0) ? remainingSigned : -remainingSigned;

    if (remainingSteps == 0)
    {
        stop();
        return;
    }

    // Incremento de velocidad por periodo de 20 ms: deltaV = accel * 20 / 1000 = accel / 50
    uint16_t deltaV = config_.accelStepsPerSec2 / (1000U / PROFILE_PERIOD_MS);
    if (deltaV == 0U)
    {
        deltaV = 1U;
    }

    // Distancia de frenado: d = v^2 / (2 * a)
    const uint32_t speedSq = static_cast<uint32_t>(speed) * static_cast<uint32_t>(speed);
    const uint32_t twoAccel = 2UL * static_cast<uint32_t>(config_.accelStepsPerSec2);
    const int32_t decelSteps = (twoAccel > 0UL) ? static_cast<int32_t>(speedSq / twoAccel) : 0;

    uint16_t nextSpeed = speed;
    if (remainingSteps <= decelSteps)
    {
        if (speed > (MIN_START_SPEED + deltaV))
        {
            nextSpeed = speed - deltaV;
        }
        else
        {
            nextSpeed = MIN_START_SPEED;
        }
    }
    else if (speed < cruiseSpeedStepsPerSec_)
    {
        const uint32_t candidate = static_cast<uint32_t>(speed) + deltaV;
        nextSpeed = (candidate > cruiseSpeedStepsPerSec_)
                        ? cruiseSpeedStepsPerSec_
                        : static_cast<uint16_t>(candidate);
    }
    else if (speed > cruiseSpeedStepsPerSec_)
    {
        nextSpeed = (speed > (cruiseSpeedStepsPerSec_ + deltaV))
                        ? (speed - deltaV)
                        : cruiseSpeedStepsPerSec_;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        currentSpeedStepsPerSec_ = nextSpeed;
    }
}

bool StepperAxis::isBusy() const
{
    bool busy = false;
    HomingPhase phase = HomingPhase::Unhomed;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        busy = moving_;
        phase = homingPhase_;
    }
    return (busy || (phase == HomingPhase::SeekingSwitch) ||
            (phase == HomingPhase::HitSwitch) || (phase == HomingPhase::BackingOff));
}

bool StepperAxis::isTargetReached() const
{
    bool reached = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        reached = (!moving_ && !softLimitTripped_ &&
                   (homingPhase_ == HomingPhase::Homed) &&
                   (currentSteps_ == targetSteps_));
    }
    return reached;
}

bool StepperAxis::isHome() const
{
    HomingPhase phase = HomingPhase::Unhomed;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        phase = homingPhase_;
    }
    return (phase == HomingPhase::Homed);
}

bool StepperAxis::hasSoftLimitTripped() const
{
    return softLimitTripped_;
}

void StepperAxis::clearSoftLimitFault()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        softLimitTripped_ = false;
    }
}

int32_t StepperAxis::getCurrentSteps() const
{
    int32_t pos = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        pos = currentSteps_;
    }
    return pos;
}

bool StepperAxis::handleTimerIsrTick()
{
    if (!moving_)
    {
        return false;
    }

    // 1. Vigilancia de fin de carrera superior cuando el eje sube (!dirDown_)
    if (!dirDown_ && isLimitSwitchTriggeredIsr())
    {
        moving_ = false;
        currentSpeedStepsPerSec_ = 0;
        currentSteps_ = 0;
        targetSteps_ = 0;

        if (homingPhase_ == HomingPhase::SeekingSwitch)
        {
            homingPhase_ = HomingPhase::HitSwitch;
        }
        else
        {
            homingPhase_ = HomingPhase::Homed;
        }
        return false;
    }

    // 2. Vigilancia de soft limit inferior cuando el eje baja (dirDown_)
    if (dirDown_ && !ignoreSoftLimit_ && (currentSteps_ >= maxTravelSteps_))
    {
        moving_ = false;
        currentSpeedStepsPerSec_ = 0;
        softLimitTripped_ = true;
        return false;
    }

    // 3. Verificacion de llegada a la cota objetivo
    if (currentSteps_ == targetSteps_)
    {
        moving_ = false;
        currentSpeedStepsPerSec_ = 0;
        return false;
    }

    // 4. Acumulador de fase para generacion de pulsos a 20 kHz
    phaseAccumulator_ += currentSpeedStepsPerSec_;
    if (phaseAccumulator_ < ISR_FREQ_HZ)
    {
        return false;
    }

    phaseAccumulator_ -= ISR_FREQ_HZ;
    if (dirDown_)
    {
        ++currentSteps_;
    }
    else
    {
        --currentSteps_;
    }

    if (currentSteps_ == targetSteps_)
    {
        moving_ = false;
        currentSpeedStepsPerSec_ = 0;
    }

    return (stepOutReg_ != nullptr);
}

void StepperAxis::initTimer3Once()
{
    if (timer3Initialized_)
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        // Configurar Timer3 en modo CTC (WGM32 = 1), prescaler = 8 (CS31 = 1 -> 2 MHz)
        // OCR3A = 99 fija el tope del contador -> 2,000,000 / (99 + 1) = 20,000 Hz (periodo de 50 us).
        // Se habilita OCIE3B (TIMER3_COMPB_vect) porque la librería Servo declara TIMER3_COMPA_vect
        // aunque solo utilice el Timer5 para los 4 servomotores.
        TCCR3A = 0;
        TCCR3B = static_cast<uint8_t>((1 << WGM32) | (1 << CS31));
        TCNT3  = 0;
        OCR3A  = 99;
        OCR3B  = 99;
        TIMSK3 = static_cast<uint8_t>(1 << OCIE3B);
        timer3Initialized_ = true;
    }
}

void StepperAxis::setDirectionDown(bool down)
{
    dirDown_ = down;
    const bool pinHigh = config_.dirDownHigh ? down : !down;
    digitalWrite(config_.dirPin, pinHigh ? HIGH : LOW);
}

bool StepperAxis::isLimitSwitchTriggeredIsr() const
{
    if (limitInReg_ == nullptr)
    {
        return false;
    }
    const bool pinHigh = ((*limitInReg_ & limitBitMask_) != 0U);
    return config_.limitSwitchActiveLow ? !pinHigh : pinHigh;
}

void StepperAxis::handleTimer3Isr()
{
    StepperAxis* const axis0 = instances_[0];
    const bool step0 = (axis0 != nullptr) && axis0->handleTimerIsrTick();

    StepperAxis* const axis1 = instances_[1];
    const bool step1 = (axis1 != nullptr) && axis1->handleTimerIsrTick();

    if (step0 || step1)
    {
        if (step0)
        {
            *(axis0->stepOutReg_) |= axis0->stepBitMask_;
        }
        if (step1)
        {
            *(axis1->stepOutReg_) |= axis1->stepBitMask_;
        }

        _delay_us(STEP_PULSE_WIDTH_US);

        if (step0)
        {
            *(axis0->stepOutReg_) &= static_cast<uint8_t>(~axis0->stepBitMask_);
        }
        if (step1)
        {
            *(axis1->stepOutReg_) &= static_cast<uint8_t>(~axis1->stepBitMask_);
        }
    }
}

ISR(TIMER3_COMPB_vect)
{
    StepperAxis::handleTimer3Isr();
}
