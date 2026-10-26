#include "filling_automaton.hpp"
#include "config/process_params.hpp"

FillingAutomaton::FillingAutomaton(
    DigitalSensor& presenceSensor,
    DigitalSensor& limitSwitch,
    BottleGate& gate,
    StepperAxis& zAxis,
    FillPump& pump)
    : presenceSensor_(presenceSensor),
      limitSwitch_(limitSwitch),
      gate_(gate),
      zAxis_(zAxis),
      pump_(pump),
      currentState_(FillState::Idle),
      nextState_(FillState::Idle),
      previousState_(FillState::Idle),
      profile_(BottleProfile::Small),
      activeError_(ErrorCode::None),
      stageTimerMs_(0),
      isFirstRun_(true),
      needsClear_(false),
      canReleaseBottle_(true),
      bottleReleasedEvent_(false)
{
}

void FillingAutomaton::reset()
{
    pump_.off();
    zAxis_.stop();
    zAxis_.clearSoftLimitFault();
    gate_.raiseArmsImmediate();

    currentState_ = FillState::Idle;
    nextState_ = FillState::Idle;
    previousState_ = FillState::Idle;
    activeError_ = ErrorCode::None;
    stageTimerMs_ = millis();
    isFirstRun_ = true;
    needsClear_ = false;
    canReleaseBottle_ = true;
    bottleReleasedEvent_ = false;
}

void FillingAutomaton::stopImmediate()
{
    pump_.off();
    zAxis_.stop();
}

void FillingAutomaton::update()
{
    if (activeError_ != ErrorCode::None)
    {
        return;
    }

    // Vigilancia prioritaria de sobrecarrera reportada por la ISR del Timer3
    if (zAxis_.hasSoftLimitTripped())
    {
        triggerFault(ErrorCode::FillZSoftLimitHit);
        return;
    }

    // Limpiar el bloqueo de re-disparo en cuanto la botella saliente libere el sensor óptico
    if (needsClear_ && !presenceSensor_.isTriggered())
    {
        needsClear_ = false;
    }

    // 1. Detección y ejecución de la fase de salida (Exit)
    if (nextState_ != currentState_)
    {
        onExit(currentState_);
        previousState_ = currentState_;
        currentState_ = nextState_;
        isFirstRun_ = true;
    }

    // 2. Ejecución de la fase de entrada (Entry)
    if (isFirstRun_)
    {
        onEntry(currentState_);
        isFirstRun_ = false;
    }

    // 3. Ejecución continua de la fase activa (During)
    onDuring(currentState_);
}

void FillingAutomaton::setProfile(BottleProfile profile)
{
    profile_ = profile;
}

void FillingAutomaton::setReleasePermitted(bool canRelease)
{
    canReleaseBottle_ = canRelease;
}

bool FillingAutomaton::consumeBottleReleasedEvent()
{
    const bool occurred = bottleReleasedEvent_;
    bottleReleasedEvent_ = false;
    return occurred;
}

FillState FillingAutomaton::getState() const
{
    return currentState_;
}

bool FillingAutomaton::isIdle() const
{
    return (currentState_ == FillState::Idle) && (activeError_ == ErrorCode::None);
}

ErrorCode FillingAutomaton::getError() const
{
    return activeError_;
}

