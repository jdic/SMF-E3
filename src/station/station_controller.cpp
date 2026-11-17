#include "station_controller.hpp"
#include "station_hardware_factory.hpp"
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"

StationController::StationController()
    : presenceFill_(
          HardwarePins::PIN_PRESENCE_FILL,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          ProcessParams::OPTICAL_ACTIVE_HIGH,
          ProcessParams::OPTICAL_USE_PULLUP),
      positionFill_(
          HardwarePins::PIN_POS_FILL,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          ProcessParams::OPTICAL_ACTIVE_HIGH,
          ProcessParams::OPTICAL_USE_PULLUP),
      presenceCap_(
          HardwarePins::PIN_PRESENCE_CAP,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          ProcessParams::OPTICAL_ACTIVE_HIGH,
          ProcessParams::OPTICAL_USE_PULLUP),
      positionCap_(
          HardwarePins::PIN_POS_CAP,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          ProcessParams::OPTICAL_ACTIVE_HIGH,
          ProcessParams::OPTICAL_USE_PULLUP),
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
      visionSensor_(
          HardwarePins::PIN_VISION_SENSOR,
          ProcessParams::SENSOR_DEBOUNCE_MS,
          ProcessParams::VISION_ALARM_ACTIVE_HIGH,
          false), // usePullup = false (3.3V directo desde ESP32 sin inyectar 5V)
      fillGate_(StationHardwareFactory::makeFillGateConfig()),
      capGate_(StationHardwareFactory::makeCapGateConfig()),
      fillZ_(StationHardwareFactory::makeFillZConfig()),
      capZ_(StationHardwareFactory::makeCapZConfig()),
      fillPump_(
          HardwarePins::PIN_PUMP_FILL,
          ProcessParams::MAX_PUMP_ON_MS,
          !ProcessParams::PUMP_RELAY_ACTIVE_LOW),
      chuckMotor_(
          HardwarePins::PIN_BTS_CHUCK_RPWM,
          HardwarePins::PIN_BTS_CHUCK_EN,
          DcMotorBts7960::UNUSED_PIN,
          HardwarePins::PIN_ADC_CHUCK_CURRENT),
      conveyor_(StationHardwareFactory::makeConveyorConfig()),
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
          visionSensor_,
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
          0U,
          0U,
          0U,
          0U,
          ProcessParams::defaultConfigFlags()},
      snapshotDirty_(true)
{
}

