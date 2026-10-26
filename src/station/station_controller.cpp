#include "station_controller.hpp"
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"

namespace
{
    BottleGateConfig makeFillGateConfig()
    {
        BottleGateConfig cfg{};
        cfg.stopPin      = HardwarePins::PIN_SERVO_FILL_STOP;
        cfg.clampPin     = HardwarePins::PIN_SERVO_FILL_CLAMP;
        cfg.stopUpDeg    = ProcessParams::FILL_STOP_UP_DEG;
        cfg.stopDownDeg  = ProcessParams::FILL_STOP_DOWN_DEG;
        cfg.clampUpDeg   = ProcessParams::FILL_CLAMP_UP_DEG;
        cfg.clampDownDeg = ProcessParams::FILL_CLAMP_DOWN_DEG;
        cfg.armMoveMs    = ProcessParams::ARM_MOVE_MS;
        cfg.releaseGapMs = ProcessParams::RELEASE_GAP_MS;
        return cfg;
    }

    BottleGateConfig makeCapGateConfig()
    {
        BottleGateConfig cfg{};
        cfg.stopPin      = HardwarePins::PIN_SERVO_CAP_STOP;
        cfg.clampPin     = HardwarePins::PIN_SERVO_CAP_CLAMP;
        cfg.stopUpDeg    = ProcessParams::CAP_STOP_UP_DEG;
        cfg.stopDownDeg  = ProcessParams::CAP_STOP_DOWN_DEG;
        cfg.clampUpDeg   = ProcessParams::CAP_CLAMP_UP_DEG;
        cfg.clampDownDeg = ProcessParams::CAP_CLAMP_DOWN_DEG;
        cfg.armMoveMs    = ProcessParams::ARM_MOVE_MS;
        cfg.releaseGapMs = ProcessParams::RELEASE_GAP_MS;
        return cfg;
    }

    StepperAxisConfig makeFillZConfig()
    {
        StepperAxisConfig cfg{};
        cfg.slot                 = StepperSlot::FillZ;
        cfg.stepPin              = HardwarePins::PIN_STEPPER_FILL_STEP;
        cfg.dirPin               = HardwarePins::PIN_STEPPER_FILL_DIR;
        cfg.enPin                = HardwarePins::PIN_STEPPER_FILL_EN;
        cfg.limitSwitchPin       = HardwarePins::PIN_LIMIT_SWITCH_FILL;
        cfg.maxTravelSteps       = ProcessParams::FILL_Z_MAX_TRAVEL_STEPS;
        cfg.accelStepsPerSec2    = ProcessParams::Z_ACCEL;
        cfg.homeBackoffSteps     = ProcessParams::Z_HOME_BACKOFF_STEPS;
        cfg.dirDownHigh          = true;
        cfg.enableActiveLow      = true;
        cfg.limitSwitchActiveLow = true;
        return cfg;
    }

    StepperAxisConfig makeCapZConfig()
    {
        StepperAxisConfig cfg{};
        cfg.slot                 = StepperSlot::CapZ;
        cfg.stepPin              = HardwarePins::PIN_STEPPER_CAP_STEP;
        cfg.dirPin               = HardwarePins::PIN_STEPPER_CAP_DIR;
        cfg.enPin                = HardwarePins::PIN_STEPPER_CAP_EN;
        cfg.limitSwitchPin       = HardwarePins::PIN_LIMIT_SWITCH_CAP;
        cfg.maxTravelSteps       = ProcessParams::CAP_Z_MAX_TRAVEL_STEPS;
        cfg.accelStepsPerSec2    = ProcessParams::Z_ACCEL;
        cfg.homeBackoffSteps     = ProcessParams::Z_HOME_BACKOFF_STEPS;
        cfg.dirDownHigh          = true;
        cfg.enableActiveLow      = true;
        cfg.limitSwitchActiveLow = true;
        return cfg;
    }

    ConveyorConfig makeConveyorConfig()
    {
        ConveyorConfig cfg{};
        cfg.rpwmPin     = HardwarePins::PIN_BTS_CONV_RPWM;
        cfg.enPin       = HardwarePins::PIN_BTS_CONV_EN;
        cfg.encPinA     = HardwarePins::PIN_ENC_CONV_A;
        cfg.encPinB     = HardwarePins::PIN_ENC_CONV_B;
        cfg.encoderPpr  = ProcessParams::BELT_ENCODER_PPR;
        cfg.targetRpm   = ProcessParams::BELT_TARGET_RPM;
        cfg.kpTenths    = ProcessParams::BELT_PI_KP;
        cfg.kiTenths    = ProcessParams::BELT_PI_KI;
        cfg.stallTimeMs = ProcessParams::BELT_STALL_MS;
        cfg.stallMinPwm = ProcessParams::BELT_STALL_MIN_PWM;
        return cfg;
    }
}

