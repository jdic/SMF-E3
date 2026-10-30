#include <Arduino.h>
#include <Servo.h>
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"
#include "types/station_types.hpp"
#include "digital_sensor.hpp"
#include "fill_pump.hpp"
#include "bottle_gate.hpp"
#include "dc_motor_bts7960.hpp"
#include "conveyor_controller.hpp"
#include "stepper_axis.hpp"
#include "alarm_buzzer.hpp"
#include "display_manager.hpp"

namespace
{
    constexpr uint16_t COLOR_BLACK  = 0x0000;
    constexpr uint16_t COLOR_WHITE  = 0xFFFF;
    constexpr uint16_t COLOR_GREEN  = 0x07E0;
    constexpr uint16_t COLOR_RED    = 0xF800;
    constexpr uint16_t COLOR_YELLOW = 0xFFE0;
    constexpr uint16_t COLOR_CYAN   = 0x07FF;
    constexpr uint16_t COLOR_BLUE   = 0x001F;
    constexpr uint16_t COLOR_ORANGE = 0xFC00;
    constexpr uint16_t COLOR_DARK   = 0x2104;

    constexpr uint16_t SENSOR_POLL_PERIOD_MS = 1;
    constexpr uint16_t MOTION_PERIOD_MS      = 20;
    constexpr uint16_t UI_REFRESH_PERIOD_MS  = 50;
    constexpr uint16_t ADC_DISPLAY_PERIOD_MS = 250;
    constexpr uint16_t BENCH_JOG_STEPS       = 1600;
    constexpr uint16_t BENCH_JOG_SPEED       = 2000;

    ConveyorConfig makeBenchConveyorConfig()
    {
        ConveyorConfig cfg{};
        cfg.rpwmPin         = HardwarePins::PIN_BTS_CONV_RPWM;
        cfg.enPin           = HardwarePins::PIN_BTS_CONV_EN;
        cfg.currentSensePin = HardwarePins::PIN_ADC_CONV_CURRENT;
        cfg.targetPwm       = ProcessParams::BELT_PWM;
        cfg.accelStepPwm    = ProcessParams::BELT_ACCEL_PWM_STEP;
        cfg.decelStepPwm    = ProcessParams::BELT_DECEL_PWM_STEP;
        cfg.currentLimitAdc = 1023; // Margen alto para prueba manual en banco
        cfg.stallTimeMs     = ProcessParams::BELT_STALL_MS;
        return cfg;
    }

    StepperAxisConfig makeBenchFillZConfig()
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

    // =========================================================================
    // SENSORES DE LA ESTACIÓN DE LLENADO (no requiere botón de reset ni tapado)
    // - Pin 22: 1er sensor IR (Aproximación antes de los brazos, activo en 5 V)
    // - Pin 26: 2do sensor IR (Posición exacta entre los dos brazos, activo en 5 V)
    // - Pin 24: Fin de carrera superior eje Z1 Llenado (COM a GND, INPUT_PULLUP)
    // =========================================================================
    DigitalSensor sensApproachFill(
        HardwarePins::PIN_PRESENCE_FILL,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        true,
        false);

    DigitalSensor sensPositionFill(
        HardwarePins::PIN_POS_FILL,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        true,
        false);

    DigitalSensor sensLimitFill(
        HardwarePins::PIN_LIMIT_SWITCH_FILL,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        false,
        true);

    // =========================================================================
    // ACTUADORES EN BANCO:
    // - 1 módulo IBT-2 compartido:
    //   * Canal R (M+ a GND, RPWM=44, R_EN=41, R_IS=A9): Banda a 7V con rampa trapezoidal
    //   * Canal L (M- a GND, LPWM=45, L_EN=40, L_IS=A8): Motor DC de Tapado
    // - 2 Servomotores de brazos de Llenado:
    //   * Brazo 1 (Tope)     : Pin 29
    //   * Brazo 2 (Sujeción) : Pin 30
    // - Bomba de Llenado     : Pin 43
    // - Stepper Z1 Llenado   : STEP=37, DIR=36, EN=35
    // =========================================================================
    ConveyorController conveyor(makeBenchConveyorConfig());
    DcMotorBts7960     chuckMotor(
        HardwarePins::PIN_BTS_CHUCK_RPWM,
        HardwarePins::PIN_BTS_CHUCK_EN,
        DcMotorBts7960::UNUSED_PIN,
        HardwarePins::PIN_ADC_CHUCK_CURRENT);

