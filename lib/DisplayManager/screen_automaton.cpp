/**
 * @file screen_automaton.cpp
 * @brief Renderizado de las pantallas de los Autómatas 1 y 2 (ScreenId::AutoFill y ScreenId::AutoCap).
 */

#include "display_manager.hpp"
#include "ui_theme.hpp"
#include "ui_strings.hpp"

void DisplayManager::drawAutoFillStaticFrame()
{
    if (hasRenderedSnapshot_ && (renderedSnapshot_.mode == StationMode::TestMode))
    {
        tft_.fillRect(0, 0, SCREEN_WIDTH, 28, UiTheme::COLOR_ORANGE_REC);
        tft_.setTextSize(2);
        tft_.setTextColor(UiTheme::COLOR_BG, UiTheme::COLOR_ORANGE_REC);
        tft_.setCursor(8, 6);
        tft_.print(F("TEST AUTO 1: LLENADO"));
        return;
    }

    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
    tft_.setCursor(8, 10);
    tft_.print(F("AUT\x93MATA 1: LLENADO Y DOSIFICACI\x93N"));

    tft_.drawRect(6, 30, 308, 38, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 42);
    tft_.print(F("FASE:"));

    tft_.drawRect(6, 74, 308, 38, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 86);
    tft_.print(F("APROX / RETENC:"));
    tft_.setCursor(116, 86);
    tft_.print(F("PRE1:"));
    tft_.setCursor(190, 86);
    tft_.print(F("POS1:"));

    tft_.drawRect(6, 118, 308, 38, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 130);
    tft_.print(F("BOMBA / EJE Z1:"));
}

void DisplayManager::drawAutoCapStaticFrame()
{
    if (hasRenderedSnapshot_ && (renderedSnapshot_.mode == StationMode::TestMode))
    {
        tft_.fillRect(0, 0, SCREEN_WIDTH, 28, UiTheme::COLOR_ORANGE_REC);
        tft_.setTextSize(2);
        tft_.setTextColor(UiTheme::COLOR_BG, UiTheme::COLOR_ORANGE_REC);
        tft_.setCursor(8, 6);
        tft_.print(F("TEST AUTO 2: TAPADO"));
        return;
    }

    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
    tft_.setCursor(8, 10);
    tft_.print(F("AUT\x93MATA 2: TAPADO Y APRIETE"));

    tft_.drawRect(6, 30, 308, 38, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 42);
    tft_.print(F("FASE:"));

    tft_.drawRect(6, 74, 308, 38, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 86);
    tft_.print(F("PRESENCIA TAPADO:"));
    tft_.setCursor(124, 86);
    tft_.print(F("PRE2:"));
    tft_.setCursor(196, 86);
    tft_.print(F("POS2:"));

    tft_.drawRect(6, 118, 308, 38, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 130);
    tft_.print(F("MOTOR APRIETE (IBT-2):"));
}

