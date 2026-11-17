#include "filling_automaton.hpp"
#include "config/process_params.hpp"

FillingAutomaton::FillingAutomaton(
    DigitalSensor& presenceSensor,
    DigitalSensor& positionSensor,
    DigitalSensor& limitSwitch,
    BottleGate& gate,
    StepperAxis& zAxis,
    FillPump& pump)
    : StationAutomatonBase<FillState>(presenceSensor, positionSensor, limitSwitch, gate, zAxis),
      pump_(pump),
      canReleaseBottle_(true)
{
}

void FillingAutomaton::reset()
{
    pump_.off();
    resetBaseState();
    canReleaseBottle_ = true;
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

    if (zAxis_.hasSoftLimitTripped())
    {
        if (ProcessParams::ENABLE_ERR_202_FILL_Z_SOFT_LIMIT)
        {
            triggerFault(ErrorCode::FillZSoftLimitHit);
            return;
        }
        zAxis_.clearSoftLimitFault();
    }

    updateClearLock();
    updateBeltDependentTimer();

    if (nextState_ != currentState_)
    {
        onExit(currentState_);
        previousState_ = currentState_;
        currentState_  = nextState_;
        isFirstRun_    = true;
    }

    if (isFirstRun_)
    {
        onEntry(currentState_);
        isFirstRun_ = false;
    }

    onDuring(currentState_);
}

void FillingAutomaton::setReleasePermitted(bool canRelease)
{
    canReleaseBottle_ = canRelease;
}

bool FillingAutomaton::consumeBottleReleasedEvent()
{
    return consumeBottleEvent();
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
            break;

        case FillState::ReleasingBottle:
            bottleEvent_ = true;
            needsClear_  = true;
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
            evalIdleTransition();
            break;

        case FillState::StopArmDown:
            if (!evalStopArmDown(elapsedMs))
            {
                triggerFault(ErrorCode::FillBottleLost);
            }
            break;

        case FillState::ClampArmDown:
            if (!evalClampArmDown(
                    elapsedMs,
                    FillState::LoweringNozzle,
                    ProcessParams::ENABLE_ERR_101_FILL_BOTTLE_LOST))
            {
                triggerFault(ErrorCode::FillBottleLost);
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
            evalReleasingBottle(elapsedMs);
            break;
    }
}

void FillingAutomaton::onExit(FillState state)
{
    if (state == FillState::Filling)
    {
        pump_.off();
    }
}

void FillingAutomaton::triggerFault(ErrorCode error)
{
    if (!ProcessParams::isErrorEnabled(error))
    {
        return;
    }

    activeError_ = error;
    pump_.off();
    zAxis_.stop();
}