void FillingAutomaton::onEntry(FillState state)
{
    stageTimerMs_ = millis();

    switch (state)
    {
        case FillState::Idle:
            pump_.off();
            gate_.raiseArmsImmediate();
            break;

        case FillState::StopArmDown:
            gate_.lowerStopArm();
            break;

        case FillState::ClampArmDown:
            gate_.lowerClampArm();
            break;

        case FillState::LoweringNozzle:
        {
            const uint8_t idx = static_cast<uint8_t>(profile_);
            const int32_t targetSteps = ProcessParams::BOTTLE_PROFILES[idx].fillZSteps;
            if (!zAxis_.moveToTarget(targetSteps, ProcessParams::Z_CRUISE_SPEED))
            {
                triggerFault(ErrorCode::FillZSoftLimitHit);
            }
            break;
        }

        case FillState::Filling:
        {
            const uint8_t idx = static_cast<uint8_t>(profile_);
            const uint16_t fillMs = ProcessParams::BOTTLE_PROFILES[idx].fillMs;
            if (fillMs > 0)
            {
                pump_.on();
            }
            break;
        }

        case FillState::Draining:
            pump_.off();
            break;

        case FillState::RaisingNozzle:
            zAxis_.moveTowardHomeSwitch(ProcessParams::Z_CRUISE_SPEED);
            break;

        case FillState::WaitingQueueSlot:
            // Los brazos permanecen abajo reteniendo la botella llena hasta tener cupo en la cola
            break;

        case FillState::ReleasingBottle:
            bottleReleasedEvent_ = true;
            needsClear_ = true;
            gate_.releaseBottle();
            break;
    }
}

void FillingAutomaton::onDuring(FillState state)
{
    const uint32_t elapsedMs = millis() - stageTimerMs_;

    switch (state)
    {
        case FillState::Idle:
            if (presenceSensor_.isTriggered() && !needsClear_)
            {
                nextState_ = FillState::StopArmDown;
            }
            break;

        case FillState::StopArmDown:
            if (elapsedMs >= ProcessParams::CLAMP_DELAY_MS)
            {
                nextState_ = FillState::ClampArmDown;
            }
            break;

        case FillState::ClampArmDown:
            if (elapsedMs >= ProcessParams::ARM_MOVE_MS)
            {
                if (presenceSensor_.isTriggered())
                {
                    nextState_ = FillState::LoweringNozzle;
                }
                else
                {
                    triggerFault(ErrorCode::FillBottleLost);
                }
            }
            break;

        case FillState::LoweringNozzle:
            if (zAxis_.hasSoftLimitTripped())
            {
                triggerFault(ErrorCode::FillZSoftLimitHit);
            }
            else if (zAxis_.isTargetReached())
            {
                nextState_ = FillState::Filling;
            }
            else if (elapsedMs >= ProcessParams::Z_MOVE_TIMEOUT_MS)
            {
                triggerFault(ErrorCode::FillZLoweringTimeout);
            }
            break;

        case FillState::Filling:
        {
            const uint8_t idx = static_cast<uint8_t>(profile_);
            const uint16_t fillMs = ProcessParams::BOTTLE_PROFILES[idx].fillMs;
            if (elapsedMs >= fillMs)
            {
                nextState_ = FillState::Draining;
            }
            break;
        }

        case FillState::Draining:
            if (elapsedMs >= ProcessParams::DRIP_MS)
            {
                nextState_ = FillState::RaisingNozzle;
            }
            break;

        case FillState::RaisingNozzle:
            if (limitSwitch_.isTriggered() || (!zAxis_.isBusy() && zAxis_.getCurrentSteps() == 0))
            {
                zAxis_.stop();
                nextState_ = FillState::WaitingQueueSlot;
            }
            else if (elapsedMs >= ProcessParams::Z_MOVE_TIMEOUT_MS)
            {
                triggerFault(ErrorCode::FillZRaisingTimeout);
            }
            break;

        case FillState::WaitingQueueSlot:
            if (canReleaseBottle_)
            {
                nextState_ = FillState::ReleasingBottle;
            }
            break;

        case FillState::ReleasingBottle:
            if (elapsedMs >= ProcessParams::RELEASE_MS)
            {
                nextState_ = FillState::Idle;
            }
            break;
    }
}

void FillingAutomaton::onExit(FillState state)
{
    if (state == FillState::Filling)
    {
        // Garantizar el apagado de la bomba al abandonar la fase de llenado por cualquier vía
        pump_.off();
    }
}

void FillingAutomaton::triggerFault(ErrorCode error)
{
    activeError_ = error;
    pump_.off();
    zAxis_.stop();
}