void DisplayManager::renderAutomatonTestMode(const DisplaySnapshot& snapshot, bool fullRedraw, bool isFill)
{
    const uint16_t diffFlags   = snapshot.testFlags ^ renderedSnapshot_.testFlags;
    const uint16_t stopFlag    = isFill ? TestFlags::FILL_STOP_DN  : TestFlags::CAP_STOP_DN;
    const uint16_t clampFlag   = isFill ? TestFlags::FILL_CLAMP_DN : TestFlags::CAP_CLAMP_DN;
    const uint16_t actFlag     = isFill ? TestFlags::FILL_PUMP_ON  : TestFlags::CAP_CHUCK_ON;
    const uint16_t homeSwFlag  = isFill ? TestFlags::FILL_HOME_SW  : TestFlags::CAP_HOME_SW;
    const int32_t  currZSteps  = isFill ? snapshot.fillZSteps      : snapshot.capZSteps;
    const int32_t  prevZSteps  = isFill ? renderedSnapshot_.fillZSteps : renderedSnapshot_.capZSteps;

    if (fullRedraw || ((diffFlags & stopFlag) != 0U))
    {
        const bool down = (snapshot.testFlags & stopFlag) != 0U;
        const __FlashStringHelper* lbl = isFill
            ? (down ? F("TOPE 1: ABAJO") : F("TOPE 1: ARRIBA"))
            : (down ? F("TOPE 2: ABAJO") : F("TOPE 2: ARRIBA"));
        UiWidgets::drawButton(
            tft_,
            UiLayout::AUTO_BTN_TEST_STOP_ARM,
            down ? UiTheme::COLOR_ORANGE_REC : UiTheme::COLOR_BG,
            down ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
            down ? UiTheme::COLOR_BG : UiTheme::COLOR_WHITE,
            lbl, 1, fullRedraw);
    }

    if (fullRedraw || ((diffFlags & clampFlag) != 0U))
    {
        const bool down = (snapshot.testFlags & clampFlag) != 0U;
        const __FlashStringHelper* lbl = isFill
            ? (down ? F("SUJEC 1: ABAJO") : F("SUJEC 1: ARRIBA"))
            : (down ? F("SUJEC 2: ABAJO") : F("SUJEC 2: ARRIBA"));
        UiWidgets::drawButton(
            tft_,
            UiLayout::AUTO_BTN_TEST_CLAMP_ARM,
            down ? UiTheme::COLOR_ORANGE_REC : UiTheme::COLOR_BG,
            down ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
            down ? UiTheme::COLOR_BG : UiTheme::COLOR_WHITE,
            lbl, 1, fullRedraw);
    }

    if (fullRedraw || ((diffFlags & actFlag) != 0U))
    {
        const bool on = (snapshot.testFlags & actFlag) != 0U;
        const __FlashStringHelper* lbl = isFill
            ? (on ? F("BOMBA LLENADO: ENCENDIDA") : F("BOMBA LLENADO: APAGADA"))
            : (on ? F("MOTOR APRIETE: ENCENDIDO") : F("MOTOR APRIETE: APAGADO"));
        UiWidgets::drawButton(
            tft_,
            UiLayout::AUTO_BTN_TEST_ACTUATOR,
            on ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_BG,
            on ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
            UiTheme::COLOR_WHITE,
            lbl, 1, fullRedraw);
    }

    if (fullRedraw || (currZSteps != prevZSteps) || ((diffFlags & homeSwFlag) != 0U))
    {
        const bool isHomeSw = (snapshot.testFlags & homeSwFlag) != 0U;
        char zBuf[16];
        snprintf(
            zBuf,
            sizeof(zBuf),
            "%s:%5ld%s",
            isFill ? "Z1" : "Z2",
            static_cast<long>(currZSteps),
            isHomeSw ? "H" : " ");
        tft_.setTextSize(1);
        tft_.setTextColor(UiTheme::COLOR_BG, UiTheme::COLOR_ORANGE_REC);
        tft_.setCursor(254, 10);
        tft_.print(zBuf);
    }

    if (fullRedraw)
    {
        UiWidgets::drawButton(
            tft_,
            UiLayout::AUTO_BTN_TEST_JOG_UP,
            UiTheme::COLOR_BG, UiTheme::COLOR_CYAN, UiTheme::COLOR_WHITE,
            isFill ? F("SUBIR Z1 (+JOG)") : F("SUBIR Z2 (+JOG)"), 1, true);

        UiWidgets::drawButton(
            tft_,
            UiLayout::AUTO_BTN_TEST_JOG_DOWN,
            UiTheme::COLOR_BG, UiTheme::COLOR_CYAN, UiTheme::COLOR_WHITE,
            isFill ? F("BAJAR Z1 (-JOG)") : F("BAJAR Z2 (-JOG)"), 1, true);
    }
}

