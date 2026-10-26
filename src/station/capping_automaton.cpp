#include "capping_automaton.hpp"
#include "config/process_params.hpp"

CappingAutomaton::CappingAutomaton(
    DigitalSensor& presenceSensor,
    DigitalSensor& limitSwitch,
    BottleGate& gate,
    StepperAxis& zAxis,
    DcMotorBts7960& chuckMotor)
    : presenceSensor_(presenceSensor),
      limitSwitch_(limitSwitch),
      gate_(gate),
      zAxis_(zAxis),
      chuckMotor_(chuckMotor),
      currentState_(CappingState::Idle),
      nextState_(CappingState::Idle),
      previousState_(CappingState::Idle),
      profile_(BottleProfile::Small),
      activeError_(ErrorCode::None),
      stageTimerMs_(0),
      isFirstRun_(true),
      needsClear_(false),
      bottleCompletedEvent_(false)
{
}

void CappingAutomaton::reset()
{
    chuckMotor_.stop();
    zAxis_.stop();
    zAxis_.clearSoftLimitFault();
    gate_.raiseArmsImmediate();

    currentState_ = CappingState::Idle;
    nextState_ = CappingState::Idle;
    previousState_ = CappingState::Idle;
    activeError_ = ErrorCode::None;
    stageTimerMs_ = millis();
    isFirstRun_ = true;
    needsClear_ = false;
    bottleCompletedEvent_ = false;
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

    // Vigilancia prioritaria de sobrecarrera reportada por la ISR del Timer3
    if (zAxis_.hasSoftLimitTripped())
    {
        triggerFault(ErrorCode::CapZSoftLimitHit);
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

void CappingAutomaton::setProfile(BottleProfile profile)
{
    profile_ = profile;
}

bool CappingAutomaton::consumeBottleCompletedEvent()
{
    const bool occurred = bottleCompletedEvent_;
    bottleCompletedEvent_ = false;
    return occurred;
}

CappingState CappingAutomaton::getState() const
{
    return currentState_;
}

bool CappingAutomaton::isIdle() const
{
    return (currentState_ == CappingState::Idle) && (activeError_ == ErrorCode::None);
}

ErrorCode CappingAutomaton::getError() const
{
    return activeError_;
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
            const uint8_t idx = static_cast<uint8_t>(profile_);
            const int32_t targetSteps = ProcessParams::BOTTLE_PROFILES[idx].capZSteps;
            if (!zAxis_.moveToTarget(targetSteps, ProcessParams::Z_CRUISE_SPEED))
            {
                triggerFault(ErrorCode::CapZSoftLimitHit);
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
            bottleCompletedEvent_ = true;
            needsClear_ = true;
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
            if (presenceSensor_.isTriggered() && !needsClear_)
            {
                nextState_ = CappingState::StopArmDown;
            }
            break;

        case CappingState::StopArmDown:
            if (elapsedMs >= ProcessParams::CLAMP_DELAY_MS)
            {
                nextState_ = CappingState::ClampArmDown;
            }
            break;

        case CappingState::ClampArmDown:
            if (elapsedMs >= ProcessParams::ARM_MOVE_MS)
            {
                if (presenceSensor_.isTriggered())
                {
                    nextState_ = CappingState::LoweringHead;
                }
                else
                {
                    triggerFault(ErrorCode::CapBottleLost);
                }
            }
            break;

        case CappingState::LoweringHead:
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

        case CappingState::TighteningCap:
        {
            const uint16_t currentAdc = chuckMotor_.sampleCurrentAdc();
            if (currentAdc >= ProcessParams::CHUCK_CURRENT_LIMIT_ADC)
            {
                triggerFault(ErrorCode::ChuckOvercurrent);
                break;
            }

            if ((elapsedMs < ProcessParams::CHUCK_MIN_TIGHTEN_MS) &&
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
            if (elapsedMs >= ProcessParams::RELEASE_MS)
            {
                nextState_ = CappingState::Idle;
            }
            break;
    }
}

void CappingAutomaton::onExit(CappingState state)
{
    if (state == CappingState::TighteningCap)
    {
        // Garantizar el frenado dinámico del cabezal al salir de la fase de apriete
        chuckMotor_.brake();
    }
}

void CappingAutomaton::triggerFault(ErrorCode error)
{
    activeError_ = error;
    chuckMotor_.stop();
    zAxis_.stop();
}