void StationController::init()
{
    StationHardwareFactory::setTowerLights(false, false);
    pinMode(HardwarePins::PIN_TOWER_GREEN, OUTPUT);
    pinMode(HardwarePins::PIN_TOWER_RED, OUTPUT);
    StationHardwareFactory::setTowerLights(false, false);

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
    visionSensor_.init();

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

    if (ProcessParams::isErrorEnabled(ErrorCode::EmergencyStopPressed) && estopAux_.isTriggered())
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
    visionSensor_.update();

    fillPump_.update();
    fillGate_.update();
    capGate_.update();

    // 2. Prioridad de seguridad: contacto de parada de emergencia (en TestMode permite operar en banco)
    if (ProcessParams::isErrorEnabled(ErrorCode::EmergencyStopPressed) &&
        estopAux_.isTriggered() &&
        mode_ != StationMode::TestMode)
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

    // 3. Pulsadores físicos de control y rearme
    if (btnReset_.onRisingEdge())
    {
        handleTouchCommand(TouchCommand::AcknowledgeAlarm);
    }

    if (btnStop_.onRisingEdge() && mode_ == StationMode::Running)
    {
        linePaused_ = true;
        conveyor_.pauseSmooth();
        StationHardwareFactory::setTowerLights(false, true);
    }

    if (btnStart_.onRisingEdge())
    {
        if (mode_ == StationMode::Running && cappingAutomaton_.acknowledgeVisionAlarm())
        {
            activeError_ = ErrorCode::None;
            if (!linePaused_)
            {
                StationHardwareFactory::setTowerLights(true, false);
            }
            buzzer_.silence();
        }
        else if (mode_ == StationMode::Running && linePaused_)
        {
            linePaused_ = false;
            StationHardwareFactory::setTowerLights(true, false);
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
            updateBootMode(nowMs);
            break;

        case StationMode::Running:
            updateRunningMode(nowMs);
            break;

        case StationMode::ErrorStop:
            buzzer_.setPattern(BuzzerPattern::Fault, nowMs);
            break;

        case StationMode::ManualRecovery:
            break;

        case StationMode::TestMode:
            updateTestMode();
            break;
    }

    if (mode_ != StationMode::TestMode)
    {
        buzzer_.update(nowMs);
    }
    refreshDisplaySnapshot();
}

void StationController::updateBootMode(uint32_t nowMs)
{
    const bool fillEnabled = ProcessParams::hasConfigFlag(ConfigFlags::FILL_STATION);
    const bool capEnabled  = ProcessParams::hasConfigFlag(ConfigFlags::CAP_STATION);

    if (fillEnabled && fillZ_.hasSoftLimitTripped())
    {
        if (ProcessParams::isErrorEnabled(ErrorCode::FillZSoftLimitHit))
        {
            enterErrorStop(ErrorCode::FillZSoftLimitHit);
        }
        else
        {
            fillZ_.clearSoftLimitFault();
        }
    }
    else if (capEnabled && capZ_.hasSoftLimitTripped())
    {
        if (ProcessParams::isErrorEnabled(ErrorCode::CapZSoftLimitHit))
        {
            enterErrorStop(ErrorCode::CapZSoftLimitHit);
        }
        else
        {
            capZ_.clearSoftLimitFault();
        }
    }
    else if ((!fillEnabled || fillZ_.isHome()) &&
             (!capEnabled || capZ_.isHome()))
    {
        enterRunningMode();
    }
    else if (nowMs - bootStartTimeMs_ >= ProcessParams::Z_HOMING_TIMEOUT_MS)
    {
        if (fillEnabled && !fillZ_.isHome())
        {
            if (ProcessParams::isErrorEnabled(ErrorCode::FillZHomeFailed))
            {
                enterErrorStop(ErrorCode::FillZHomeFailed);
            }
            else
            {
                enterRunningMode();
            }
        }
        else if (capEnabled && !capZ_.isHome())
        {
            if (ProcessParams::isErrorEnabled(ErrorCode::CapZHomeFailed))
            {
                enterErrorStop(ErrorCode::CapZHomeFailed);
            }
            else
            {
                enterRunningMode();
            }
        }
        else
        {
            enterRunningMode();
        }
    }
}

void StationController::updateRunningMode(uint32_t nowMs)
{
    const bool fillEnabled = ProcessParams::hasConfigFlag(ConfigFlags::FILL_STATION);
    const bool capEnabled  = ProcessParams::hasConfigFlag(ConfigFlags::CAP_STATION);
    const bool queueEnabled = ProcessParams::hasConfigFlag(ConfigFlags::INTERSTAGE_QUEUE);

    if (conveyor_.isStalled())
    {
        if (ProcessParams::isErrorEnabled(ErrorCode::ConveyorStall))
        {
            enterErrorStop(ErrorCode::ConveyorStall);
            return;
        }
        conveyor_.clearStall();
    }

    const bool visionFaultBefore =
        capEnabled &&
        (cappingAutomaton_.getError() == ErrorCode::CapMissingVision);

    if (fillEnabled)
    {
        const bool canReleaseFromFill =
            !visionFaultBefore &&
            (!queueEnabled ||
             (interstageQueueCount_ < ProcessParams::MAX_INTERSTAGE_QUEUE));
        fillingAutomaton_.setReleasePermitted(canReleaseFromFill);
        fillingAutomaton_.update();
    }

    if (capEnabled)
    {
        cappingAutomaton_.update();
    }

    if (fillEnabled &&
        fillingAutomaton_.getError() != ErrorCode::None)
    {
        enterErrorStop(fillingAutomaton_.getError());
        return;
    }

    if (capEnabled &&
        (cappingAutomaton_.getError() != ErrorCode::None) &&
        (cappingAutomaton_.getError() != ErrorCode::CapMissingVision))
    {
        enterErrorStop(cappingAutomaton_.getError());
        return;
    }

    const bool visionFaultActive =
        capEnabled &&
        (cappingAutomaton_.getError() == ErrorCode::CapMissingVision);

    // Coordinación de banda: se pausa por STOP o mientras algún autómata tenga botella retenida
    const bool fillHolding = fillEnabled && fillingAutomaton_.isHoldingBottle();
    const bool capHolding  = capEnabled && cappingAutomaton_.isHoldingBottle();
    const bool pauseBelt   = linePaused_ || fillHolding || capHolding;

    fillingAutomaton_.setBeltPaused(pauseBelt);
    cappingAutomaton_.setBeltPaused(pauseBelt);

    if (pauseBelt)
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

    if (fillEnabled &&
        fillingAutomaton_.consumeBottleReleasedEvent())
    {
        if (queueEnabled)
        {
            if (interstageQueueCount_ < 0xFFU)
            {
                interstageQueueCount_++;
            }
        }
        else if (!capEnabled)
        {
            bottleCount_++;
        }
    }

    if (capEnabled &&
        cappingAutomaton_.consumeBottleCompletedEvent())
    {
        bottleCount_++;
        if (queueEnabled && interstageQueueCount_ > 0U)
        {
            interstageQueueCount_--;
        }
    }

    if (visionFaultActive)
    {
        if (activeError_ != ErrorCode::CapMissingVision)
        {
            activeError_ = ErrorCode::CapMissingVision;
            StationHardwareFactory::setTowerLights(false, true);
        }
        buzzer_.setPattern(BuzzerPattern::Fault, nowMs);
    }
    else
    {
        if (activeError_ == ErrorCode::CapMissingVision)
        {
            activeError_ = ErrorCode::None;
            StationHardwareFactory::setTowerLights(!linePaused_, linePaused_);
        }

        if (buzzer_.getPattern() != BuzzerPattern::BootReady)
        {
            const BuzzerPattern runPattern =
                (ProcessParams::isErrorEnabled(ErrorCode::NetworkLinkLost) && networkWarningActive_)
                    ? BuzzerPattern::Warning
                    : BuzzerPattern::Silent;
            buzzer_.setPattern(runPattern, nowMs);
        }
    }
}

void StationController::updateTestMode()
{
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
}

void StationController::updateMotionProfiles()
{
    fillZ_.updateMotionProfile();
    capZ_.updateMotionProfile();
    conveyor_.update();
    if (chuckMotor_.isRunning())
    {
        chuckMotor_.sampleCurrentAdc();
    }
}

bool StationController::toggleTestFlag(uint16_t flag)
{
    testFlags_ ^= flag;
    return (testFlags_ & flag) != 0U;
}

void StationController::handleTouchCommand(TouchCommand cmd)
{
    if (cmd == TouchCommand::None)
    {
        return;
    }

    const bool isRuntimeConfigCmd =
        (cmd >= TouchCommand::ToggleFillStation) &&
        (cmd <= TouchCommand::ToggleErrScadaLink);

    if (estopAux_.isTriggered() &&
        (mode_ != StationMode::TestMode) &&
        (cmd != TouchCommand::ToggleTestMode) &&
        (cmd != TouchCommand::CycleBottleProfile) &&
        !isRuntimeConfigCmd)
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
            else if (mode_ == StationMode::Running && cappingAutomaton_.acknowledgeVisionAlarm())
            {
                activeError_ = ErrorCode::None;
                StationHardwareFactory::setTowerLights(!linePaused_, linePaused_);
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
                if (ProcessParams::isErrorEnabled(ErrorCode::EmergencyStopPressed) && estopAux_.isTriggered())
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

        case TouchCommand::ToggleFillStation:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::FILL_STATION;
            if (!ProcessParams::hasConfigFlag(ConfigFlags::FILL_STATION))
            {
                fillingAutomaton_.reset();
                fillZ_.stop();
                fillZ_.setDriverEnabled(false);
            }
            else if (mode_ == StationMode::Running && !fillZ_.isHome())
            {
                enterBootMode();
            }
            break;

        case TouchCommand::ToggleCapStation:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::CAP_STATION;
            if (!ProcessParams::hasConfigFlag(ConfigFlags::CAP_STATION))
            {
                cappingAutomaton_.reset();
                capZ_.stop();
                capZ_.setDriverEnabled(false);
                if (activeError_ == ErrorCode::CapMissingVision)
                {
                    activeError_ = ErrorCode::None;
                    StationHardwareFactory::setTowerLights(!linePaused_, linePaused_);
                }
            }
            else if (mode_ == StationMode::Running && !capZ_.isHome())
            {
                enterBootMode();
            }
            break;

        case TouchCommand::ToggleVisionInspect:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::VISION_INSPECT;
            if (!ProcessParams::hasConfigFlag(ConfigFlags::VISION_INSPECT) &&
                (cappingAutomaton_.getError() == ErrorCode::CapMissingVision))
            {
                cappingAutomaton_.acknowledgeVisionAlarm();
                activeError_ = ErrorCode::None;
                StationHardwareFactory::setTowerLights(!linePaused_, linePaused_);
                buzzer_.silence();
            }
            break;

        case TouchCommand::ToggleVisionAdvance:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::VISION_ADVANCE;
            break;

        case TouchCommand::ToggleInterstageQueue:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::INTERSTAGE_QUEUE;
            if (!ProcessParams::hasConfigFlag(ConfigFlags::INTERSTAGE_QUEUE))
            {
                interstageQueueCount_ = 0U;
            }
            break;

        case TouchCommand::ToggleBuzzerConfig:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::BUZZER_ENABLED;
            if (!ProcessParams::hasConfigFlag(ConfigFlags::BUZZER_ENABLED))
            {
                testFlags_ &= static_cast<uint16_t>(~TestFlags::BUZZER_ON);
                buzzer_.silence();
            }
            break;

        case TouchCommand::ToggleBeltIrComp:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::BELT_IR_COMP;
            conveyor_.setIrCompEnabled(ProcessParams::hasConfigFlag(ConfigFlags::BELT_IR_COMP));
            break;

        case TouchCommand::ToggleBeltStallProt:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::BELT_STALL_PROT;
            conveyor_.setStallProtEnabled(ProcessParams::hasConfigFlag(ConfigFlags::BELT_STALL_PROT));
            break;

        case TouchCommand::ToggleErrBottleLost:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::ERR_BOTTLE_LOST;
            break;

        case TouchCommand::ToggleErrZHomeLimit:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::ERR_Z_HOME_LIMIT;
            break;

        case TouchCommand::ToggleErrZTimeout:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::ERR_Z_TIMEOUT;
            break;

        case TouchCommand::ToggleErrChuckTorque:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::ERR_CHUCK_TORQUE;
            break;

        case TouchCommand::ToggleErrEstopHw:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::ERR_ESTOP_HW;
            break;

        case TouchCommand::ToggleErrScadaLink:
            ProcessParams::runtimeConfigFlags() ^= ConfigFlags::ERR_SCADA_LINK;
            break;

        default:
            if (mode_ == StationMode::TestMode)
            {
                handleTestModeCommand(cmd);
            }
            break;
    }

    refreshDisplaySnapshot();
}

void StationController::handleTestModeCommand(TouchCommand cmd)
{
    switch (cmd)
    {
        case TouchCommand::TestToggleBelt:
            if (toggleTestFlag(TestFlags::BELT_ON))
            {
                conveyor_.clearStall();
                conveyor_.start();
            }
            else
            {
                conveyor_.stop();
            }
            break;

        case TouchCommand::TestToggleTowerGreen:
            StationHardwareFactory::writeTowerRelayPin(
                HardwarePins::PIN_TOWER_GREEN,
                toggleTestFlag(TestFlags::TOWER_GREEN_ON));
            break;

        case TouchCommand::TestToggleTowerRed:
            StationHardwareFactory::writeTowerRelayPin(
                HardwarePins::PIN_TOWER_RED,
                toggleTestFlag(TestFlags::TOWER_RED_ON));
            break;

        case TouchCommand::TestToggleBuzzer:
            if (!ProcessParams::hasConfigFlag(ConfigFlags::BUZZER_ENABLED))
            {
                testFlags_ &= static_cast<uint16_t>(~TestFlags::BUZZER_ON);
                buzzer_.silence();
                break;
            }
            buzzer_.setManualOverride(toggleTestFlag(TestFlags::BUZZER_ON));
            break;

        case TouchCommand::TestToggleFillStopArm:
            if (toggleTestFlag(TestFlags::FILL_STOP_DN)) { fillGate_.lowerStopArm(); }
            else                                         { fillGate_.raiseStopArm(); }
            break;

        case TouchCommand::TestToggleFillClampArm:
            if (toggleTestFlag(TestFlags::FILL_CLAMP_DN)) { fillGate_.lowerClampArm(); }
            else                                          { fillGate_.raiseClampArm(); }
            break;

        case TouchCommand::TestToggleFillPump:
            if (toggleTestFlag(TestFlags::FILL_PUMP_ON)) { fillPump_.on(); }
            else                                         { fillPump_.off(); }
            break;

        case TouchCommand::TestJogFillZDown:
            fillZ_.jogDown(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            break;

        case TouchCommand::TestToggleCapStopArm:
            if (toggleTestFlag(TestFlags::CAP_STOP_DN)) { capGate_.lowerStopArm(); }
            else                                        { capGate_.raiseStopArm(); }
            break;

        case TouchCommand::TestToggleCapClampArm:
            if (toggleTestFlag(TestFlags::CAP_CLAMP_DN)) { capGate_.lowerClampArm(); }
            else                                         { capGate_.raiseClampArm(); }
            break;

        case TouchCommand::TestToggleChuckMotor:
            if (toggleTestFlag(TestFlags::CAP_CHUCK_ON))
            {
                chuckMotor_.driveForward(ProcessParams::CHUCK_PWM);
            }
            else
            {
                chuckMotor_.stop();
            }
            break;

        case TouchCommand::TestJogCapZDown:
            capZ_.jogDown(ProcessParams::Z_JOG_STEPS, ProcessParams::Z_HOMING_SPEED);
            break;

        default:
            break;
    }
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

    StationHardwareFactory::setTowerLights(false, false);
    buzzer_.silence();

    fillPump_.off();
    chuckMotor_.stop();
    conveyor_.stop();
    conveyor_.clearStall();

    fillingAutomaton_.reset();
    cappingAutomaton_.reset();

    fillZ_.clearSoftLimitFault();
    capZ_.clearSoftLimitFault();
    if (ProcessParams::hasConfigFlag(ConfigFlags::FILL_STATION))
    {
        fillZ_.home(ProcessParams::Z_HOMING_SPEED);
    }
    else
    {
        fillZ_.stop();
        fillZ_.setDriverEnabled(false);
    }

    if (ProcessParams::hasConfigFlag(ConfigFlags::CAP_STATION))
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

    StationHardwareFactory::setTowerLights(true, false);
    buzzer_.triggerBootReady(millis());

    refreshDisplaySnapshot();
}

void StationController::enterErrorStop(ErrorCode error)
{
    if (!ProcessParams::isErrorEnabled(error))
    {
        return;
    }

    mode_ = StationMode::ErrorStop;
    testFlags_ = 0U;

    if (activeError_ == ErrorCode::None || error == ErrorCode::EmergencyStopPressed)
    {
        activeError_ = error;
    }

    StationHardwareFactory::setTowerLights(false, true);

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

    StationHardwareFactory::setTowerLights(false, false);
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
    uint16_t effectiveFlags = testFlags_ & static_cast<uint16_t>(~(TestFlags::FILL_HOME_SW | TestFlags::CAP_HOME_SW));
    if (limitSwitchFill_.isTriggered()) { effectiveFlags |= TestFlags::FILL_HOME_SW; }
    if (limitSwitchCap_.isTriggered())  { effectiveFlags |= TestFlags::CAP_HOME_SW; }

    uint16_t inFlags = 0U;
    if (presenceFill_.isTriggered())    { inFlags |= InputFlags::PRESENCE_FILL; }
    if (positionFill_.isTriggered())    { inFlags |= InputFlags::POS_FILL; }
    if (presenceCap_.isTriggered())     { inFlags |= InputFlags::PRESENCE_CAP; }
    if (positionCap_.isTriggered())     { inFlags |= InputFlags::POS_CAP; }
    if (limitSwitchFill_.isTriggered()) { inFlags |= InputFlags::LIMIT_FILL; }
    if (limitSwitchCap_.isTriggered())  { inFlags |= InputFlags::LIMIT_CAP; }
    if (btnStart_.isTriggered())        { inFlags |= InputFlags::BTN_START; }
    if (btnStop_.isTriggered())         { inFlags |= InputFlags::BTN_STOP; }
    if (btnReset_.isTriggered())        { inFlags |= InputFlags::BTN_RESET; }
    if (estopAux_.isTriggered())        { inFlags |= InputFlags::ESTOP_ACTIVE; }
    if (visionSensor_.isTriggered())    { inFlags |= InputFlags::VISION_ALARM; }

    const DisplaySnapshot current{
        computeEffectiveError(),
        mode_,
        fillingAutomaton_.getState(),
        cappingAutomaton_.getState(),
        profile_,
        interstageQueueCount_,
        bottleCount_,
        effectiveFlags,
        fillZ_.getCurrentSteps(),
        capZ_.getCurrentSteps(),
        conveyor_.getCurrentPwm(),
        conveyor_.getFilteredCurrentAdc(),
        chuckMotor_.getCurrentPwm(),
        chuckMotor_.getFilteredCurrentAdc(),
        inFlags,
        ProcessParams::runtimeConfigFlags()};

    if (current != lastSnapshot_)
    {
        lastSnapshot_  = current;
        snapshotDirty_ = true;
    }
}

ErrorCode StationController::computeEffectiveError() const
{
    if (activeError_ != ErrorCode::None)
    {
        return activeError_;
    }

    if (ProcessParams::isErrorEnabled(ErrorCode::NetworkLinkLost) && networkWarningActive_)
    {
        return ErrorCode::NetworkLinkLost;
    }

    return ErrorCode::None;
}