void DisplayManager::renderAutoFillDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (snapshot.mode == StationMode::TestMode)
    {
        renderAutomatonTestMode(snapshot, fullRedraw, true);
        return;
    }

    // 1. Tarjeta 1: Fase activa del Autómata de Llenado
    if (fullRedraw || (snapshot.fillState != renderedSnapshot_.fillState))
    {
        tft_.setTextSize(2);
        tft_.setTextColor(UiTheme::COLOR_WHITE, UiTheme::COLOR_BG);
        tft_.setCursor(60, 40);
        tft_.print(UiStrings::getFillStateProgmem(snapshot.fillState));
    }

    // 2. Tarjeta 2: Sensores de aproximación y retención (PRE1 y POS1)
    const uint16_t diffIn = snapshot.inputFlags ^ renderedSnapshot_.inputFlags;
    if (fullRedraw || ((diffIn & InputFlags::PRESENCE_FILL) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 148, 86, (snapshot.inputFlags & InputFlags::PRESENCE_FILL) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::POS_FILL) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 222, 86, (snapshot.inputFlags & InputFlags::POS_FILL) != 0U);
    }

    // 3. Tarjeta 3: Estado de la Bomba y posición del Eje Z1 en pasos
    if (fullRedraw ||
        (snapshot.fillState != renderedSnapshot_.fillState) ||
        (snapshot.fillZSteps != renderedSnapshot_.fillZSteps) ||
        ((diffIn & InputFlags::LIMIT_FILL) != 0U))
    {
        const bool isPumping = (snapshot.fillState == FillState::Filling);
        const bool isHomeSw  = (snapshot.inputFlags & InputFlags::LIMIT_FILL) != 0U;
        char z1Buf[28];
        snprintf(
            z1Buf,
            sizeof(z1Buf),
            "%s | Z1:%5ld stp %s",
            isPumping ? "ON " : "OFF",
            static_cast<long>(snapshot.fillZSteps),
            isHomeSw ? "[H]" : "   ");

        tft_.setTextSize(1);
        tft_.setTextColor(isPumping ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(116, 130);
        tft_.print(z1Buf);
    }
}

void DisplayManager::renderAutoCapDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (snapshot.mode == StationMode::TestMode)
    {
        renderAutomatonTestMode(snapshot, fullRedraw, false);
        return;
    }

    // 1. Tarjeta 1: Fase activa del Autómata de Tapado
    if (fullRedraw || (snapshot.capState != renderedSnapshot_.capState))
    {
        tft_.setTextSize(2);
        tft_.setTextColor(UiTheme::COLOR_WHITE, UiTheme::COLOR_BG);
        tft_.setCursor(60, 40);
        tft_.print(UiStrings::getCapStateProgmem(snapshot.capState));
    }

    // 2. Tarjeta 2: Sensores de aproximación y retención (PRE2 y POS2)
    const uint16_t diffIn = snapshot.inputFlags ^ renderedSnapshot_.inputFlags;
    if (fullRedraw || ((diffIn & InputFlags::PRESENCE_CAP) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 156, 86, (snapshot.inputFlags & InputFlags::PRESENCE_CAP) != 0U);
    }
    if (fullRedraw || ((diffIn & InputFlags::POS_CAP) != 0U))
    {
        UiWidgets::drawInputBadge(tft_, 228, 86, (snapshot.inputFlags & InputFlags::POS_CAP) != 0U);
    }

    // 3. Tarjeta 3: Telemetría del Motor de Apriete (PWM, IS ADC) y eje Z2
    if (fullRedraw ||
        (snapshot.chuckPwm != renderedSnapshot_.chuckPwm) ||
        (snapshot.chuckCurrentAdc != renderedSnapshot_.chuckCurrentAdc) ||
        (snapshot.capZSteps != renderedSnapshot_.capZSteps))
    {
        char capBuf[28];
        snprintf(
            capBuf,
            sizeof(capBuf),
            "PWM:%3u IS:%4u Z2:%4ld",
            snapshot.chuckPwm,
            snapshot.chuckCurrentAdc,
            static_cast<long>(snapshot.capZSteps));

        tft_.setTextSize(1);
        tft_.setTextColor((snapshot.chuckPwm > 0U) ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(150, 130);
        tft_.print(capBuf);
    }
}
