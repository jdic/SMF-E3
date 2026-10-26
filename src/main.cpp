#include <Arduino.h>
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
#include "station/filling_automaton.hpp"
#include "station/capping_automaton.hpp"
#include "station/station_controller.hpp"

namespace
{
    constexpr uint16_t COLOR_BLACK  = 0x0000;
    constexpr uint16_t COLOR_WHITE  = 0xFFFF;
    constexpr uint16_t COLOR_GREEN  = 0x07E0;
    constexpr uint16_t COLOR_RED    = 0xF800;
    constexpr uint16_t COLOR_YELLOW = 0xFFE0;
    constexpr uint16_t COLOR_CYAN   = 0x07FF;
    constexpr uint16_t COLOR_BLUE   = 0x001F;
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
        cfg.currentLimitAdc = 1023; // Margen alto para prueba en banco
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

    StepperAxisConfig makeBenchCapZConfig()
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

    // 6 Sensores Digitales (pines 22, 23, 24, 25, 27, 28)
    DigitalSensor sensPresenceFill(
        HardwarePins::PIN_PRESENCE_FILL,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        false,
        true);

    DigitalSensor sensPresenceCap(
        HardwarePins::PIN_PRESENCE_CAP,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        false,
        true);

    DigitalSensor sensLimitFill(
        HardwarePins::PIN_LIMIT_SWITCH_FILL,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        false,
        true);

    DigitalSensor sensLimitCap(
        HardwarePins::PIN_LIMIT_SWITCH_CAP,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        false,
        true);

    DigitalSensor sensEstopAux(
        HardwarePins::PIN_ESTOP_AUX,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        true,
        true);

    DigitalSensor sensBtnReset(
        HardwarePins::PIN_BTN_RESET,
        ProcessParams::SENSOR_DEBOUNCE_MS,
        false,
        true);

    // 2 Motores DC en el MISMO módulo BTS7960:
    // - Motor 1 Banda : RPWM=44, R_EN=41, R_IS=A9, Borne M+ hacia GND (B-)
    // - Motor 2 Tapado: LPWM=45, L_EN=40, L_IS=A8, Borne M- hacia GND (B-)
    ConveyorController conveyor(makeBenchConveyorConfig());
    DcMotorBts7960     chuckMotor(
        HardwarePins::PIN_BTS_CHUCK_RPWM,
        HardwarePins::PIN_BTS_CHUCK_EN,
        DcMotorBts7960::UNUSED_PIN,
        HardwarePins::PIN_ADC_CHUCK_CURRENT);

    // 2 Motores a Pasos (Z1 Llenado en 37/36/35 y Z2 Tapado en 34/33/32)
    StepperAxis fillZ(makeBenchFillZConfig());
    StepperAxis capZ(makeBenchCapZConfig());

    // Pantalla TFT + Botones táctiles de prueba
    DisplayManager      display;
    Adafruit_GFX_Button btnToggleBelt;
    Adafruit_GFX_Button btnToggleChuck;
    Adafruit_GFX_Button btnFillZUp;
    Adafruit_GFX_Button btnFillZDown;
    Adafruit_GFX_Button btnCapZUp;
    Adafruit_GFX_Button btnCapZDown;

    ScreenId lastRenderedScreen = ScreenId::None;
    bool     lastBeltRunning    = false;
    bool     lastChuckRunning   = false;
    uint16_t lastBeltAdc        = 0xFFFFU;
    uint16_t lastChuckAdc       = 0xFFFFU;
    uint8_t  lastSensorMask     = 0xFFU;

    int32_t  lastFillZSteps     = 0x7FFFFFFFL;
    int32_t  lastCapZSteps      = 0x7FFFFFFFL;
    bool     lastFillZBusy      = false;
    bool     lastCapZBusy       = false;

    uint32_t lastSensorPollMs   = 0;
    uint32_t lastMotionMs       = 0;
    uint32_t lastUiRefreshMs    = 0;
    uint32_t lastAdcDisplayMs   = 0;