    Servo       servoStopArm;
    Servo       servoClampArm;
    bool        stopArmDown  = false;
    bool        clampArmDown = false;
    bool        pumpRunning  = false;

    StepperAxis fillZ(makeBenchFillZConfig());

    // Pantalla TFT + Botones táctiles
    DisplayManager      display;
    Adafruit_GFX_Button btnToggleBelt;
    Adafruit_GFX_Button btnToggleChuck;

    // Botones en pantalla Home (Brazos 1 y 2 + Bomba)
    Adafruit_GFX_Button btnToggleStopArm;
    Adafruit_GFX_Button btnToggleClampArm;
    Adafruit_GFX_Button btnTogglePump;

    // Botones en pantalla Settings (Stepper Z1 Llenado)
    Adafruit_GFX_Button btnFillZUp;
    Adafruit_GFX_Button btnFillZDown;
    Adafruit_GFX_Button btnFillZHome;
    Adafruit_GFX_Button btnFillZStop;

    ScreenId lastRenderedScreen = ScreenId::None;
    bool     lastBeltRunning    = false;
    bool     lastChuckRunning   = false;
    bool     lastStopArmDown    = false;
    bool     lastClampArmDown   = false;
    bool     lastPumpRunning    = false;
    uint8_t  lastSensorMask     = 0xFFU;

    int32_t  lastFillZSteps     = 0x7FFFFFFFL;
    bool     lastFillZBusy      = false;
    uint16_t lastBeltAdc        = 0xFFFFU;
    uint16_t lastChuckAdc       = 0xFFFFU;

    uint32_t lastSensorPollMs   = 0;
    uint32_t lastMotionMs       = 0;
    uint32_t lastUiRefreshMs    = 0;
    uint32_t lastAdcDisplayMs   = 0;

    uint8_t packSensorStates()
    {
        uint8_t mask = 0;
        if (sensApproachFill.isTriggered()) { mask |= (1U << 0); }
        if (sensPositionFill.isTriggered()) { mask |= (1U << 1); }
        if (sensLimitFill.isTriggered())    { mask |= (1U << 2); }
        return mask;
    }

    void drawBottomDcButtons(bool beltRunning, bool chuckRunning)
    {
        MCUFRIEND_kbv& tft = display.getTft();

        // Botón [BANDA M+ 7V] en la barra inferior (X = 126..218, Y = 190..232)
        const uint16_t beltCol = beltRunning ? COLOR_RED : COLOR_GREEN;
        tft.fillRoundRect(126, 190, 92, 42, 5, beltCol);
        tft.drawRoundRect(126, 190, 92, 42, 5, COLOR_WHITE);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_BLACK, beltCol);
        tft.setCursor(134, 198);
        tft.print(beltRunning ? F("PARAR 7V") : F("MOVER 7V"));
        tft.setCursor(134, 212);
        tft.print(F("BANDA M+"));

