/**
 * @file screen_debug.cpp
 * @brief Renderizado de la pantalla de diagnóstico I/O (ScreenId::Debug) y del HUD Power Overlay.
 */

#include "display_manager.hpp"
#include "ui_theme.hpp"
#include "ui_strings.hpp"

void DisplayManager::drawDebugStaticFrame()
{
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
    tft_.setCursor(8, 6);
    tft_.print(F("DIAGN\x93STICO EN VIVO: ENTRADAS, EJES Y BANDA"));

    // Tarjeta 1: Sensores Ópticos y Fines de Carrera (Limit Switches) (Y = 20..70)
    tft_.drawRect(6, 20, 308, 50, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
    tft_.setCursor(12, 25);
    tft_.print(F("SENSORES \x93PTICOS Y LIMIT SWITCHES (HOME):"));

    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(12, 39);
    tft_.print(F("LLEN: PRE1:"));
    tft_.setCursor(116, 39);
    tft_.print(F("POS1:"));
    tft_.setCursor(184, 39);
    tft_.print(F("FC-Z1:"));

    tft_.setCursor(12, 53);
    tft_.print(F("TAP : PRE2:"));
    tft_.setCursor(116, 53);
    tft_.print(F("POS2:"));
    tft_.setCursor(184, 53);
    tft_.print(F("FC-Z2:"));

    // Tarjeta 2: Botonera de Control y Paro de Emergencia E-STOP (Y = 74..98)
    tft_.drawRect(6, 74, 308, 24, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(12, 82);
    tft_.print(F("BTN : STRT:"));
    tft_.setCursor(112, 82);
    tft_.print(F("STOP:"));
    tft_.setCursor(178, 82);
    tft_.print(F("RST:"));
    tft_.setCursor(238, 82);
    tft_.print(F("ESTOP:"));

    // Tarjeta 3: Posición en Pasos de los Ejes Z desde Home=0 (Y = 102..142)
    tft_.drawRect(6, 102, 308, 40, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(12, 109);
    tft_.print(F("Z1 (LLENADO):"));
    tft_.setCursor(12, 125);
    tft_.print(F("Z2 (TAPADO) :"));

    // Tarjeta 4: Banda (Voltaje, PWM, Corriente IS) y Alarma Activa (Y = 146..186)
    tft_.drawRect(6, 146, 308, 40, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(12, 153);
    tft_.print(F("BANDA:"));
    tft_.setCursor(12, 169);
    tft_.print(F("ALRM :"));
}

void DisplayManager::renderDebugDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    const uint16_t diffIn = snapshot.inputFlags ^ renderedSnapshot_.inputFlags;

    // 1. Tarjeta 1: Sensores Ópticos y Fines de Carrera
    if (fullRedraw || ((diffIn & InputFlags::PRESENCE_FILL) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 78, 39, (snapshot.inputFlags & InputFlags::PRESENCE_FILL) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::POS_FILL) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 146, 39, (snapshot.inputFlags & InputFlags::POS_FILL) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::LIMIT_FILL) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 220, 39, (snapshot.inputFlags & InputFlags::LIMIT_FILL) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::PRESENCE_CAP) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 78, 53, (snapshot.inputFlags & InputFlags::PRESENCE_CAP) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::POS_CAP) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 146, 53, (snapshot.inputFlags & InputFlags::POS_CAP) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::LIMIT_CAP) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 220, 53, (snapshot.inputFlags & InputFlags::LIMIT_CAP) != 0U);
    }

    // 2. Tarjeta 2: Botonera de Control y E-STOP
    if (fullRedraw || ((diffIn & InputFlags::BTN_START) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 76, 82, (snapshot.inputFlags & InputFlags::BTN_START) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::BTN_STOP) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 142, 82, (snapshot.inputFlags & InputFlags::BTN_STOP) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::BTN_RESET) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 202, 82, (snapshot.inputFlags & InputFlags::BTN_RESET) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::ESTOP_ACTIVE) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 274, 82, (snapshot.inputFlags & InputFlags::ESTOP_ACTIVE) != 0U, UiTheme::COLOR_RED_ALARM);
    }

    // 3. Tarjeta 3: Pasos del Eje Z1 y Z2
    if (fullRedraw ||
        (snapshot.fillZSteps != renderedSnapshot_.fillZSteps) ||
        ((diffIn & InputFlags::LIMIT_FILL) != 0U))
    {
        const bool isHomeSw = (snapshot.inputFlags & InputFlags::LIMIT_FILL) != 0U;
        char z1Buf[28];
        snprintf(
            z1Buf,
            sizeof(z1Buf),
            "%6ld stp %s",
            static_cast<long>(snapshot.fillZSteps),
            isHomeSw ? "[HOME=0]" : "        ");

        tft_.setTextSize(1);
        tft_.setTextColor(isHomeSw ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(96, 109);
        tft_.print(z1Buf);
    }

    if (fullRedraw ||
        (snapshot.capZSteps != renderedSnapshot_.capZSteps) ||
        ((diffIn & InputFlags::LIMIT_CAP) != 0U))
    {
        const bool isHomeSw = (snapshot.inputFlags & InputFlags::LIMIT_CAP) != 0U;
        char z2Buf[28];
        snprintf(
            z2Buf,
            sizeof(z2Buf),
            "%6ld stp %s",
            static_cast<long>(snapshot.capZSteps),
            isHomeSw ? "[HOME=0]" : "        ");

        tft_.setTextSize(1);
        tft_.setTextColor(isHomeSw ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(96, 125);
        tft_.print(z2Buf);
    }

    // 4. Tarjeta 4: Telemetría en vivo de la Banda y Alarma Activa
    if (fullRedraw ||
        (snapshot.beltPwm != renderedSnapshot_.beltPwm) ||
        (snapshot.beltCurrentAdc != renderedSnapshot_.beltCurrentAdc))
    {
        const uint16_t beltMv = static_cast<uint16_t>(
            (static_cast<uint32_t>(snapshot.beltPwm) * ProcessParams::BELT_SUPPLY_MV) / 255UL);
        const uint16_t voltsInt = beltMv / 1000U;
        const uint16_t voltsDec = (beltMv % 1000U) / 100U;

        char beltBuf[34];
        snprintf(
            beltBuf,
            sizeof(beltBuf),
            "%u.%uV (PWM:%3u) IS:%4u ADC",
            voltsInt,
            voltsDec,
            snapshot.beltPwm,
            snapshot.beltCurrentAdc);

        tft_.setTextSize(1);
        tft_.setTextColor((snapshot.beltPwm > ProcessParams::BELT_PWM) ? UiTheme::COLOR_ORANGE_REC : UiTheme::COLOR_CYAN,
                          UiTheme::COLOR_BG);
        tft_.setCursor(56, 153);
        tft_.print(beltBuf);
    }

    if (fullRedraw || (snapshot.error != renderedSnapshot_.error))
    {
        uint16_t dummyBg = 0;
        uint16_t dummyFg = 0;
        tft_.setTextSize(1);
        tft_.setCursor(56, 169);
        if (snapshot.error != ErrorCode::None)
        {
            tft_.setTextColor(UiTheme::COLOR_RED_ALARM, UiTheme::COLOR_BG);
            tft_.print(UiStrings::getErrorProgmem(snapshot.error, dummyBg, dummyFg));
        }
        else
        {
            tft_.setTextColor(UiTheme::COLOR_GREEN_OK, UiTheme::COLOR_BG);
            tft_.print(F("OK: SISTEMA SIN ALARMAS   "));
        }
    }
}

void DisplayManager::renderPowerOverlay(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (!showPowerOverlay_ || (currentScreen_ == ScreenId::Settings) || (currentScreen_ == ScreenId::None))
    {
        return;
    }

    const uint16_t diffTest = snapshot.testFlags ^ renderedSnapshot_.testFlags;
    const uint16_t diffIn   = snapshot.inputFlags ^ renderedSnapshot_.inputFlags;

    const bool underlyingRedrawn =
        fullRedraw ||
        (snapshot.mode != renderedSnapshot_.mode) ||
        (snapshot.fillState != renderedSnapshot_.fillState) ||
        (snapshot.capState != renderedSnapshot_.capState) ||
        ((diffTest & (TestFlags::BUZZER_ON | TestFlags::FILL_CLAMP_DN | TestFlags::CAP_CLAMP_DN)) != 0U) ||
        ((currentScreen_ == ScreenId::Debug) && (diffIn != 0U));

    const bool valuesChanged =
        (snapshot.beltCurrentAdc != renderedSnapshot_.beltCurrentAdc) ||
        (snapshot.chuckCurrentAdc != renderedSnapshot_.chuckCurrentAdc) ||
        (snapshot.beltPwm != renderedSnapshot_.beltPwm) ||
        (snapshot.chuckPwm != renderedSnapshot_.chuckPwm);

    if (!underlyingRedrawn && !valuesChanged)
    {
        return;
    }

    if (underlyingRedrawn)
    {
        tft_.fillRect(185, 30, 128, 24, UiTheme::COLOR_BG);
        tft_.drawRect(184, 29, 130, 26, UiTheme::COLOR_YELLOW_WARN);
    }

    // Conversión entera de ADC (0..1023 -> 0..5.0V en R_IS=1kOhm, k_ILIS=8500 -> ~42.5A FS) a décimas de Amperio
    const uint16_t clampTenthsA = static_cast<uint16_t>(
        (static_cast<uint32_t>(snapshot.chuckCurrentAdc) * 425UL + 511UL) / 1023UL);
    const uint16_t fillTenthsA = static_cast<uint16_t>(
        (static_cast<uint32_t>(snapshot.beltCurrentAdc) * 425UL + 511UL) / 1023UL);

    char line1[24];
    char line2[24];
    snprintf(
        line1,
        sizeof(line1),
        "CLAMP-%4u ADC|%2u.%uA",
        snapshot.chuckCurrentAdc,
        clampTenthsA / 10U,
        clampTenthsA % 10U);
    snprintf(
        line2,
        sizeof(line2),
        "FILL -%4u ADC|%2u.%uA",
        snapshot.beltCurrentAdc,
        fillTenthsA / 10U,
        fillTenthsA % 10U);

    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_YELLOW_WARN, UiTheme::COLOR_BG);
    tft_.setCursor(189, 33);
    tft_.print(line1);
    tft_.setCursor(189, 44);
    tft_.print(line2);
}