    uint8_t packSensorStates()
    {
        uint8_t mask = 0;
        if (sensPresenceFill.isTriggered()) { mask |= (1U << 0); }
        if (sensPresenceCap.isTriggered())  { mask |= (1U << 1); }
        if (sensLimitFill.isTriggered())    { mask |= (1U << 2); }
        if (sensLimitCap.isTriggered())     { mask |= (1U << 3); }
        if (sensEstopAux.isTriggered())     { mask |= (1U << 4); }
        if (sensBtnReset.isTriggered())     { mask |= (1U << 5); }
        return mask;
    }

    void drawBottomDcButtons(bool beltRunning, bool chuckRunning)
    {
        MCUFRIEND_kbv& tft = display.getTft();

        // Botón [BANDA] en la barra inferior (X = 126..218, Y = 190..232) -> Controla M+
        const uint16_t beltCol = beltRunning ? COLOR_RED : COLOR_GREEN;
        tft.fillRoundRect(126, 190, 92, 42, 5, beltCol);
        tft.drawRoundRect(126, 190, 92, 42, 5, COLOR_WHITE);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_BLACK, beltCol);
        tft.setCursor(136, 198);
        tft.print(beltRunning ? F("PARAR") : F("MOVER"));
        tft.setCursor(136, 212);
        tft.print(F("BANDA M+"));

        // Botón [TAPADO] en la barra inferior (X = 222..314, Y = 190..232) -> Controla M-
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
        const int16_t y = static_cast<int16_t>(74 + (rowIdx * 17));

        tft.setTextSize(1);
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
        tft.setCursor(8, y);
        tft.print(label);

