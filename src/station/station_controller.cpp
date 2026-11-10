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
        cfg.rpwmPin            = HardwarePins::PIN_BTS_CONV_RPWM;
        cfg.enPin              = HardwarePins::PIN_BTS_CONV_EN;
        cfg.currentSensePin    = HardwarePins::PIN_ADC_CONV_CURRENT;
        cfg.targetPwm          = ProcessParams::BELT_PWM;
        cfg.maxBoostPwm        = ProcessParams::BELT_MAX_BOOST_PWM;
        cfg.enableIrComp       = ProcessParams::ENABLE_BELT_IR_COMP;
        cfg.idleCurrentAdc     = ProcessParams::BELT_IDLE_CURRENT_ADC;
        cfg.fullLoadCurrentAdc = ProcessParams::BELT_FULL_LOAD_ADC;
        cfg.accelStepPwm       = ProcessParams::BELT_ACCEL_PWM_STEP;
        cfg.decelStepPwm       = ProcessParams::BELT_DECEL_PWM_STEP;
        cfg.currentLimitAdc    = ProcessParams::BELT_CURRENT_LIMIT_ADC;
        cfg.stallTimeMs        = ProcessParams::BELT_STALL_MS;
        return cfg;
    }

    void writeTowerRelayPin(uint8_t pin, bool turnOn)
    {
        const bool levelHigh = ProcessParams::TOWER_RELAY_ACTIVE_LOW ? !turnOn : turnOn;
        digitalWrite(pin, levelHigh ? HIGH : LOW);
    }

    void setTowerLights(bool greenOn, bool redOn)
    {
        writeTowerRelayPin(HardwarePins::PIN_TOWER_GREEN, greenOn);
        writeTowerRelayPin(HardwarePins::PIN_TOWER_RED, redOn);
    }
}

StationController::StationController()
    : presenceFill_(
          HardwarePins::PIN_PRESENCE_FILL,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          true,
          false),
      positionFill_(
          HardwarePins::PIN_POS_FILL,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          true,
          false),
      presenceCap_(
          HardwarePins::PIN_PRESENCE_CAP,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          true,
          false),
      positionCap_(
          HardwarePins::PIN_POS_CAP,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          true,
          false),
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
      btnStart_(
          HardwarePins::PIN_BTN_START,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          false,
          true),
      btnStop_(
          HardwarePins::PIN_BTN_STOP,
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
          DcMotorBts7960::UNUSED_PIN,
          HardwarePins::PIN_ADC_CHUCK_CURRENT),
      conveyor_(makeConveyorConfig()),
      buzzer_(HardwarePins::PIN_BUZZER, !ProcessParams::TOWER_RELAY_ACTIVE_LOW),
      fillingAutomaton_(
          presenceFill_,
          positionFill_,
          limitSwitchFill_,
          fillGate_,
          fillZ_,
          fillPump_),
      cappingAutomaton_(
          presenceCap_,
          positionCap_,
          limitSwitchCap_,
          capGate_,
          capZ_,
          chuckMotor_),
      mode_(StationMode::Boot),
      activeError_(ErrorCode::None),
      profile_(BottleProfile::Small),
      interstageQueueCount_(0),
      bottleCount_(0),
      bootStartTimeMs_(0),
      linePaused_(false),
      networkWarningActive_(false),
      testFlags_(0U),
      lastSnapshot_{
          ErrorCode::None,
          StationMode::Boot,
          FillState::Idle,
          CappingState::Idle,
          BottleProfile::Small,
          0,
          0,
          0U,
          0,
          0,
          0U,
          0U},
      snapshotDirty_(true)
{
}