        // Botón [TAPADO M-] en la barra inferior (X = 222..314, Y = 190..232)
        const uint16_t chuckCol = chuckRunning ? COLOR_RED : COLOR_CYAN;
        tft.fillRoundRect(222, 190, 92, 42, 5, chuckCol);
        tft.drawRoundRect(222, 190, 92, 42, 5, COLOR_WHITE);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_BLACK, chuckCol);
        tft.setCursor(232, 198);
        tft.print(chuckRunning ? F("PARAR") : F("GIRAR"));
        tft.setCursor(228, 212);
        tft.print(F("TAPADO M-"));
    }

    void drawSensorRow(uint8_t rowIdx, const __FlashStringHelper* label, bool active)
    {
        MCUFRIEND_kbv& tft = display.getTft();
        const int16_t y = static_cast<int16_t>(26 + (rowIdx * 18));

        tft.setTextSize(1);
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
        tft.setCursor(8, y);
        tft.print(label);

        tft.setCursor(206, y);
        if (active)
        {
            tft.setTextColor(COLOR_GREEN, COLOR_BLACK);
            tft.print(F("[ ACTIVO ]"));
        }
        else
        {
            tft.setTextColor(COLOR_YELLOW, COLOR_BLACK);
            tft.print(F("[ LIBRE  ]"));
        }
    }

    void drawArmButtons(bool stopDown, bool clampDown, bool pumpOn)
    {
        MCUFRIEND_kbv& tft = display.getTft();

        // Botón Brazo 1: Tope (P29) -> X = 8..154, Y = 88..130
        const uint16_t stopCol = stopDown ? COLOR_ORANGE : COLOR_BLUE;
        tft.fillRoundRect(8, 88, 148, 42, 5, stopCol);
        tft.drawRoundRect(8, 88, 148, 42, 5, COLOR_WHITE);
        tft.setTextSize(1);
        tft.setTextColor(stopDown ? COLOR_BLACK : COLOR_WHITE, stopCol);
        tft.setCursor(16, 96);
        tft.print(F("BRAZO 1 TOPE (P29)"));
        tft.setCursor(16, 112);
        tft.print(stopDown ? F("ESTADO: ABAJO (100)") : F("ESTADO: ARRIBA (10)"));

        // Botón Brazo 2: Sujeción (P30) -> X = 164..312, Y = 88..130
        const uint16_t clampCol = clampDown ? COLOR_ORANGE : COLOR_BLUE;
        tft.fillRoundRect(164, 88, 148, 42, 5, clampCol);
        tft.drawRoundRect(164, 88, 148, 42, 5, COLOR_WHITE);
        tft.setTextSize(1);
        tft.setTextColor(clampDown ? COLOR_BLACK : COLOR_WHITE, clampCol);
        tft.setCursor(170, 96);
        tft.print(F("BRAZO 2 SUJEC (P30)"));
        tft.setCursor(170, 112);
        tft.print(clampDown ? F("ESTADO: ABAJO (95) ") : F("ESTADO: ARRIBA (10)"));

        // Botón Bomba Llenado (P43) -> X = 8..312, Y = 136..176
        const uint16_t pumpCol = pumpOn ? COLOR_RED : COLOR_DARK;
        tft.fillRoundRect(8, 136, 304, 40, 5, pumpCol);
        tft.drawRoundRect(8, 136, 304, 40, 5, COLOR_WHITE);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE, pumpCol);
        tft.setCursor(36, 148);
        tft.print(pumpOn ? F("BOMBA P43: ENCENDIDA") : F("BOMBA P43: APAGADA  "));
    }

    void drawHomeBenchStatic()
    {
        MCUFRIEND_kbv& tft = display.getTft();
        tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, DisplayManager::CONTENT_HEIGHT, COLOR_BLACK);

        tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, 20, COLOR_DARK);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_CYAN, COLOR_DARK);
        tft.setCursor(6, 2);
        tft.print(F("TEST LLENADO: SENS+BRAZOS"));

        tft.drawFastHLine(0, 82, DisplayManager::SCREEN_WIDTH, COLOR_WHITE);
    }

    void updateHomeBenchDynamic(bool forceRedraw)
    {
        const bool beltOn  = conveyor.isRunning();
        const bool chuckOn = chuckMotor.isRunning();
        const uint8_t sensorMask = packSensorStates();

        if (forceRedraw)
        {
            drawHomeBenchStatic();
            drawBottomDcButtons(beltOn, chuckOn);
            drawArmButtons(stopArmDown, clampArmDown, pumpRunning);
            lastBeltRunning  = beltOn;
            lastChuckRunning = chuckOn;
            lastStopArmDown  = stopArmDown;
            lastClampArmDown = clampArmDown;
            lastPumpRunning  = pumpRunning;
        }

        if ((beltOn != lastBeltRunning) || (chuckOn != lastChuckRunning))
        {
            drawBottomDcButtons(beltOn, chuckOn);
            lastBeltRunning  = beltOn;
            lastChuckRunning = chuckOn;
        }

        if ((stopArmDown != lastStopArmDown) ||
            (clampArmDown != lastClampArmDown) ||
            (pumpRunning != lastPumpRunning))
        {
            drawArmButtons(stopArmDown, clampArmDown, pumpRunning);
            lastStopArmDown  = stopArmDown;
            lastClampArmDown = clampArmDown;
            lastPumpRunning  = pumpRunning;
        }

        const uint8_t diff = forceRedraw ? 0x07U : static_cast<uint8_t>(sensorMask ^ lastSensorMask);

        if ((diff & (1U << 0)) != 0U)
        {
            drawSensorRow(0, F("P22 IR1 Aproximacion (5V):"), (sensorMask & (1U << 0)) != 0U);
        }
        if ((diff & (1U << 1)) != 0U)
        {
            drawSensorRow(1, F("P26 IR2 Posic. Exacta(5V):"), (sensorMask & (1U << 1)) != 0U);
        }
        if ((diff & (1U << 2)) != 0U)
        {
            drawSensorRow(2, F("P24 Fin Carrera Z1 (GND) :"), (sensorMask & (1U << 2)) != 0U);
        }

        lastSensorMask = sensorMask;
    }

    void drawStepperBoxButton(int16_t x, int16_t y, const __FlashStringHelper* line1, uint16_t color)
    {
        MCUFRIEND_kbv& tft = display.getTft();
        tft.fillRoundRect(x, y, 146, 38, 6, color);
        tft.drawRoundRect(x, y, 146, 38, 6, COLOR_WHITE);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE, color);
        tft.setCursor(x + 12, y + 11);
        tft.print(line1);
    }

    void drawSettingsSteppersStatic()
    {
        MCUFRIEND_kbv& tft = display.getTft();
        tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, DisplayManager::CONTENT_HEIGHT, COLOR_BLACK);

        tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, 20, COLOR_DARK);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_YELLOW, COLOR_DARK);
        tft.setCursor(6, 2);
        tft.print(F("EJE Z1 LLENADO + CORRIENTE"));

        // 4 Botones para Eje Z1 Llenado (P37/36/35)
        drawStepperBoxButton(8,   84, F("Z1 SUBIR"), COLOR_BLUE);
        drawStepperBoxButton(8,  132, F("Z1 BAJAR"), COLOR_DARK);
        drawStepperBoxButton(166, 84, F("Z1 HOME "), COLOR_GREEN);
        drawStepperBoxButton(166, 132, F("Z1 PARAR"), COLOR_RED);
    }

    void updateSettingsSteppersDynamic(bool forceRedraw)
    {
        MCUFRIEND_kbv& tft = display.getTft();
        const bool beltOn  = conveyor.isRunning();
        const bool chuckOn = chuckMotor.isRunning();

        if (forceRedraw)
        {
            drawSettingsSteppersStatic();
            drawBottomDcButtons(beltOn, chuckOn);
            lastBeltRunning  = beltOn;
            lastChuckRunning = chuckOn;
        }
        else if ((beltOn != lastBeltRunning) || (chuckOn != lastChuckRunning))
        {
            drawBottomDcButtons(beltOn, chuckOn);
            lastBeltRunning  = beltOn;
            lastChuckRunning = chuckOn;
        }

        const int32_t fSteps = fillZ.getCurrentSteps();
        const bool    fBusy  = fillZ.isBusy();
        if (forceRedraw || (fSteps != lastFillZSteps) || (fBusy != lastFillZBusy))
        {
            tft.setTextSize(1);
            tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
            tft.setCursor(8, 28);
            tft.print(F("Z1 Llenado (P37/36/35) Pasos: "));
            tft.print(fSteps);
            tft.print(F("      "));

            tft.setCursor(8, 42);
            tft.setTextColor(fBusy ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK);
            tft.print(fBusy ? F("Estado Z1: MOVIENDO   ") : F("Estado Z1: EN REPOSO  "));

            lastFillZSteps = fSteps;
            lastFillZBusy  = fBusy;
        }

        const uint32_t nowMs = millis();
        if (forceRedraw || (nowMs - lastAdcDisplayMs >= ADC_DISPLAY_PERIOD_MS))
        {
            lastAdcDisplayMs = nowMs;
            const uint16_t bAdc = conveyor.getFilteredCurrentAdc();
            const uint16_t cAdc = chuckMotor.getFilteredCurrentAdc();

            if (forceRedraw || (bAdc != lastBeltAdc) || (cAdc != lastChuckAdc))
            {
                tft.setTextSize(1);
                tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
                tft.setCursor(8, 60);
                tft.print(F("R_IS Banda(A9): "));
                tft.print(bAdc);
                tft.print(F("   L_IS Tapa(A8): "));
                tft.print(cAdc);
                tft.print(F("   "));

                lastBeltAdc  = bAdc;
                lastChuckAdc = cAdc;
            }
        }
    }

    void toggleConveyorBelt()
    {
        if (conveyor.isRunning())
        {
            conveyor.pauseSmooth();
            digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);
            Serial.println(F("[TEST] Motor Banda DESACELERANDO (Rampa trapezoidal hacia 0V en Borne M+)"));
        }
        else
        {
            conveyor.clearStall();
            conveyor.start();
            digitalWrite(HardwarePins::PIN_STATUS_LED, HIGH);
            Serial.print(F("[TEST] Motor Banda ACELERANDO (Rampa trapezoidal hacia PWM="));
            Serial.print(ProcessParams::BELT_PWM);
            Serial.println(F(" [~7.0V efectivos], Pin 41 R_EN=HIGH, Borne M+ activo)"));
        }
    }

    void toggleChuckMotor()
    {
        if (chuckMotor.isRunning())
        {
            chuckMotor.stop();
            Serial.println(F("[TEST] Motor Tapado DETENIDO (Pin 45 LPWM=0, Pin 40 L_EN=LOW, Borne M-=0V)"));
        }
        else
        {
            chuckMotor.driveForward(ProcessParams::CHUCK_PWM);
            Serial.print(F("[TEST] Motor Tapado ENCENDIDO (Pin 45 LPWM="));
            Serial.print(ProcessParams::CHUCK_PWM);
            Serial.println(F(", Pin 40 L_EN=HIGH, Borne M- activo)"));
        }
    }

    void toggleStopArm()
    {
        stopArmDown = !stopArmDown;
        const uint8_t targetDeg = stopArmDown
                                      ? ProcessParams::FILL_STOP_DOWN_DEG
                                      : ProcessParams::FILL_STOP_UP_DEG;
        servoStopArm.write(targetDeg);
        Serial.print(F("[SERVO] Brazo 1 Tope (Pin 29) -> "));
        Serial.print(stopArmDown ? F("ABAJO (") : F("ARRIBA ("));
        Serial.print(targetDeg);
        Serial.println(F(" grados)"));
    }

    void toggleClampArm()
    {
        clampArmDown = !clampArmDown;
        const uint8_t targetDeg = clampArmDown
                                      ? ProcessParams::FILL_CLAMP_DOWN_DEG
                                      : ProcessParams::FILL_CLAMP_UP_DEG;
        servoClampArm.write(targetDeg);
        Serial.print(F("[SERVO] Brazo 2 Sujecion (Pin 30) -> "));
        Serial.print(clampArmDown ? F("ABAJO (") : F("ARRIBA ("));
        Serial.print(targetDeg);
        Serial.println(F(" grados)"));
    }

    void togglePump()
    {
        pumpRunning = !pumpRunning;
        digitalWrite(HardwarePins::PIN_PUMP_FILL, pumpRunning ? HIGH : LOW);
        Serial.print(F("[BOMBA] Pin 43 -> "));
        Serial.println(pumpRunning ? F("ENCENDIDA") : F("APAGADA"));
    }

    void logSensorChangesToSerial()
    {
        if (sensApproachFill.hasChanged())
        {
            Serial.print(F("[SENSOR] P22 IR1 Aproximacion Llenado -> "));
            Serial.println(sensApproachFill.isTriggered() ? F("ACTIVO (5V)") : F("LIBRE (0V)"));
        }
        if (sensPositionFill.hasChanged())
        {
            Serial.print(F("[SENSOR] P26 IR2 Posicion Exacta Llenado -> "));
            Serial.println(sensPositionFill.isTriggered() ? F("ACTIVO (5V)") : F("LIBRE (0V)"));
        }
        if (sensLimitFill.hasChanged())
        {
            Serial.print(F("[SENSOR] P24 Fin Carrera Z1 -> "));
            Serial.println(sensLimitFill.isTriggered() ? F("ACTIVO (GND)") : F("LIBRE"));
        }
    }

    void handleSerialCommands()
    {
        if (Serial.available() <= 0)
        {
            return;
        }

        const char c = static_cast<char>(Serial.read());
        switch (c)
        {
            case 'b':
            case 'B':
                toggleConveyorBelt();
                break;

            case 'c':
            case 'C':
                toggleChuckMotor();
                break;

            case 't':
            case 'T':
                toggleStopArm();
                break;

            case 'g':
            case 'G':
                toggleClampArm();
                break;

            case 'p':
            case 'P':
                togglePump();
                break;

            case '1':
                fillZ.jogUp(BENCH_JOG_STEPS, BENCH_JOG_SPEED);
                Serial.println(F("[STEPPER] Z1 Llenado SUBIR 1600 pasos"));
                break;

            case '2':
                fillZ.jogDown(BENCH_JOG_STEPS, BENCH_JOG_SPEED);
                Serial.println(F("[STEPPER] Z1 Llenado BAJAR 1600 pasos"));
                break;

            case 'h':
            case 'H':
                fillZ.home(ProcessParams::Z_HOMING_SPEED);
                Serial.println(F("[STEPPER] Z1 Llenado HOMING hacia P24"));
                break;

            case 's':
            case 'S':
                conveyor.stop();
                chuckMotor.stop();
                fillZ.stop();
                pumpRunning = false;
                digitalWrite(HardwarePins::PIN_PUMP_FILL, LOW);
                Serial.println(F("[TEST] PARO TOTAL de motores y bomba"));
                break;

            default:
                break;
        }
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(HardwarePins::PIN_STATUS_LED, OUTPUT);
    digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);

    pinMode(HardwarePins::PIN_PUMP_FILL, OUTPUT);
    digitalWrite(HardwarePins::PIN_PUMP_FILL, LOW);

    sensApproachFill.init();
    sensPositionFill.init();
    sensLimitFill.init();

    conveyor.init();
    chuckMotor.init();
    fillZ.init();

    // Inicializar los 2 servomotores de los brazos de Llenado en posición ARRIBA
    servoStopArm.attach(HardwarePins::PIN_SERVO_FILL_STOP);
    servoClampArm.attach(HardwarePins::PIN_SERVO_FILL_CLAMP);
    servoStopArm.write(ProcessParams::FILL_STOP_UP_DEG);
    servoClampArm.write(ProcessParams::FILL_CLAMP_UP_DEG);

    display.init();
    char emptyLabel[] = "";
    MCUFRIEND_kbv& tft = display.getTft();

    // Botones de la barra inferior (activos en Home y Settings)
    btnToggleBelt.initButton(&tft,  172, 211, 92, 42, 0, 0, 0, emptyLabel, 1);
    btnToggleChuck.initButton(&tft, 268, 211, 92, 42, 0, 0, 0, emptyLabel, 1);

    // Botones en Home: Brazo 1 (Tope P29), Brazo 2 (Sujeción P30) y Bomba (P43)
    btnToggleStopArm.initButton(&tft,   82, 109, 148, 42, 0, 0, 0, emptyLabel, 1);
    btnToggleClampArm.initButton(&tft, 238, 109, 148, 42, 0, 0, 0, emptyLabel, 1);
    btnTogglePump.initButton(&tft,     160, 156, 304, 40, 0, 0, 0, emptyLabel, 1);

    // Botones en Settings: Stepper Z1 Llenado
    btnFillZUp.initButton(&tft,    81, 103, 146, 38, 0, 0, 0, emptyLabel, 1);
    btnFillZDown.initButton(&tft,  81, 151, 146, 38, 0, 0, 0, emptyLabel, 1);
    btnFillZHome.initButton(&tft, 239, 103, 146, 38, 0, 0, 0, emptyLabel, 1);
    btnFillZStop.initButton(&tft, 239, 151, 146, 38, 0, 0, 0, emptyLabel, 1);

    display.showScreen(ScreenId::Home);
    lastRenderedScreen = ScreenId::Home;
    updateHomeBenchDynamic(true);

    Serial.println(F("=========================================================="));
    Serial.println(F(" TEST ESTACION DE LLENADO + BANDA 7V + BRAZOS + SENSORES"));
    Serial.println(F(" - Banda 7V (M+)      : RPWM=44, R_EN=41, R_IS=A9 (Tecla 'b')"));
    Serial.println(F(" - Motor Tapa (M-)    : LPWM=45, L_EN=40, L_IS=A8 (Tecla 'c')"));
    Serial.println(F(" - Brazo 1 Tope       : Pin 29 (Tecla 't' o boton en TFT)"));
    Serial.println(F(" - Brazo 2 Sujecion   : Pin 30 (Tecla 'g' o boton en TFT)"));
    Serial.println(F(" - Bomba Llenado      : Pin 43 (Tecla 'p' o boton en TFT)"));
    Serial.println(F(" - Stepper Z1 Llenado : STEP=37, DIR=36, EN=35 (Teclas '1'/'2'/'h')"));
    Serial.println(F(" - Sensores Llenado   : P22 (IR1 Aprox), P26 (IR2 Posic), P24 (FC Z1)"));
    Serial.println(F("=========================================================="));
}