        tft.setCursor(192, y);
        if (active)
        {
            tft.setTextColor(COLOR_GREEN, COLOR_BLACK);
            tft.print(F("[ ACTIVO ] "));
        }
        else
        {
            tft.setTextColor(COLOR_YELLOW, COLOR_BLACK);
            tft.print(F("[ LIBRE  ] "));
        }
    }

    void drawHomeBenchStatic()
    {
        MCUFRIEND_kbv& tft = display.getTft();
        tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, DisplayManager::CONTENT_HEIGHT, COLOR_BLACK);

        tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, 20, COLOR_DARK);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_CYAN, COLOR_DARK);
        tft.setCursor(6, 2);
        tft.print(F("HOME: 1xBTS + SENSORES"));

        tft.drawFastHLine(0, 68, DisplayManager::SCREEN_WIDTH, COLOR_WHITE);
    }

    void updateHomeBenchDynamic(bool forceRedraw)
    {
        MCUFRIEND_kbv& tft = display.getTft();
        const bool beltOn  = conveyor.isRunning();
        const bool chuckOn = chuckMotor.isRunning();
        const uint8_t sensorMask = packSensorStates();

        if (forceRedraw)
        {
            drawHomeBenchStatic();
            drawBottomDcButtons(beltOn, chuckOn);
        }

        if (forceRedraw || (beltOn != lastBeltRunning) || (chuckOn != lastChuckRunning))
        {
            tft.setTextSize(1);
            tft.setCursor(8, 25);
            tft.setTextColor(beltOn ? COLOR_GREEN : COLOR_WHITE, COLOR_BLACK);
            tft.print(beltOn ? F("Banda M+(P44/41): ON ") : F("Banda M+(P44/41): OFF"));

            tft.setCursor(164, 25);
            tft.setTextColor(chuckOn ? COLOR_CYAN : COLOR_WHITE, COLOR_BLACK);
            tft.print(chuckOn ? F("Tapa M-(P45/40): ON ") : F("Tapa M-(P45/40): OFF"));

            drawBottomDcButtons(beltOn, chuckOn);
            lastBeltRunning  = beltOn;
            lastChuckRunning = chuckOn;
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
                tft.setTextColor(COLOR_YELLOW, COLOR_BLACK);
                tft.setCursor(8, 42);
                tft.print(F("R_IS Banda(A9): "));
                tft.print(bAdc);
                tft.print(F("   "));

                tft.setCursor(164, 42);
                tft.print(F("L_IS Tapa(A8): "));
                tft.print(cAdc);
                tft.print(F("   "));

                tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
                tft.setCursor(8, 56);
                tft.print(F("(Toque Engrane abajo para probar Steppers Z1/Z2)"));

                lastBeltAdc  = bAdc;
                lastChuckAdc = cAdc;
            }
        }

        const uint8_t diff = forceRedraw ? 0x3FU : static_cast<uint8_t>(sensorMask ^ lastSensorMask);

        if ((diff & (1U << 0)) != 0U)
        {
            drawSensorRow(0, F("P22 Presencia Llenado :"), (sensorMask & (1U << 0)) != 0U);
        }
        if ((diff & (1U << 1)) != 0U)
        {
            drawSensorRow(1, F("P23 Presencia Tapado  :"), (sensorMask & (1U << 1)) != 0U);
        }
        if ((diff & (1U << 2)) != 0U)
        {
            drawSensorRow(2, F("P24 Fin Carrera Z1    :"), (sensorMask & (1U << 2)) != 0U);
        }
        if ((diff & (1U << 3)) != 0U)
        {
            drawSensorRow(3, F("P25 Fin Carrera Z2    :"), (sensorMask & (1U << 3)) != 0U);
        }
        if ((diff & (1U << 4)) != 0U)
        {
            drawSensorRow(4, F("P27 Seta E-STOP (NC)  :"), (sensorMask & (1U << 4)) != 0U);
        }
        if ((diff & (1U << 5)) != 0U)
        {
            drawSensorRow(5, F("P28 Boton Reset Fisico:"), (sensorMask & (1U << 5)) != 0U);
        }

        lastSensorMask = sensorMask;
    }

    void drawStepperBoxButton(int16_t x, int16_t y, const __FlashStringHelper* line1, uint16_t color)
    {
        MCUFRIEND_kbv& tft = display.getTft();
        tft.fillRoundRect(x, y, 142, 38, 6, color);
        tft.drawRoundRect(x, y, 142, 38, 6, COLOR_WHITE);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE, color);
        tft.setCursor(x + 14, y + 11);
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
        tft.print(F("STEPPERS Z1 / Z2"));

        // Columna izquierda: Z1 Llenado (37/36/35)
        tft.setTextSize(1);
        tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
        tft.setCursor(10, 24);
        tft.print(F("Z1 LLENADO (P37/36/35)"));

        // Columna derecha: Z2 Tapado (34/33/32)
        tft.setCursor(168, 24);
        tft.print(F("Z2 TAPADO (P34/33/32)"));

        // 4 Botones de Jog (Subir / Bajar)
        drawStepperBoxButton(10,  76, F("Z1 SUBIR"), COLOR_BLUE);
        drawStepperBoxButton(10, 124, F("Z1 BAJAR"), COLOR_DARK);
        drawStepperBoxButton(168, 76, F("Z2 SUBIR"), COLOR_BLUE);
        drawStepperBoxButton(168, 124, F("Z2 BAJAR"), COLOR_DARK);
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
            tft.setCursor(10, 38);
            tft.print(F("Pasos: "));
            tft.print(fSteps);
            tft.print(F("      "));

            tft.setCursor(10, 52);
            tft.setTextColor(fBusy ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK);
            tft.print(fBusy ? F("Estado: MOVIENDO ") : F("Estado: REPOSO   "));

            lastFillZSteps = fSteps;
            lastFillZBusy  = fBusy;
        }

        const int32_t cSteps = capZ.getCurrentSteps();
        const bool    cBusy  = capZ.isBusy();
        if (forceRedraw || (cSteps != lastCapZSteps) || (cBusy != lastCapZBusy))
        {
            tft.setTextSize(1);
            tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
            tft.setCursor(168, 38);
            tft.print(F("Pasos: "));
            tft.print(cSteps);
            tft.print(F("      "));

            tft.setCursor(168, 52);
            tft.setTextColor(cBusy ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK);
            tft.print(cBusy ? F("Estado: MOVIENDO ") : F("Estado: REPOSO   "));

            lastCapZSteps = cSteps;
            lastCapZBusy  = cBusy;
        }
    }

    void toggleConveyorBelt()
    {
        if (conveyor.isRunning())
        {
            conveyor.stop();
            digitalWrite(HardwarePins::PIN_STATUS_LED, LOW);
            Serial.println(F("[TEST] Motor Banda DETENIDO (Pin 44 RPWM=0, Pin 41 R_EN=LOW, Borne M+=0V)"));
        }
        else
        {
            conveyor.clearStall();
            conveyor.start();
            digitalWrite(HardwarePins::PIN_STATUS_LED, HIGH);
            Serial.print(F("[TEST] Motor Banda ENCENDIDO (Pin 44 RPWM="));
            Serial.print(ProcessParams::BELT_PWM);
            Serial.println(F(", Pin 41 R_EN=HIGH, Borne M+ activo)"));
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

    void logSensorChangesToSerial()
    {
        if (sensPresenceFill.hasChanged())
        {
            Serial.print(F("[SENSOR] P22 Presencia Llenado -> "));
            Serial.println(sensPresenceFill.isTriggered() ? F("ACTIVO") : F("LIBRE"));
        }
        if (sensPresenceCap.hasChanged())
        {
            Serial.print(F("[SENSOR] P23 Presencia Tapado -> "));
            Serial.println(sensPresenceCap.isTriggered() ? F("ACTIVO") : F("LIBRE"));
        }
        if (sensLimitFill.hasChanged())
        {
            Serial.print(F("[SENSOR] P24 Fin Carrera Z1 -> "));
            Serial.println(sensLimitFill.isTriggered() ? F("ACTIVO") : F("LIBRE"));
        }
        if (sensLimitCap.hasChanged())
        {
            Serial.print(F("[SENSOR] P25 Fin Carrera Z2 -> "));
            Serial.println(sensLimitCap.isTriggered() ? F("ACTIVO") : F("LIBRE"));
        }
        if (sensEstopAux.hasChanged())
        {
            Serial.print(F("[SENSOR] P27 Seta E-STOP -> "));
            Serial.println(sensEstopAux.isTriggered() ? F("ACTIVA") : F("LIBRE"));
        }
        if (sensBtnReset.hasChanged())
        {
            Serial.print(F("[SENSOR] P28 Boton Reset -> "));
            Serial.println(sensBtnReset.isTriggered() ? F("PULSADO") : F("LIBRE"));
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

            case '1':
                fillZ.jogUp(BENCH_JOG_STEPS, BENCH_JOG_SPEED);
                Serial.println(F("[STEPPER] Z1 Llenado SUBIR 1600 pasos"));
                break;

            case '2':
                fillZ.jogDown(BENCH_JOG_STEPS, BENCH_JOG_SPEED);
                Serial.println(F("[STEPPER] Z1 Llenado BAJAR 1600 pasos"));
                break;

            case '3':
                capZ.jogUp(BENCH_JOG_STEPS, BENCH_JOG_SPEED);
                Serial.println(F("[STEPPER] Z2 Tapado SUBIR 1600 pasos"));
                break;

            case '4':
                capZ.jogDown(BENCH_JOG_STEPS, BENCH_JOG_SPEED);
                Serial.println(F("[STEPPER] Z2 Tapado BAJAR 1600 pasos"));
                break;

            case 's':
            case 'S':
                conveyor.stop();
                chuckMotor.stop();
                fillZ.stop();
                capZ.stop();
                Serial.println(F("[TEST] PARO TOTAL de motores"));
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

    sensPresenceFill.init();
    sensPresenceCap.init();
    sensLimitFill.init();
    sensLimitCap.init();
    sensEstopAux.init();
    sensBtnReset.init();

    conveyor.init();
    chuckMotor.init();
    fillZ.init();
    capZ.init();

    display.init();
    char emptyLabel[] = "";
    MCUFRIEND_kbv& tft = display.getTft();

    // Botones de la barra inferior (activos en ambas pantallas)
    btnToggleBelt.initButton(&tft,  172, 211, 92, 42, 0, 0, 0, emptyLabel, 1);
    btnToggleChuck.initButton(&tft, 268, 211, 92, 42, 0, 0, 0, emptyLabel, 1);

    // Botones de la pantalla Settings para mover los 2 motores a pasos
    btnFillZUp.initButton(&tft,    81,  95, 142, 38, 0, 0, 0, emptyLabel, 1);
    btnFillZDown.initButton(&tft,  81, 143, 142, 38, 0, 0, 0, emptyLabel, 1);
    btnCapZUp.initButton(&tft,    239,  95, 142, 38, 0, 0, 0, emptyLabel, 1);
    btnCapZDown.initButton(&tft,  239, 143, 142, 38, 0, 0, 0, emptyLabel, 1);

    display.showScreen(ScreenId::Home);
    lastRenderedScreen = ScreenId::Home;
    updateHomeBenchDynamic(true);

    Serial.println(F("=========================================================="));
    Serial.println(F(" MODO PRUEBA EN BANCO: 1 MODULO BTS7960 PARA 2 MOTORES DC"));
    Serial.println(F(" - Motor 1 Banda (M+) : RPWM=44, R_EN=41, R_IS=A9 (Tecla 'b')"));
    Serial.println(F(" - Motor 2 Tapa  (M-) : LPWM=45, L_EN=40, L_IS=A8 (Tecla 'c')"));
    Serial.println(F(" - Stepper Z1 Llenado : STEP=37, DIR=36, EN=35   (Teclas '1'/'2')"));
    Serial.println(F(" - Stepper Z2 Tapado  : STEP=34, DIR=33, EN=32   (Teclas '3'/'4')"));
    Serial.println(F(" - Sensores           : 22, 23, 24, 25, 27, 28   ('s' = Paro)"));
    Serial.println(F("=========================================================="));
}

void loop()
{
    const uint32_t nowMs = millis();

    // 1. Muestreo de los 6 sensores digitales (cada 1 ms)
    if (nowMs - lastSensorPollMs >= SENSOR_POLL_PERIOD_MS)
    {
        lastSensorPollMs = nowMs;

        sensPresenceFill.update();
        sensPresenceCap.update();
        sensLimitFill.update();
        sensLimitCap.update();
        sensEstopAux.update();
        sensBtnReset.update();

        logSensorChangesToSerial();

        if (sensBtnReset.onRisingEdge())
        {
            toggleConveyorBelt();
        }
    }

    // 2. Comandos por Monitor Serie
    handleSerialCommands();

    // 3. Perfiles de movimiento y muestreo ADC de ambos canales del BTS7960 (cada 20 ms)
    if (nowMs - lastMotionMs >= MOTION_PERIOD_MS)
    {
        lastMotionMs = nowMs;
        conveyor.update();
        (void)chuckMotor.sampleCurrentAdc();
        fillZ.updateMotionProfile();
        capZ.updateMotionProfile();
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

        // Botones inferiores para los 2 canales del BTS7960 (activos en Home y Settings)
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
            updateHomeBenchDynamic(screenChanged);
        }
        else if (currentScreen == ScreenId::Settings)
        {
            btnFillZUp.press(touched && btnFillZUp.contains(px, py));
            btnFillZDown.press(touched && btnFillZDown.contains(px, py));
            btnCapZUp.press(touched && btnCapZUp.contains(px, py));
            btnCapZDown.press(touched && btnCapZDown.contains(px, py));

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
            if (btnCapZUp.justReleased())
            {
                if (capZ.isBusy()) { capZ.stop(); }
                else { capZ.jogUp(BENCH_JOG_STEPS, BENCH_JOG_SPEED); }
                Serial.println(F("[TFT] Boton Z2 SUBIR"));
            }
            if (btnCapZDown.justReleased())
            {
                if (capZ.isBusy()) { capZ.stop(); }
                else { capZ.jogDown(BENCH_JOG_STEPS, BENCH_JOG_SPEED); }
                Serial.println(F("[TFT] Boton Z2 BAJAR"));
            }

            updateSettingsSteppersDynamic(screenChanged);
        }
    }
}