void StationController::init()
{
    // Inicializar indicadores lumínicos en estado apagado
    setTowerLights(false, false);
    pinMode(HardwarePins::PIN_TOWER_GREEN, OUTPUT);
    pinMode(HardwarePins::PIN_TOWER_RED, OUTPUT);
    setTowerLights(false, false);

    presenceFill_.init();
    positionFill_.init();
    presenceCap_.init();
    positionCap_.init();
    limitSwitchFill_.init();
    limitSwitchCap_.init();
    btnStart_.init();
    btnStop_.init();
    estopAux_.init();
    btnReset_.init();

    fillPump_.init();
    chuckMotor_.init();
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
    positionFill_.update();
    presenceCap_.update();
    positionCap_.update();
    limitSwitchFill_.update();
    limitSwitchCap_.update();
    btnStart_.update();
    btnStop_.update();
    estopAux_.update();
    btnReset_.update();

    fillPump_.update();
    fillGate_.update();
    capGate_.update();

    // 2. Prioridad de seguridad: contacto de parada de emergencia (en TestMode permite operar en banco)
    if (estopAux_.isTriggered() && mode_ != StationMode::TestMode)
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

    // 3. Pulsadores de control y rearme
    if (btnReset_.onRisingEdge())
    {
        handleTouchCommand(TouchCommand::AcknowledgeAlarm);
    }

    if (btnStop_.onRisingEdge() && mode_ == StationMode::Running)
    {
        linePaused_ = true;
        conveyor_.pauseSmooth();
        setTowerLights(false, true);
    }

    if (btnStart_.onRisingEdge())
    {
        if (mode_ == StationMode::Running && linePaused_)
        {
            linePaused_ = false;
            setTowerLights(true, false);
        }
        else if (mode_ == StationMode::ManualRecovery)
        {
            handleTouchCommand(TouchCommand::ConfirmLineClear);
        }
    }

    // 4. Máquina supervisora del modo global de la estación
    switch (mode_)
    {
        case StationMode::Boot:
            if (fillZ_.hasSoftLimitTripped())
            {
                enterErrorStop(ErrorCode::FillZSoftLimitHit);
            }
            else if (ProcessParams::ENABLE_CAPPING_STATION && capZ_.hasSoftLimitTripped())
            {
                enterErrorStop(ErrorCode::CapZSoftLimitHit);
            }
            else if (fillZ_.isHome() && (!ProcessParams::ENABLE_CAPPING_STATION || capZ_.isHome()))
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
        {
            if (conveyor_.isStalled())
            {
                enterErrorStop(ErrorCode::ConveyorStall);
                break;
            }

            // Control de flujo de la cola intermedia entre Llenado y Tapado
            const bool canReleaseFromFill =
                !ProcessParams::ENABLE_INTERSTAGE_QUEUE ||
                (interstageQueueCount_ < ProcessParams::MAX_INTERSTAGE_QUEUE);
            fillingAutomaton_.setReleasePermitted(canReleaseFromFill);

            fillingAutomaton_.update();
            if (ProcessParams::ENABLE_CAPPING_STATION)
            {
                cappingAutomaton_.update();
            }

            if (fillingAutomaton_.getError() != ErrorCode::None)
            {
                enterErrorStop(fillingAutomaton_.getError());
                break;
            }

            if (ProcessParams::ENABLE_CAPPING_STATION &&
                cappingAutomaton_.getError() != ErrorCode::None)
            {
                enterErrorStop(cappingAutomaton_.getError());
                break;
            }

            // Coordinación de avance de banda: se pausa por solicitud de STOP o durante
            // la carrera de descenso del brazo de sujeción para fijar la botella.
            const bool pauseBeltForClamping =
                linePaused_ ||
                (fillingAutomaton_.getState() == FillState::ClampArmDown) ||
                (ProcessParams::ENABLE_CAPPING_STATION &&
                 (cappingAutomaton_.getState() == CappingState::ClampArmDown));

            if (pauseBeltForClamping)
            {
                if (conveyor_.isRunning())
                {
                    conveyor_.pauseSmooth();
                }
            }
            else
            {
                if (!conveyor_.isRunning())
                {
                    conveyor_.start();
                }
            }

            if (fillingAutomaton_.consumeBottleReleasedEvent())
            {
                if (ProcessParams::ENABLE_INTERSTAGE_QUEUE)
                {
                    if (interstageQueueCount_ < 0xFFU)
                    {
                        interstageQueueCount_++;
                    }
                }
                else if (!ProcessParams::ENABLE_CAPPING_STATION)
                {
                    bottleCount_++;
                }
            }

            if (ProcessParams::ENABLE_CAPPING_STATION &&
                cappingAutomaton_.consumeBottleCompletedEvent())
            {
                bottleCount_++;
                if (ProcessParams::ENABLE_INTERSTAGE_QUEUE && interstageQueueCount_ > 0U)
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
        }

        case StationMode::ErrorStop:
            // Mantener patrón acústico de fallo mientras la alarma no haya sido reconocida
            buzzer_.setPattern(BuzzerPattern::Fault, nowMs);
            break;

        case StationMode::ManualRecovery:
            // En recuperación manual el buzzer permanece en silencio para asistir al operador
            break;

        case StationMode::TestMode:
            // Sincronizar bandera de bomba si se apagó automáticamente por MAX_PUMP_ON_MS
            if (!fillPump_.isOn() && ((testFlags_ & TestFlags::FILL_PUMP_ON) != 0U))
            {
                testFlags_ &= static_cast<uint16_t>(~TestFlags::FILL_PUMP_ON);
            }
            if (conveyor_.isStalled())
            {
                conveyor_.stop();
                conveyor_.clearStall();
                testFlags_ &= static_cast<uint16_t>(~TestFlags::BELT_ON);
            }
            break;
    }

    if (mode_ != StationMode::TestMode)
    {
        buzzer_.update(nowMs);
    }
    refreshDisplaySnapshot();
}

void StationController::updateMotionProfiles()
{
    fillZ_.updateMotionProfile();
    capZ_.updateMotionProfile();
    conveyor_.update();
}

void StationController::handleTouchCommand(TouchCommand cmd)
{
    if (cmd == TouchCommand::None)
    {
        return;
    }

    // Permitir entrar/salir de TestMode y cambiar perfil sin importar el estado de E-STOP en pruebas de banco
    if (estopAux_.isTriggered() &&
        (mode_ != StationMode::TestMode) &&
        (cmd != TouchCommand::ToggleTestMode) &&
        (cmd != TouchCommand::CycleBottleProfile))
    {
        return;
    }

    switch (cmd)
    {
        case TouchCommand::CycleBottleProfile:
            if (fillingAutomaton_.isIdle() &&
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
            if (mode_ == StationMode::ManualRecovery || mode_ == StationMode::TestMode)
            {
                fillZ_.jogUp(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            }
            break;

        case TouchCommand::JogCapZUp:
            if (mode_ == StationMode::ManualRecovery || mode_ == StationMode::TestMode)
            {
                capZ_.jogUp(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            }
            break;

        case TouchCommand::ReleaseArms:
            if (mode_ == StationMode::ManualRecovery || mode_ == StationMode::TestMode)
            {
                fillGate_.raiseArmsImmediate();
                capGate_.raiseArmsImmediate();
                testFlags_ &= static_cast<uint16_t>(~(TestFlags::FILL_STOP_DN | TestFlags::FILL_CLAMP_DN |
                                                      TestFlags::CAP_STOP_DN  | TestFlags::CAP_CLAMP_DN));
            }
            break;

        case TouchCommand::ConfirmLineClear:
            if (mode_ == StationMode::ManualRecovery)
            {
                enterBootMode();
            }
            break;

        case TouchCommand::ToggleTestMode:
            if (mode_ == StationMode::TestMode)
            {
                if (estopAux_.isTriggered())
                {
                    enterErrorStop(ErrorCode::EmergencyStopPressed);
                }
                else
                {
                    enterBootMode();
                }
            }
            else
            {
                enterTestMode();
            }
            break;

        case TouchCommand::TestToggleBelt:
            if (mode_ == StationMode::TestMode)
            {
                if ((testFlags_ & TestFlags::BELT_ON) != 0U)
                {
                    testFlags_ &= static_cast<uint16_t>(~TestFlags::BELT_ON);
                    conveyor_.stop();
                }
                else
                {
                    testFlags_ |= TestFlags::BELT_ON;
                    conveyor_.clearStall();
                    conveyor_.start();
                }
            }
            break;

        case TouchCommand::TestToggleTowerGreen:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::TOWER_GREEN_ON;
                writeTowerRelayPin(HardwarePins::PIN_TOWER_GREEN, (testFlags_ & TestFlags::TOWER_GREEN_ON) != 0U);
            }
            break;

        case TouchCommand::TestToggleTowerRed:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::TOWER_RED_ON;
                writeTowerRelayPin(HardwarePins::PIN_TOWER_RED, (testFlags_ & TestFlags::TOWER_RED_ON) != 0U);
            }
            break;

        case TouchCommand::TestToggleBuzzer:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::BUZZER_ON;
                const bool turnOn = (testFlags_ & TestFlags::BUZZER_ON) != 0U;
                const bool buzzerActiveLow = ProcessParams::TOWER_RELAY_ACTIVE_LOW;
                const bool levelHigh = buzzerActiveLow ? !turnOn : turnOn;
                digitalWrite(HardwarePins::PIN_BUZZER, levelHigh ? HIGH : LOW);
            }
            break;

        case TouchCommand::TestToggleFillStopArm:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::FILL_STOP_DN;
                if ((testFlags_ & TestFlags::FILL_STOP_DN) != 0U)
                {
                    fillGate_.lowerStopArm();
                }
                else
                {
                    fillGate_.raiseStopArm();
                }
            }
            break;

        case TouchCommand::TestToggleFillClampArm:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::FILL_CLAMP_DN;
                if ((testFlags_ & TestFlags::FILL_CLAMP_DN) != 0U)
                {
                    fillGate_.lowerClampArm();
                }
                else
                {
                    fillGate_.raiseClampArm();
                }
            }
            break;

        case TouchCommand::TestToggleFillPump:
            if (mode_ == StationMode::TestMode)
            {
                if ((testFlags_ & TestFlags::FILL_PUMP_ON) != 0U)
                {
                    testFlags_ &= static_cast<uint16_t>(~TestFlags::FILL_PUMP_ON);
                    fillPump_.off();
                }
                else
                {
                    testFlags_ |= TestFlags::FILL_PUMP_ON;
                    fillPump_.on();
                }
            }
            break;

        case TouchCommand::TestJogFillZDown:
            if (mode_ == StationMode::TestMode)
            {
                fillZ_.jogDown(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            }
            break;

        case TouchCommand::TestToggleCapStopArm:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::CAP_STOP_DN;
                if ((testFlags_ & TestFlags::CAP_STOP_DN) != 0U)
                {
                    capGate_.lowerStopArm();
                }
                else
                {
                    capGate_.raiseStopArm();
                }
            }
            break;

        case TouchCommand::TestToggleCapClampArm:
            if (mode_ == StationMode::TestMode)
            {
                testFlags_ ^= TestFlags::CAP_CLAMP_DN;
                if ((testFlags_ & TestFlags::CAP_CLAMP_DN) != 0U)
                {
                    capGate_.lowerClampArm();
                }
                else
                {
                    capGate_.raiseClampArm();
                }
            }
            break;

        case TouchCommand::TestToggleChuckMotor:
            if (mode_ == StationMode::TestMode)
            {
                if ((testFlags_ & TestFlags::CAP_CHUCK_ON) != 0U)
                {
                    testFlags_ &= static_cast<uint16_t>(~TestFlags::CAP_CHUCK_ON);
                    chuckMotor_.stop();
                }
                else
                {
                    testFlags_ |= TestFlags::CAP_CHUCK_ON;
                    chuckMotor_.driveForward(ProcessParams::CHUCK_PWM);
                }
            }
            break;

        case TouchCommand::TestJogCapZDown:
            if (mode_ == StationMode::TestMode)
            {
                capZ_.jogDown(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
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
    snap.beltPwm      = conveyor_.getCurrentPwm();
    snap.bottleCount  = bottleCount_;
    snap.presenceFill = positionFill_.isTriggered();
    snap.presenceCap  = positionCap_.isTriggered();
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
    linePaused_ = false;
    testFlags_ = 0U;

    setTowerLights(false, false);
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
    if (ProcessParams::ENABLE_CAPPING_STATION)
    {
        capZ_.home(ProcessParams::Z_HOMING_SPEED);
    }
    else
    {
        capZ_.stop();
        capZ_.setDriverEnabled(false);
    }

    refreshDisplaySnapshot();
}

void StationController::enterRunningMode()
{
    mode_ = StationMode::Running;
    activeError_ = ErrorCode::None;
    interstageQueueCount_ = 0;
    linePaused_ = false;

    fillingAutomaton_.reset();
    cappingAutomaton_.reset();
    fillingAutomaton_.setProfile(profile_);
    cappingAutomaton_.setProfile(profile_);

    conveyor_.clearStall();
    conveyor_.start();

    setTowerLights(true, false);
    buzzer_.triggerBootReady(millis());

    refreshDisplaySnapshot();
}

void StationController::enterErrorStop(ErrorCode error)
{
    mode_ = StationMode::ErrorStop;
    testFlags_ = 0U;

    // Retener el primer fallo crítico que provocó la parada, salvo que E-STOP tome prioridad
    if (activeError_ == ErrorCode::None || error == ErrorCode::EmergencyStopPressed)
    {
        activeError_ = error;
    }

    setTowerLights(false, true);

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

void StationController::enterTestMode()
{
    mode_ = StationMode::TestMode;
    activeError_ = ErrorCode::None;
    interstageQueueCount_ = 0;
    linePaused_ = false;
    testFlags_ = 0U;

    setTowerLights(false, false);
    buzzer_.silence();

    fillPump_.off();
    chuckMotor_.stop();
    conveyor_.stop();
    conveyor_.clearStall();

    fillingAutomaton_.stopImmediate();
    cappingAutomaton_.stopImmediate();

    fillGate_.raiseArmsImmediate();
    capGate_.raiseArmsImmediate();

    fillZ_.stop();
    capZ_.stop();
    fillZ_.setDriverEnabled(true);
    capZ_.setDriverEnabled(true);

    refreshDisplaySnapshot();
}

void StationController::refreshDisplaySnapshot()
{
    const ErrorCode effectiveErr = computeEffectiveError();
    const FillState fState       = fillingAutomaton_.getState();
    const CappingState cState    = cappingAutomaton_.getState();
    const int32_t fSteps         = fillZ_.getCurrentSteps();
    const int32_t cSteps         = capZ_.getCurrentSteps();
    const uint8_t bPwm           = conveyor_.getCurrentPwm();
    const uint16_t bCurrentAdc   = conveyor_.getFilteredCurrentAdc();

    uint16_t effectiveFlags = testFlags_ & static_cast<uint16_t>(~(TestFlags::FILL_HOME_SW | TestFlags::CAP_HOME_SW));
    if (limitSwitchFill_.isTriggered())
    {
        effectiveFlags |= TestFlags::FILL_HOME_SW;
    }
    if (limitSwitchCap_.isTriggered())
    {
        effectiveFlags |= TestFlags::CAP_HOME_SW;
    }

    if (lastSnapshot_.error != effectiveErr ||
        lastSnapshot_.mode != mode_ ||
        lastSnapshot_.fillState != fState ||
        lastSnapshot_.capState != cState ||
        lastSnapshot_.profile != profile_ ||
        lastSnapshot_.queueCount != interstageQueueCount_ ||
        lastSnapshot_.bottleCount != bottleCount_ ||
        lastSnapshot_.testFlags != effectiveFlags ||
        lastSnapshot_.fillZSteps != fSteps ||
        lastSnapshot_.capZSteps != cSteps ||
        lastSnapshot_.beltPwm != bPwm ||
        lastSnapshot_.beltCurrentAdc != bCurrentAdc)
    {
        lastSnapshot_.error          = effectiveErr;
        lastSnapshot_.mode           = mode_;
        lastSnapshot_.fillState      = fState;
        lastSnapshot_.capState       = cState;
        lastSnapshot_.profile        = profile_;
        lastSnapshot_.queueCount     = interstageQueueCount_;
        lastSnapshot_.bottleCount    = bottleCount_;
        lastSnapshot_.testFlags      = effectiveFlags;
        lastSnapshot_.fillZSteps     = fSteps;
        lastSnapshot_.capZSteps      = cSteps;
        lastSnapshot_.beltPwm        = bPwm;
        lastSnapshot_.beltCurrentAdc = bCurrentAdc;
        snapshotDirty_               = true;
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
