/**
 * @file screen_home.cpp
 * @brief Renderizado estático y dinámico de la pantalla principal (ScreenId::Home).
 */

#include "display_manager.hpp"
#include "ui_theme.hpp"
#include "ui_strings.hpp"

void DisplayManager::drawHomeStaticFrame()
{
    if (hasRenderedSnapshot_ && (renderedSnapshot_.mode == StationMode::TestMode))
    {
        tft_.setTextSize(1);
        tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(8, 34);
        tft_.print(F("CONTROL MANUAL GENERAL (HOME - TEST MODE):"));
        return;
    }

    // Tarjeta Etapa 1: Llenado (Y = 34..68)
    tft_.drawRect(6, 34, 308, 34, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 47);
    tft_.print(F("LLENADO:"));

    // Tarjeta Etapa 2: Tapado (Y = 72..106)
    tft_.drawRect(6, 72, 308, 34, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 85);
    tft_.print(F("TAPADO :"));

    // Tarjeta de Contadores y Perfil (Y = 110..140)
    tft_.drawRect(6, 110, 308, 30, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(14, 121);
    tft_.print(F("COLA:"));
    tft_.setCursor(104, 121);
    tft_.print(F("PERFIL:"));
    tft_.setCursor(218, 121);
    tft_.print(F("PROD:"));
}

void DisplayManager::renderHomeDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    // 1. Banner superior de Modo Operativo (Y = 0..28)
    if (fullRedraw || (snapshot.mode != renderedSnapshot_.mode))
    {
        uint16_t bgColor = UiTheme::COLOR_BLUE_HEADER;
        uint16_t fgColor = UiTheme::COLOR_WHITE;
        const __FlashStringHelper* modeStr = UiStrings::getModeProgmem(snapshot.mode, bgColor, fgColor);

        tft_.fillRect(0, 0, SCREEN_WIDTH, 28, bgColor);
        tft_.setTextSize(2);
        tft_.setTextColor(fgColor, bgColor);
        tft_.setCursor(10, 6);
        tft_.print(modeStr);
    }

    // Vista de HOME en TEST MODE: botones manuales para Banda, Buzzer y Torreta
    if (snapshot.mode == StationMode::TestMode)
    {
        const uint16_t diffFlags = snapshot.testFlags ^ renderedSnapshot_.testFlags;

        if (fullRedraw || ((diffFlags & TestFlags::BELT_ON) != 0U))
        {
            const bool on = (snapshot.testFlags & TestFlags::BELT_ON) != 0U;
            UiWidgets::drawButton(
                tft_,
                UiLayout::HOME_BTN_TEST_BELT,
                on ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_BG,
                on ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
                UiTheme::COLOR_WHITE,
                on ? F("BANDA: ON") : F("BANDA: OFF"), 1, fullRedraw);
        }

        if (fullRedraw || ((diffFlags & TestFlags::BUZZER_ON) != 0U))
        {
            const bool on = (snapshot.testFlags & TestFlags::BUZZER_ON) != 0U;
            UiWidgets::drawButton(
                tft_,
                UiLayout::HOME_BTN_TEST_BUZZER,
                on ? UiTheme::COLOR_ORANGE_REC : UiTheme::COLOR_BG,
                on ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
                on ? UiTheme::COLOR_BG : UiTheme::COLOR_WHITE,
                on ? F("BUZZER: ON") : F("BUZZER: OFF"), 1, fullRedraw);
        }

        if (fullRedraw || ((diffFlags & TestFlags::TOWER_GREEN_ON) != 0U))
        {
            const bool on = (snapshot.testFlags & TestFlags::TOWER_GREEN_ON) != 0U;
            UiWidgets::drawButton(
                tft_,
                UiLayout::HOME_BTN_TEST_TOWER_GREEN,
                on ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_BG,
                on ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
                UiTheme::COLOR_WHITE,
                on ? F("TORRE VERDE: ON") : F("TORRE VERDE: OFF"), 1, fullRedraw);
        }

        if (fullRedraw || ((diffFlags & TestFlags::TOWER_RED_ON) != 0U))
        {
            const bool on = (snapshot.testFlags & TestFlags::TOWER_RED_ON) != 0U;
            UiWidgets::drawButton(
                tft_,
                UiLayout::HOME_BTN_TEST_TOWER_RED,
                on ? UiTheme::COLOR_RED_ALARM : UiTheme::COLOR_BG,
                on ? UiTheme::COLOR_WHITE : UiTheme::COLOR_CYAN,
                UiTheme::COLOR_WHITE,
                on ? F("TORRE ROJA: ON") : F("TORRE ROJA: OFF"), 1, fullRedraw);
        }
        return;
    }

    // 2. Fase activa del Autómata de Llenado
    if (fullRedraw || (snapshot.fillState != renderedSnapshot_.fillState))
    {
        const uint16_t stateColor =
            (snapshot.fillState == FillState::Idle) ? UiTheme::COLOR_LIGHT_GRAY : UiTheme::COLOR_CYAN;
        tft_.setTextSize(2);
        tft_.setTextColor(stateColor, UiTheme::COLOR_BG);
        tft_.setCursor(74, 43);
        tft_.print(UiStrings::getFillStateProgmem(snapshot.fillState));
    }

    // 3. Fase activa del Autómata de Tapado
    if (fullRedraw || (snapshot.capState != renderedSnapshot_.capState))
    {
        const uint16_t stateColor =
            (snapshot.capState == CappingState::Idle) ? UiTheme::COLOR_LIGHT_GRAY : UiTheme::COLOR_CYAN;
        tft_.setTextSize(2);
        tft_.setTextColor(stateColor, UiTheme::COLOR_BG);
        tft_.setCursor(74, 81);
        tft_.print(UiStrings::getCapStateProgmem(snapshot.capState));
    }

    // 4. Cola intermedia (WIP Llenado -> Tapado)
    if (fullRedraw || (snapshot.queueCount != renderedSnapshot_.queueCount))
    {
        char qBuf[8];
        snprintf(qBuf, sizeof(qBuf), "%u/%u ", snapshot.queueCount, ProcessParams::MAX_INTERSTAGE_QUEUE);
        tft_.setTextSize(1);
        tft_.setTextColor(UiTheme::COLOR_WHITE, UiTheme::COLOR_BG);
        tft_.setCursor(48, 121);
        tft_.print(qBuf);
    }

    // 5. Perfil de botella activo
    if (fullRedraw || (snapshot.profile != renderedSnapshot_.profile))
    {
        tft_.setTextSize(1);
        tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(150, 121);
        tft_.print(UiStrings::getProfileProgmem(snapshot.profile));
    }

    // 6. Conteo acumulado de producción
    if (fullRedraw || (snapshot.bottleCount != renderedSnapshot_.bottleCount))
    {
        char prodBuf[10];
        snprintf(prodBuf, sizeof(prodBuf), "%5lu", static_cast<unsigned long>(snapshot.bottleCount));
        tft_.setTextSize(1);
        tft_.setTextColor(UiTheme::COLOR_GREEN_OK, UiTheme::COLOR_BG);
        tft_.setCursor(252, 121);
        tft_.print(prodBuf);
    }

    // 7. Banner inferior de diagnóstico / código de error (Y = 144..178)
    if (fullRedraw || (snapshot.error != renderedSnapshot_.error))
    {
        uint16_t errBg = UiTheme::COLOR_BG;
        uint16_t errFg = UiTheme::COLOR_GREEN_OK;
        const __FlashStringHelper* errStr = UiStrings::getErrorProgmem(snapshot.error, errBg, errFg);

        const UiRect banner = UiLayout::HOME_BANNER_ALARM_ACK;
        if (!(fullRedraw && (errBg == UiTheme::COLOR_BG)))
        {
            tft_.fillRect(banner.x + 1, banner.y + 1, banner.w - 2, banner.h - 2, errBg);
        }
        tft_.drawRect(banner.x, banner.y, banner.w, banner.h,
                      (snapshot.error == ErrorCode::None) ? UiTheme::COLOR_DARK_GRAY : UiTheme::COLOR_WHITE);
        tft_.setTextSize(2);
        tft_.setTextColor(errFg, errBg);
        tft_.setCursor(banner.x + 6, banner.y + 9);
        tft_.print(errStr);
    }
}