StationController::StationController()
    : presenceFill_(
          HardwarePins::PIN_PRESENCE_FILL,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          false,
          true),
      presenceCap_(
          HardwarePins::PIN_PRESENCE_CAP,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          false,
          true),
      limitSwitchFill_(
          HardwarePins::PIN_LIMIT_SWITCH_FILL,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          false,
          true),
      limitSwitchCap_(
          HardwarePins::PIN_LIMIT_SWITCH_CAP,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          false,
          true),
      estopAux_(
          HardwarePins::PIN_ESTOP_AUX,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          true,
          true),
      btnReset_(
          HardwarePins::PIN_BTN_RESET,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          false,
          true),
      fillGate_(makeFillGateConfig()),
      capGate_(makeCapGateConfig()),
      fillZ_(makeFillZConfig()),
      capZ_(makeCapZConfig()),
      fillPump_(
          HardwarePins::PIN_PUMP_FILL,
          ProcessParams::MAX_PUMP_ON_MS,
          true),
      chuckMotor_(
          HardwarePins::PIN_BTS_CHUCK_RPWM,
          HardwarePins::PIN_BTS_CHUCK_EN,
          HardwarePins::PIN_BTS_CHUCK_LPWM,
          HardwarePins::PIN_ADC_CHUCK_CURRENT),
      chuckEncoder_(
          EncoderSlot::Chuck,
          HardwarePins::PIN_ENC_CHUCK_A,
          HardwarePins::PIN_ENC_CHUCK_B,
          ProcessParams::CHUCK_ENCODER_PPR,
          true),
      conveyor_(makeConveyorConfig()),
      buzzer_(HardwarePins::PIN_BUZZER, true),
      fillingAutomaton_(
          presenceFill_,
          limitSwitchFill_,
          fillGate_,
          fillZ_,
          fillPump_),
      cappingAutomaton_(
          presenceCap_,
          limitSwitchCap_,
          capGate_,
          capZ_,
          chuckMotor_,
          chuckEncoder_),
      mode_(StationMode::Boot),
      activeError_(ErrorCode::None),
      profile_(BottleProfile::Small),
      interstageQueueCount_(0),
      bottleCount_(0),
      bootStartTimeMs_(0),
      networkWarningActive_(false),
      lastSnapshot_{
          ErrorCode::None,
          StationMode::Boot,
          FillState::Idle,
          CappingState::Idle,
          BottleProfile::Small,
          0,
          0},
      snapshotDirty_(true)
{
}

void StationController::init()
{
    pinMode(HardwarePins::PIN_STATUS_LED, OUTPUT);
    digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);

    presenceFill_.init();
    presenceCap_.init();
    limitSwitchFill_.init();
    limitSwitchCap_.init();
    estopAux_.init();
    btnReset_.init();

    fillPump_.init();
    chuckMotor_.init();
    chuckEncoder_.init();
    conveyor_.init();
    buzzer_.init();

    fillGate_.init();
    capGate_.init();
    fillZ_.init();
    capZ_.init();

    fillingAutomaton_.setProfile(profile_);
    cappingAutomaton_.setProfile(profile_);

    if (estopAux_.isTriggered())
    {
        enterErrorStop(ErrorCode::EmergencyStopPressed);
    }
    else
    {
        enterBootMode();
    }
}