void loop()
{
    const uint32_t nowMs = millis();

    // 1. Muestreo de los 3 sensores de Llenado (cada 1 ms)
    if (nowMs - lastSensorPollMs >= SENSOR_POLL_PERIOD_MS)
    {
        lastSensorPollMs = nowMs;

        sensApproachFill.update();
        sensPositionFill.update();
        sensLimitFill.update();

        logSensorChangesToSerial();
    }

    // 2. Comandos por Monitor Serie
    handleSerialCommands();

    // 3. Perfiles de movimiento (Rampa trapezoidal de banda 7V y Stepper Z1) cada 20 ms
    if (nowMs - lastMotionMs >= MOTION_PERIOD_MS)
    {
        lastMotionMs = nowMs;
        conveyor.update();
        (void)chuckMotor.sampleCurrentAdc();
        fillZ.updateMotionProfile();
    }

    // 4. Interfaz táctil TFT y actualización de pantallas (cada 50 ms)
    if (nowMs - lastUiRefreshMs >= UI_REFRESH_PERIOD_MS)
    {
        lastUiRefreshMs = nowMs;

        int16_t px = 0;
        int16_t py = 0;
        const bool touched = display.updateTouchNavigation(px, py);
        const ScreenId currentScreen = display.getCurrentScreen();

        const bool screenChanged = (currentScreen != lastRenderedScreen);
        lastRenderedScreen = currentScreen;

        // Botones inferiores para Banda (M+) y Motor Tapado (M-)
        btnToggleBelt.press(touched && btnToggleBelt.contains(px, py));
        btnToggleChuck.press(touched && btnToggleChuck.contains(px, py));

        if (btnToggleBelt.justReleased())
        {
            toggleConveyorBelt();
        }
        if (btnToggleChuck.justReleased())
        {
            toggleChuckMotor();
        }

        if (currentScreen == ScreenId::Home)
        {
            btnToggleStopArm.press(touched && btnToggleStopArm.contains(px, py));
            btnToggleClampArm.press(touched && btnToggleClampArm.contains(px, py));
            btnTogglePump.press(touched && btnTogglePump.contains(px, py));

            if (btnToggleStopArm.justReleased())
            {
                toggleStopArm();
            }
            if (btnToggleClampArm.justReleased())
            {
                toggleClampArm();
            }
            if (btnTogglePump.justReleased())
            {
                togglePump();
            }

            updateHomeBenchDynamic(screenChanged);
        }
        else if (currentScreen == ScreenId::Settings)
        {
            btnFillZUp.press(touched && btnFillZUp.contains(px, py));
            btnFillZDown.press(touched && btnFillZDown.contains(px, py));
            btnFillZHome.press(touched && btnFillZHome.contains(px, py));
            btnFillZStop.press(touched && btnFillZStop.contains(px, py));

            if (btnFillZUp.justReleased())
            {
                if (fillZ.isBusy()) { fillZ.stop(); }
                else { fillZ.jogUp(BENCH_JOG_STEPS, BENCH_JOG_SPEED); }
                Serial.println(F("[TFT] Boton Z1 SUBIR"));
            }
            if (btnFillZDown.justReleased())
            {
                if (fillZ.isBusy()) { fillZ.stop(); }
                else { fillZ.jogDown(BENCH_JOG_STEPS, BENCH_JOG_SPEED); }
                Serial.println(F("[TFT] Boton Z1 BAJAR"));
            }
            if (btnFillZHome.justReleased())
            {
                fillZ.home(ProcessParams::Z_HOMING_SPEED);
                Serial.println(F("[TFT] Boton Z1 HOME (hacia Fin de Carrera P24)"));
            }
            if (btnFillZStop.justReleased())
            {
                fillZ.stop();
                Serial.println(F("[TFT] Boton Z1 PARAR"));
            }

            updateSettingsSteppersDynamic(screenChanged);
        }
    }
}
