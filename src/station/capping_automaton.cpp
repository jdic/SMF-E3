#include "capping_automaton.hpp"
#include "config/process_params.hpp"

CappingAutomaton::CappingAutomaton(
    DigitalSensor& presenceSensor,
    DigitalSensor& positionSensor,
    DigitalSensor& limitSwitch,
    DigitalSensor& visionSensor,
    BottleGate& gate,
    StepperAxis& zAxis,
    DcMotorBts7960& chuckMotor)
    : StationAutomatonBase<CappingState>(presenceSensor, positionSensor, limitSwitch, gate, zAxis),
      chuckMotor_(chuckMotor),
      visionSensor_(visionSensor),
      engageStarted_(false),
      visionStopArmRaised_(false)
{
}

void CappingAutomaton::reset()
{
    chuckMotor_.stop();
    resetBaseState();
    engageStarted_       = false;
    visionStopArmRaised_ = false;
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
        if (ProcessParams::isErrorEnabled(ErrorCode::CapZSoftLimitHit))
        {
            triggerFault(ErrorCode::CapZSoftLimitHit);
            return;
        }
        zAxis_.clearSoftLimitFault();
    }

    updateClearLock();

    bool visionAdvancingBelt = false;
    if ((currentState_ == CappingState::VisionInspect) &&
        ProcessParams::hasConfigFlag(ConfigFlags::VISION_ADVANCE) &&
        !isFirstRun_)
    {
        const uint32_t elapsed   = millis() - stageTimerMs_;
        const uint32_t moveStart = ProcessParams::ARM_MOVE_MS;
        const uint32_t moveEnd   = moveStart + ProcessParams::VISION_ADVANCE_MS;
        visionAdvancingBelt = (elapsed >= moveStart) && (elapsed < moveEnd);
    }
    updateBeltDependentTimer(visionAdvancingBelt);

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

bool CappingAutomaton::isHoldingBottle() const
{
    if (currentState_ == CappingState::VisionInspect)
    {
        if (activeError_ != ErrorCode::None)
        {
            return true;
        }
        if (ProcessParams::hasConfigFlag(ConfigFlags::VISION_ADVANCE))
        {
            const uint32_t elapsedMs   = millis() - stageTimerMs_;
            const uint32_t moveStartMs = ProcessParams::ARM_MOVE_MS;
            const uint32_t moveEndMs   = moveStartMs + ProcessParams::VISION_ADVANCE_MS;
            if ((elapsedMs >= moveStartMs) && (elapsedMs < moveEndMs))
            {
                return false;
            }
        }
        return true;
    }

    return StationAutomatonBase<CappingState>::isHoldingBottle();
}

bool CappingAutomaton::acknowledgeVisionAlarm()
{
    if (activeError_ != ErrorCode::CapMissingVision)
    {
        return false;
    }

    activeError_ = ErrorCode::None;
    nextState_   = CappingState::ReleasingBottle;
    return true;
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

        case CappingState::VisionInspect:
            chuckMotor_.stop();
            visionStopArmRaised_ = false;
            if (ProcessParams::hasConfigFlag(ConfigFlags::VISION_ADVANCE))
            {
                gate_.raiseStopArm();
                visionStopArmRaised_ = true;
            }
            break;

        case CappingState::ReleasingBottle:
            chuckMotor_.stop();
            bottleEvent_ = true;
            if (visionStopArmRaised_)
            {
                gate_.raiseArmsImmediate();
                visionStopArmRaised_ = false;
            }
            else
            {
                gate_.releaseBottle();
            }
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
                    ProcessParams::isErrorEnabled(ErrorCode::CapBottleLost)))
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
            const uint16_t currentAdc = chuckMotor_.getFilteredCurrentAdc();
            if (ProcessParams::isErrorEnabled(ErrorCode::ChuckOvercurrent) &&
                (currentAdc >= ProcessParams::CHUCK_CURRENT_LIMIT_ADC))
            {
                triggerFault(ErrorCode::ChuckOvercurrent);
                break;
            }

            if (ProcessParams::isErrorEnabled(ErrorCode::ChuckStallPremature) &&
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
                nextState_ = ProcessParams::hasConfigFlag(ConfigFlags::VISION_INSPECT)
                                 ? CappingState::VisionInspect
                                 : CappingState::ReleasingBottle;
            }
            else if (elapsedMs >= ProcessParams::Z_MOVE_TIMEOUT_MS)
            {
                triggerFault(ErrorCode::CapZRaisingTimeout);
            }
            break;

        case CappingState::VisionInspect:
        {
            const uint32_t advanceWindowMs = ProcessParams::hasConfigFlag(ConfigFlags::VISION_ADVANCE)
                ? (static_cast<uint32_t>(ProcessParams::ARM_MOVE_MS) +
                   static_cast<uint32_t>(ProcessParams::VISION_ADVANCE_MS))
                : 0UL;
            const uint32_t evalReadyMs =
                advanceWindowMs + static_cast<uint32_t>(ProcessParams::VISION_SETTLE_MS);

            if (elapsedMs >= evalReadyMs)
            {
                if (visionSensor_.isTriggered() && ProcessParams::isErrorEnabled(ErrorCode::CapMissingVision))
                {
                    triggerFault(ErrorCode::CapMissingVision);
                }
                else
                {
                    nextState_ = CappingState::ReleasingBottle;
                }
            }
            break;
        }

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
