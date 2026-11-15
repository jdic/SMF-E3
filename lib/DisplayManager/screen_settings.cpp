/**
 * @file screen_settings.cpp
 * @brief Renderizado de la pantalla de ajustes, modo de prueba y recuperación manual (ScreenId::Settings).
 */

#include "display_manager.hpp"
#include "ui_theme.hpp"
#include "ui_strings.hpp"

void DisplayManager::drawSettingsStaticFrame()
{
    // Encabezado fijo de Settings y Modo de Prueba
    tft_.fillRect(0, 0, SCREEN_WIDTH, 28, UiTheme::COLOR_BLUE_HEADER);
    tft_.setTextSize(2);
    tft_.setTextColor(UiTheme::COLOR_WHITE, UiTheme::COLOR_BLUE_HEADER);
    tft_.setCursor(10, 6);
    tft_.print(F("AJUSTES Y TEST MODE"));

    // Tarjeta 1 (Izquierda): Checkbox TEST MODE
    const UiRect cardTest = UiLayout::SETTINGS_CARD_TEST_MODE;
    tft_.drawRect(cardTest.x, cardTest.y, cardTest.w, cardTest.h, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_WHITE, UiTheme::COLOR_BG);
    tft_.setCursor(62, 44);
    tft_.print(F("TEST MODE"));
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(62, 58);
    tft_.print(F("Actuadores man."));

    // Tarjeta 2 (Derecha): Checkbox OVERLAY V/I
    const UiRect cardOverlay = UiLayout::SETTINGS_CARD_OVERLAY;
    tft_.drawRect(cardOverlay.x, cardOverlay.y, cardOverlay.w, cardOverlay.h, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_YELLOW_WARN, UiTheme::COLOR_BG);
    tft_.setCursor(218, 44);
    tft_.print(F("OVERLAY V/I"));
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(218, 58);
    tft_.print(F("HUD ADC | Amp"));

    // Tarjeta de formato de botella (Y = 90..120)
    tft_.drawRect(6, 90, 178, 30, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(12, 101);
    tft_.print(F("FORMATO:"));
}

void DisplayManager::renderSettingsDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    const bool isTestMode     = (snapshot.mode == StationMode::TestMode);
    const bool prevIsTestMode = (renderedSnapshot_.mode == StationMode::TestMode);

    // 1. Icono Checkbox 48x48 de TEST MODE (X = 10, Y = 34)
    if (fullRedraw || (isTestMode != prevIsTestMode))
    {
        if (!fullRedraw)
        {
            tft_.fillRect(10, 34, 48, 48, UiTheme::COLOR_BG);
        }
        if (isTestMode)
        {
            tft_.drawBitmap(10, 34, UIIcons::ICON_CHECKBOX_ON_48x48, 48, 48, UiTheme::COLOR_GREEN_OK);
        }
        else
        {
            tft_.drawBitmap(10, 34, UIIcons::ICON_CHECKBOX_OFF_48x48, 48, 48, UiTheme::COLOR_WHITE);
        }
    }

    // 1b. Icono Checkbox 48x48 de OVERLAY V/I (X = 166, Y = 34)
    if (fullRedraw)
    {
        if (showPowerOverlay_)
        {
            tft_.drawBitmap(166, 34, UIIcons::ICON_CHECKBOX_ON_48x48, 48, 48, UiTheme::COLOR_YELLOW_WARN);
        }
        else
        {
            tft_.drawBitmap(166, 34, UIIcons::ICON_CHECKBOX_OFF_48x48, 48, 48, UiTheme::COLOR_WHITE);
        }
    }

    const bool canChangeProfile =
        (snapshot.fillState == FillState::Idle) &&
        (snapshot.capState == CappingState::Idle) &&
        (snapshot.queueCount == 0U);

    const bool prevCanChangeProfile =
        (renderedSnapshot_.fillState == FillState::Idle) &&
        (renderedSnapshot_.capState == CappingState::Idle) &&
        (renderedSnapshot_.queueCount == 0U);

    // 2. Valor del perfil activo y botón [CAMBIAR PERFIL]
    if (fullRedraw ||
        (snapshot.profile != renderedSnapshot_.profile) ||
        (canChangeProfile != prevCanChangeProfile))
    {
        tft_.setTextSize(2);
        tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft_.setCursor(66, 97);
        tft_.print(UiStrings::getProfileProgmem(snapshot.profile));

        const uint16_t btnFill   = canChangeProfile ? UiTheme::COLOR_BLUE_HEADER : UiTheme::COLOR_BG;
        const uint16_t btnBorder = canChangeProfile ? UiTheme::COLOR_CYAN : UiTheme::COLOR_DARK_GRAY;
        const uint16_t btnText   = canChangeProfile ? UiTheme::COLOR_WHITE : UiTheme::COLOR_DARK_GRAY;

        UiWidgets::drawButton(
            tft_,
            UiLayout::SETTINGS_BTN_PROFILE,
            btnFill, btnBorder, btnText,
            F("CAMBIAR PERFIL"), 1, fullRedraw);
    }

    // 3. Cuadrícula de 4 botones de Recuperación Manual (Y = 126..184)
    const bool inRecovery     = (snapshot.mode == StationMode::ManualRecovery);
    const bool prevInRecovery = (renderedSnapshot_.mode == StationMode::ManualRecovery);

    if (fullRedraw || (inRecovery != prevInRecovery))
    {
        const uint16_t jogFill   = inRecovery ? UiTheme::COLOR_BLUE_HEADER : UiTheme::COLOR_BG;
        const uint16_t jogBorder = inRecovery ? UiTheme::COLOR_CYAN : UiTheme::COLOR_DARK_GRAY;
        const uint16_t jogText   = inRecovery ? UiTheme::COLOR_WHITE : UiTheme::COLOR_DARK_GRAY;

        const uint16_t clrFill   = inRecovery ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_BG;
        const uint16_t clrBorder = inRecovery ? UiTheme::COLOR_WHITE : UiTheme::COLOR_DARK_GRAY;
        const uint16_t clrText   = inRecovery ? UiTheme::COLOR_WHITE : UiTheme::COLOR_DARK_GRAY;

        UiWidgets::drawButton(
            tft_,
            UiLayout::SETTINGS_BTN_JOG_FILL_UP,
            jogFill, jogBorder, jogText,
            F("SUBIR Z LLENADO"), 1, fullRedraw);

        UiWidgets::drawButton(
            tft_,
            UiLayout::SETTINGS_BTN_JOG_CAP_UP,
            jogFill, jogBorder, jogText,
            F("SUBIR Z TAPADO"), 1, fullRedraw);

        UiWidgets::drawButton(
            tft_,
            UiLayout::SETTINGS_BTN_RELEASE_ARMS,
            jogFill, jogBorder, jogText,
            F("SOLTAR BRAZOS"), 1, fullRedraw);

        UiWidgets::drawButton(
            tft_,
            UiLayout::SETTINGS_BTN_LINE_CLEAR,
            clrFill, clrBorder, clrText,
            F("LINEA DESPEJADA"), 1, fullRedraw);
    }
}
