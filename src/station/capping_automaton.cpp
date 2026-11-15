#include "capping_automaton.hpp"
#include "config/process_params.hpp"

CappingAutomaton::CappingAutomaton(
    DigitalSensor& presenceSensor,
    DigitalSensor& positionSensor,
    DigitalSensor& limitSwitch,
    BottleGate& gate,
    StepperAxis& zAxis,
    DcMotorBts7960& chuckMotor)
    : StationAutomatonBase<CappingState>(presenceSensor, positionSensor, limitSwitch, gate, zAxis),
      chuckMotor_(chuckMotor),
      engageStarted_(false)
{
}

void CappingAutomaton::reset()
{
    chuckMotor_.stop();
    resetBaseState();
    engageStarted_ = false;
}

void CappingAutomaton::stopImmediate()
{
    chuckMotor_.stop();
    zAxis_.stop();
}

void CappingAutomaton::update()
{
    if (activeError_ != ErrorCode::None)
    {
        return;
    }

    if (zAxis_.hasSoftLimitTripped())
    {
        if (ProcessParams::ENABLE_ERR_212_CAP_Z_SOFT_LIMIT)
        {
            triggerFault(ErrorCode::CapZSoftLimitHit);
            return;
        }
        zAxis_.clearSoftLimitFault();
    }

    updateClearLock();

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

bool CappingAutomaton::consumeBottleCompletedEvent()
{
    return consumeBottleEvent();
}

void CappingAutomaton::onEntry(CappingState state)
{
    stageTimerMs_ = millis();

    switch (state)
    {
        case CappingState::Idle:
            chuckMotor_.stop();
            gate_.raiseArmsImmediate();
            break;

        case CappingState::StopArmDown:
            gate_.lowerStopArm();
            break;

        case CappingState::ClampArmDown:
            gate_.lowerClampArm();
            break;

        case CappingState::LoweringHead:
        {
            engageStarted_ = false;
            const uint8_t idx = static_cast<uint8_t>(profile_);
            const auto& profileParams = ProcessParams::BOTTLE_PROFILES[idx];
            const uint16_t initialSpeed = (profileParams.capEngageSteps <= 0)
                                              ? ProcessParams::CAP_Z_ENGAGE_SPEED
                                              : ProcessParams::Z_CRUISE_SPEED;
            if (!zAxis_.moveToTarget(profileParams.capZSteps, initialSpeed))
            {
                triggerFault(ErrorCode::CapZSoftLimitHit);
            }
            else if (profileParams.capEngageSteps <= 0)
            {
                engageStarted_ = true;
                chuckMotor_.driveForward(ProcessParams::CHUCK_PWM);
            }
            break;
        }

        case CappingState::TighteningCap:
            chuckMotor_.driveForward(ProcessParams::CHUCK_PWM);
            break;

        case CappingState::RaisingHead:
            chuckMotor_.brake();
            zAxis_.moveTowardHomeSwitch(ProcessParams::Z_CRUISE_SPEED);
            break;

        case CappingState::ReleasingBottle:
            chuckMotor_.stop();
            bottleEvent_ = true;
            needsClear_  = true;
            gate_.releaseBottle();
            break;
    }
}

void CappingAutomaton::onDuring(CappingState state)
{
    const uint32_t elapsedMs = millis() - stageTimerMs_;

    switch (state)
    {
        case CappingState::Idle:
            evalIdleTransition();
            break;

        case CappingState::StopArmDown:
            if (!evalStopArmDown(elapsedMs))
            {
                triggerFault(ErrorCode::CapBottleLost);
            }
            break;

        case CappingState::ClampArmDown:
            if (!evalClampArmDown(
                    elapsedMs,
                    CappingState::LoweringHead,
                    ProcessParams::ENABLE_ERR_102_CAP_BOTTLE_LOST))
            {
                triggerFault(ErrorCode::CapBottleLost);
            }
            break;

        case CappingState::LoweringHead:
        {
            const uint8_t idx = static_cast<uint8_t>(profile_);
            const int32_t engageSteps = ProcessParams::BOTTLE_PROFILES[idx].capEngageSteps;
            if (!engageStarted_ && (zAxis_.getCurrentSteps() >= engageSteps))
            {
                engageStarted_ = true;
                zAxis_.setCruiseSpeed(ProcessParams::CAP_Z_ENGAGE_SPEED);
                chuckMotor_.driveForward(ProcessParams::CHUCK_PWM);
            }

            if (zAxis_.hasSoftLimitTripped())
            {
                triggerFault(ErrorCode::CapZSoftLimitHit);
            }
            else if (zAxis_.isTargetReached())
            {
                nextState_ = CappingState::TighteningCap;
            }
            else if (elapsedMs >= ProcessParams::Z_MOVE_TIMEOUT_MS)
            {
                triggerFault(ErrorCode::CapZLoweringTimeout);
            }
            break;
        }

        case CappingState::TighteningCap:
        {
            const uint16_t currentAdc = chuckMotor_.sampleCurrentAdc();
            if (ProcessParams::ENABLE_ERR_301_CHUCK_OVERCURRENT &&
                (currentAdc >= ProcessParams::CHUCK_CURRENT_LIMIT_ADC))
            {
                triggerFault(ErrorCode::ChuckOvercurrent);
                break;
            }

            if (ProcessParams::ENABLE_ERR_302_CHUCK_STALL &&
                (elapsedMs < ProcessParams::CHUCK_MIN_TIGHTEN_MS) &&
                (currentAdc >= ProcessParams::CHUCK_STALL_ADC))
            {
                triggerFault(ErrorCode::ChuckStallPremature);
                break;
            }

            if (elapsedMs >= ProcessParams::TIGHTEN_MS)
            {
                nextState_ = CappingState::RaisingHead;
            }
            break;
        }

        case CappingState::RaisingHead:
            if (limitSwitch_.isTriggered() || (!zAxis_.isBusy() && zAxis_.getCurrentSteps() == 0))
            {
                zAxis_.stop();
                chuckMotor_.stop();
                nextState_ = CappingState::ReleasingBottle;
            }
            else if (elapsedMs >= ProcessParams::Z_MOVE_TIMEOUT_MS)
            {
                triggerFault(ErrorCode::CapZRaisingTimeout);
            }
            break;

        case CappingState::ReleasingBottle:
            evalReleasingBottle(elapsedMs);
            break;
    }
}

void CappingAutomaton::onExit(CappingState state)
{
    if (state == CappingState::TighteningCap)
    {
        chuckMotor_.brake();
    }
}

void CappingAutomaton::triggerFault(ErrorCode error)
{
    if (!ProcessParams::isErrorEnabled(error))
    {
        return;
    }

    activeError_ = error;
    chuckMotor_.stop();
    zAxis_.stop();
}