void StationController::update()
{
    const uint32_t nowMs = millis();

    // 1. Muestreo no bloqueante de entradas digitales y tareas de seguridad locales
    presenceFill_.update();
    presenceCap_.update();
    limitSwitchFill_.update();
    limitSwitchCap_.update();
    estopAux_.update();
    btnReset_.update();

    fillPump_.update();
    fillGate_.update();
    capGate_.update();

    // 2. Prioridad absoluta: vigilancia del contacto auxiliar de la seta física E-STOP
    if (estopAux_.isTriggered())
    {
        if (mode_ != StationMode::ErrorStop || activeError_ != ErrorCode::EmergencyStopPressed)
        {
            enterErrorStop(ErrorCode::EmergencyStopPressed);
        }
        buzzer_.setPattern(BuzzerPattern::EmergencyStop, nowMs);
        buzzer_.update(nowMs);
        refreshDisplaySnapshot();
        return;
    }

    // 3. Pulsador físico de rearme en paralelo al botón táctil [REARME]
    if (btnReset_.onRisingEdge())
    {
        handleTouchCommand(TouchCommand::AcknowledgeAlarm);
    }

    // 4. Máquina supervisora del modo global de la estación
    switch (mode_)
    {
        case StationMode::Boot:
            if (fillZ_.hasSoftLimitTripped())
            {
                enterErrorStop(ErrorCode::FillZSoftLimitHit);
            }
            else if (capZ_.hasSoftLimitTripped())
            {
                enterErrorStop(ErrorCode::CapZSoftLimitHit);
            }
            else if (fillZ_.isHome() && capZ_.isHome())
            {
                enterRunningMode();
            }
            else if (nowMs - bootStartTimeMs_ >= ProcessParams::Z_HOMING_TIMEOUT_MS)
            {
                if (!fillZ_.isHome())
                {
                    enterErrorStop(ErrorCode::FillZHomeFailed);
                }
                else
                {
                    enterErrorStop(ErrorCode::CapZHomeFailed);
                }
            }
            break;

        case StationMode::Running:
            if (conveyor_.isStalled())
            {
                enterErrorStop(ErrorCode::ConveyorStall);
                break;
            }

            // Control de flujo de la cola intermedia entre Llenado y Tapado
            fillingAutomaton_.setReleasePermitted(
                interstageQueueCount_ < ProcessParams::MAX_INTERSTAGE_QUEUE);

            fillingAutomaton_.update();
            cappingAutomaton_.update();

            if (fillingAutomaton_.getError() != ErrorCode::None)
            {
                enterErrorStop(fillingAutomaton_.getError());
                break;
            }

            if (cappingAutomaton_.getError() != ErrorCode::None)
            {
                enterErrorStop(cappingAutomaton_.getError());
                break;
            }

            if (fillingAutomaton_.consumeBottleReleasedEvent())
            {
                if (interstageQueueCount_ < 0xFFU)
                {
                    interstageQueueCount_++;
                }
            }

            if (cappingAutomaton_.consumeBottleCompletedEvent())
            {
                bottleCount_++;
                if (interstageQueueCount_ > 0U)
                {
                    interstageQueueCount_--;
                }
            }

            // En operación nominal, mantener BootReady si aún está sonando, o reflejar advertencia 902
            if (buzzer_.getPattern() != BuzzerPattern::BootReady)
            {
                const BuzzerPattern runPattern =
                    networkWarningActive_ ? BuzzerPattern::Warning : BuzzerPattern::Silent;
                buzzer_.setPattern(runPattern, nowMs);
            }
            break;

        case StationMode::ErrorStop:
            // Mantener patrón acústico de fallo mientras la alarma no haya sido reconocida
            buzzer_.setPattern(BuzzerPattern::Fault, nowMs);
            break;

        case StationMode::ManualRecovery:
            // En recuperación manual el buzzer permanece en silencio para asistir al operador
            break;
    }

    buzzer_.update(nowMs);
    refreshDisplaySnapshot();
}

void StationController::updateMotionProfiles()
{
    fillZ_.updateMotionProfile();
    capZ_.updateMotionProfile();
    chuckEncoder_.updateSpeed();
    conveyor_.update();
}

