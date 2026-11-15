#include "capping_automaton.hpp"
#include "config/process_params.hpp"

CappingAutomaton::CappingAutomaton(
    DigitalSensor& presenceSensor,
    DigitalSensor& positionSensor,
    DigitalSensor& limitSwitch,
    BottleGate& gate,
    StepperAxis& zAxis,
    DcMotorBts7960& chuckMotor)
    : presenceSensor_(presenceSensor),
      positionSensor_(positionSensor),
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
      bottleCompletedEvent_(false),
      engageStarted_(false)
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

    // Vigilancia prioritaria de sobrecarrera reportada por la ISR del Timer3
    if (zAxis_.hasSoftLimitTripped())
    {
        if (ProcessParams::ENABLE_ERR_212_CAP_Z_SOFT_LIMIT)
        {
            triggerFault(ErrorCode::CapZSoftLimitHit);
            return;
        }
        zAxis_.clearSoftLimitFault();
    }

    // Limpiar el bloqueo de re-disparo en cuanto la botella saliente libere ambos sensores ópticos
    if (needsClear_ && !presenceSensor_.isTriggered() && !positionSensor_.isTriggered())
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
                // Si capEngageSteps <= 0, inicia lento y con el motor encendido desde el origen.
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
            // Cuando el 2do sensor (en medio de ambos brazos) detecta la botella, baja el 2do brazo
            if (positionSensor_.isTriggered())
            {
                nextState_ = CappingState::ClampArmDown;
            }
            else if (elapsedMs >= ProcessParams::CLAMP_SENSOR_TIMEOUT_MS)
            {
                triggerFault(ErrorCode::CapBottleLost);
            }
            break;

        case CappingState::ClampArmDown:
            // Mientras el 2do brazo baja (ARM_MOVE_MS), StationController pausa la banda.
            // Al concluir el descenso de ambos brazos, se valida el sensor de posición exacta
            // esperando BOTTLE_LOST_GRACE_MS si se perdió momentáneamente la presencia.
            if (elapsedMs >= ProcessParams::ARM_MOVE_MS)
            {
                if (positionSensor_.isTriggered() || !ProcessParams::ENABLE_ERR_102_CAP_BOTTLE_LOST)
                {
                    nextState_ = CappingState::LoweringHead;
                }
                else if (elapsedMs >= (ProcessParams::ARM_MOVE_MS + ProcessParams::BOTTLE_LOST_GRACE_MS))
                {
                    triggerFault(ErrorCode::CapBottleLost);
                }
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
    if (!ProcessParams::isErrorEnabled(error))
    {
        return;
    }

    activeError_ = error;
    chuckMotor_.stop();
    zAxis_.stop();
}