void StationController::handleTouchCommand(TouchCommand cmd)
{
    // Interbloqueo estricto: descartar cualquier comando si la seta E-STOP permanece activa
    if (cmd == TouchCommand::None || estopAux_.isTriggered())
    {
        return;
    }

    switch (cmd)
    {
        case TouchCommand::CycleBottleProfile:
            if (mode_ == StationMode::Running &&
                fillingAutomaton_.isIdle() &&
                cappingAutomaton_.isIdle() &&
                interstageQueueCount_ == 0U)
            {
                profile_ = (profile_ == BottleProfile::Small)
                               ? BottleProfile::Large
                               : BottleProfile::Small;
                fillingAutomaton_.setProfile(profile_);
                cappingAutomaton_.setProfile(profile_);
            }
            break;

        case TouchCommand::AcknowledgeAlarm:
            if (mode_ == StationMode::ErrorStop)
            {
                mode_ = StationMode::ManualRecovery;
                buzzer_.silence();
            }
            break;

        case TouchCommand::JogFillZUp:
            if (mode_ == StationMode::ManualRecovery)
            {
                fillZ_.jogUp(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            }
            break;

        case TouchCommand::JogCapZUp:
            if (mode_ == StationMode::ManualRecovery)
            {
                capZ_.jogUp(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            }
            break;

        case TouchCommand::ReleaseArms:
            if (mode_ == StationMode::ManualRecovery)
            {
                fillGate_.raiseArmsImmediate();
                capGate_.raiseArmsImmediate();
            }
            break;

        case TouchCommand::ConfirmLineClear:
            if (mode_ == StationMode::ManualRecovery)
            {
                enterBootMode();
            }
            break;

        case TouchCommand::None:
        default:
            break;
    }

    refreshDisplaySnapshot();
}

void StationController::setNetworkLinkLost(bool lost)
{
    if (networkWarningActive_ != lost)
    {
        networkWarningActive_ = lost;
        refreshDisplaySnapshot();
    }
}

bool StationController::consumeDisplaySnapshot(DisplaySnapshot& out)
{
    if (!snapshotDirty_)
    {
        return false;
    }

    out = lastSnapshot_;
    snapshotDirty_ = false;
    return true;
}

TelemetrySnapshot StationController::getTelemetry() const
{
    TelemetrySnapshot snap{};
    snap.error        = computeEffectiveError();
    snap.mode         = mode_;
    snap.fillState    = fillingAutomaton_.getState();
    snap.capState     = cappingAutomaton_.getState();
    snap.profile      = profile_;
    snap.queueCount   = interstageQueueCount_;
    snap.beltRpm      = conveyor_.getSpeedRpm();
    snap.bottleCount  = bottleCount_;
    snap.presenceFill = presenceFill_.isTriggered();
    snap.presenceCap  = presenceCap_.isTriggered();
    snap.estopActive  = estopAux_.isTriggered();
    return snap;
}

StationMode StationController::getMode() const
{
    return mode_;
}

ErrorCode StationController::getActiveError() const
{
    return computeEffectiveError();
}

uint8_t StationController::getQueueCount() const
{
    return interstageQueueCount_;
}

void StationController::enterBootMode()
{
    mode_ = StationMode::Boot;
    activeError_ = ErrorCode::None;
    interstageQueueCount_ = 0;
    bootStartTimeMs_ = millis();

    digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);
    buzzer_.silence();

    fillPump_.off();
    chuckMotor_.stop();
    conveyor_.stop();
    conveyor_.clearStall();

    fillingAutomaton_.reset();
    cappingAutomaton_.reset();

    fillZ_.clearSoftLimitFault();
    capZ_.clearSoftLimitFault();
    fillZ_.home(ProcessParams::Z_HOMING_SPEED);
    capZ_.home(ProcessParams::Z_HOMING_SPEED);

    refreshDisplaySnapshot();
}

void StationController::enterRunningMode()
{
    mode_ = StationMode::Running;
    activeError_ = ErrorCode::None;
    interstageQueueCount_ = 0;

    fillingAutomaton_.reset();
    cappingAutomaton_.reset();
    fillingAutomaton_.setProfile(profile_);
    cappingAutomaton_.setProfile(profile_);

    conveyor_.clearStall();
    conveyor_.start();

    digitalWrite(HardwarePins::PIN_STATUS_LED, HIGH);
    buzzer_.triggerBootReady(millis());

    refreshDisplaySnapshot();
}

void StationController::enterErrorStop(ErrorCode error)
{
    mode_ = StationMode::ErrorStop;

    // Retener el primer fallo crítico que provocó la parada, salvo que E-STOP tome prioridad
    if (activeError_ == ErrorCode::None || error == ErrorCode::EmergencyStopPressed)
    {
        activeError_ = error;
    }

    digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);

    fillPump_.off();
    chuckMotor_.stop();
    conveyor_.brake();

    fillingAutomaton_.stopImmediate();
    cappingAutomaton_.stopImmediate();

    fillZ_.stop();
    capZ_.stop();
    fillZ_.setDriverEnabled(false);
    capZ_.setDriverEnabled(false);

    const uint32_t nowMs = millis();
    if (activeError_ == ErrorCode::EmergencyStopPressed)
    {
        buzzer_.setPattern(BuzzerPattern::EmergencyStop, nowMs);
    }
    else
    {
        buzzer_.setPattern(BuzzerPattern::Fault, nowMs);
    }

    refreshDisplaySnapshot();
}

void StationController::refreshDisplaySnapshot()
{
    const ErrorCode effectiveErr = computeEffectiveError();
    const FillState fState       = fillingAutomaton_.getState();
    const CappingState cState    = cappingAutomaton_.getState();

    if (lastSnapshot_.error != effectiveErr ||
        lastSnapshot_.mode != mode_ ||
        lastSnapshot_.fillState != fState ||
        lastSnapshot_.capState != cState ||
        lastSnapshot_.profile != profile_ ||
        lastSnapshot_.queueCount != interstageQueueCount_ ||
        lastSnapshot_.bottleCount != bottleCount_)
    {
        lastSnapshot_.error       = effectiveErr;
        lastSnapshot_.mode        = mode_;
        lastSnapshot_.fillState   = fState;
        lastSnapshot_.capState    = cState;
        lastSnapshot_.profile     = profile_;
        lastSnapshot_.queueCount  = interstageQueueCount_;
        lastSnapshot_.bottleCount = bottleCount_;
        snapshotDirty_            = true;
    }
}

ErrorCode StationController::computeEffectiveError() const
{
    if (activeError_ != ErrorCode::None)
    {
        return activeError_;
    }

    if (networkWarningActive_)
    {
        return ErrorCode::NetworkLinkLost;
    }

    return ErrorCode::None;
}
