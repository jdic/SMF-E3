/**
 * @file screen_settings.cpp
 * @brief Renderizado de las 3 subpáginas de Ajustes (ScreenId::Settings) y la vista bloqueada de Recuperación Manual.
 */

#include "display_manager.hpp"
#include "ui_theme.hpp"
#include "ui_strings.hpp"

namespace
{
    void drawCheckboxCardFrame(
        MCUFRIEND_kbv& tft,
        UiRect card,
        const __FlashStringHelper* title,
        const __FlashStringHelper* subtitle,
        uint16_t titleColor = UiTheme::COLOR_WHITE)
    {
        tft.drawRect(card.x, card.y, card.w, card.h, UiTheme::COLOR_DARK_GRAY);
        tft.setTextSize(1);
        tft.setTextColor(titleColor, UiTheme::COLOR_BG);
        tft.setCursor(card.x + 50, card.y + 12);
        tft.print(title);
        tft.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
        tft.setCursor(card.x + 50, card.y + 26);
        tft.print(subtitle);
    }

    void drawCheckboxState(
        MCUFRIEND_kbv& tft,
        UiRect card,
        bool checked,
        uint16_t activeColor,
        bool fullRedraw)
    {
        if (!fullRedraw)
        {
            tft.fillRect(card.x + 6, card.y + 4, 40, 40, UiTheme::COLOR_BG);
        }
        tft.drawBitmap(
            card.x + 2,
            card.y,
            checked ? UIIcons::ICON_CHECKBOX_ON_48x48 : UIIcons::ICON_CHECKBOX_OFF_48x48,
            48,
            48,
            checked ? activeColor : UiTheme::COLOR_WHITE);
    }

    void drawZRecoveryRow(
        MCUFRIEND_kbv& tft,
        int16_t y,
        int32_t steps,
        bool isHomeSw)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%5ld %s", static_cast<long>(steps), isHomeSw ? "[HOME]" : "LIBRE ");
        tft.setTextSize(1);
        tft.setTextColor(isHomeSw ? UiTheme::COLOR_GREEN_OK : UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
        tft.setCursor(30, y);
        tft.print(buf);
    }
} // namespace

void DisplayManager::drawSettingsStaticFrame()
{
    tft_.fillRect(0, 0, SCREEN_WIDTH, 28, UiTheme::COLOR_BLUE_HEADER);
    tft_.setTextSize(2);
    tft_.setTextColor(UiTheme::COLOR_WHITE, UiTheme::COLOR_BLUE_HEADER);
    tft_.setCursor(8, 6);

    if (settingsPage_ == 0U)
    {
        tft_.print(F("AJUSTES 1/3"));
    }
    else if (settingsPage_ == 1U)
    {
        tft_.print(F("AJUSTES 2/3"));
    }
    else
    {
        tft_.print(F("AJUSTES 3/3"));
    }

    UiWidgets::drawButton(
        tft_,
        UiLayout::SETTINGS_BTN_NEXT_PAGE,
        UiTheme::COLOR_BG,
        UiTheme::COLOR_CYAN,
        UiTheme::COLOR_CYAN,
        F("PAG SIG. >>"),
        1);

    if (settingsPage_ == 0U)
    {
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_TL, F("TEST MODE"), F("Actuadores man."));
        drawCheckboxCardFrame(
            tft_, UiLayout::SETTINGS_SLOT_TR, F("OVERLAY V/I"), F("HUD ADC | Amp"), UiTheme::COLOR_YELLOW_WARN);
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_ML, F("EST. LLENADO"), F("Automata 1"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_MR, F("EST. TAPADO"), F("Automata 2"));

        tft_.drawRect(6, 136, 178, 48, UiTheme::COLOR_DARK_GRAY);
        tft_.setTextSize(1);
        tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
        tft_.setCursor(14, 144);
        tft_.print(F("FORMATO:"));
    }
    else if (settingsPage_ == 1U)
    {
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_TL, F("VISION ESP32"), F("Check tapa E304"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_TR, F("AVANCE VISION"), F("Mover tras tap."));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_ML, F("COLA WIP"), F("Limite interet."));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_MR, F("BUZZER ALARMA"), F("Audio global"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_BL, F("COMP. I*R"), F("Boost carga"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_BR, F("ATASCO BANDA"), F("Prot. IS E303"));
    }
    else
    {
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_TL, F("E101/102 BOT."), F("Botella perdida"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_TR, F("E201/12 Z-LIM"), F("Homing y soft-l"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_ML, F("E203/14 Z-TMO"), F("Timeout ejes Z"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_MR, F("E301/02 APR."), F("Torque/roscado"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_BL, F("E901 E-STOP"), F("Seta fisica HW"));
        drawCheckboxCardFrame(tft_, UiLayout::SETTINGS_SLOT_BR, F("A902 SCADA"), F("Aviso enlace"));
    }
}

void DisplayManager::renderSettingsDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    const uint16_t diffCfg = snapshot.configFlags ^ renderedSnapshot_.configFlags;

    if (settingsPage_ == 0U)
    {
        const bool isTestMode     = (snapshot.mode == StationMode::TestMode);
        const bool prevIsTestMode = (renderedSnapshot_.mode == StationMode::TestMode);
        if (fullRedraw || (isTestMode != prevIsTestMode))
        {
            drawCheckboxState(tft_, UiLayout::SETTINGS_SLOT_TL, isTestMode, UiTheme::COLOR_GREEN_OK, fullRedraw);
        }

        if (fullRedraw)
        {
            drawCheckboxState(
                tft_, UiLayout::SETTINGS_SLOT_TR, showPowerOverlay_, UiTheme::COLOR_YELLOW_WARN, true);
        }

        if (fullRedraw || ((diffCfg & ConfigFlags::FILL_STATION) != 0U))
        {
            drawCheckboxState(
                tft_,
                UiLayout::SETTINGS_SLOT_ML,
                (snapshot.configFlags & ConfigFlags::FILL_STATION) != 0U,
                UiTheme::COLOR_GREEN_OK,
                fullRedraw);
        }

        if (fullRedraw || ((diffCfg & ConfigFlags::CAP_STATION) != 0U))
        {
            drawCheckboxState(
                tft_,
                UiLayout::SETTINGS_SLOT_MR,
                (snapshot.configFlags & ConfigFlags::CAP_STATION) != 0U,
                UiTheme::COLOR_GREEN_OK,
                fullRedraw);
        }

        const bool canChangeProfile =
            (snapshot.fillState == FillState::Idle) &&
            (snapshot.capState == CappingState::Idle) &&
            (snapshot.queueCount == 0U);
        const bool prevCanChangeProfile =
            (renderedSnapshot_.fillState == FillState::Idle) &&
            (renderedSnapshot_.capState == CappingState::Idle) &&
            (renderedSnapshot_.queueCount == 0U);

        if (fullRedraw ||
            (snapshot.profile != renderedSnapshot_.profile) ||
            (canChangeProfile != prevCanChangeProfile))
        {
            tft_.setTextSize(2);
            tft_.setTextColor(UiTheme::COLOR_CYAN, UiTheme::COLOR_BG);
            tft_.setCursor(14, 158);
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
        return;
    }

    const UiRect slots[6] = {
        UiLayout::SETTINGS_SLOT_TL,
        UiLayout::SETTINGS_SLOT_TR,
        UiLayout::SETTINGS_SLOT_ML,
        UiLayout::SETTINGS_SLOT_MR,
        UiLayout::SETTINGS_SLOT_BL,
        UiLayout::SETTINGS_SLOT_BR
    };

    const uint16_t page2Masks[6] = {
        ConfigFlags::VISION_INSPECT,
        ConfigFlags::VISION_ADVANCE,
        ConfigFlags::INTERSTAGE_QUEUE,
        ConfigFlags::BUZZER_ENABLED,
        ConfigFlags::BELT_IR_COMP,
        ConfigFlags::BELT_STALL_PROT
    };

    const uint16_t page3Masks[6] = {
        ConfigFlags::ERR_BOTTLE_LOST,
        ConfigFlags::ERR_Z_HOME_LIMIT,
        ConfigFlags::ERR_Z_TIMEOUT,
        ConfigFlags::ERR_CHUCK_TORQUE,
        ConfigFlags::ERR_ESTOP_HW,
        ConfigFlags::ERR_SCADA_LINK
    };

    const uint16_t* activeMasks = (settingsPage_ == 1U) ? page2Masks : page3Masks;
    for (uint8_t i = 0U; i < 6U; ++i)
    {
        const uint16_t mask = activeMasks[i];
        if (fullRedraw || ((diffCfg & mask) != 0U))
        {
            drawCheckboxState(
                tft_,
                slots[i],
                (snapshot.configFlags & mask) != 0U,
                UiTheme::COLOR_GREEN_OK,
                fullRedraw);
        }
    }
}

void DisplayManager::drawRecoveryStaticFrame()
{
    // 1. Encabezado ámbar de modo Recuperación Manual
    tft_.fillRect(0, 0, SCREEN_WIDTH, 28, UiTheme::COLOR_ORANGE_REC);
    tft_.setTextSize(2);
    tft_.setTextColor(UiTheme::COLOR_BG, UiTheme::COLOR_ORANGE_REC);
    tft_.setCursor(10, 6);
    tft_.print(F("MODO: RECUPERACION MAN."));

    // 2. Tarjeta de Causa del Paro (Y = 32..68)
    tft_.drawRect(2, 32, 316, 36, UiTheme::COLOR_RED_ALARM);
    tft_.setTextSize(1);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(8, 37);
    tft_.print(F("CAUSA DEL PARO (CORREGIR ANTES DE REARMAR):"));

    // 3. Tarjeta de Estado en Vivo de Ejes Z y E-STOP (Y = 72..102)
    tft_.drawRect(2, 72, 316, 30, UiTheme::COLOR_DARK_GRAY);
    tft_.setTextColor(UiTheme::COLOR_LIGHT_GRAY, UiTheme::COLOR_BG);
    tft_.setCursor(8, 77);
    tft_.print(F("Z1:"));
    tft_.setCursor(8, 89);
    tft_.print(F("Z2:"));
    tft_.setCursor(176, 77);
    tft_.print(F("ESTOP:"));
    tft_.setTextColor(UiTheme::COLOR_ORANGE_REC, UiTheme::COLOR_BG);
    tft_.setCursor(176, 89);
    tft_.print(F("NAVEGACION BLOQUEADA"));

    // 4. Botones de asistencia a pantalla completa (Y = 108..232)
    UiWidgets::drawButton(
        tft_,
        UiLayout::RECOVERY_BTN_JOG_FILL_UP,
        UiTheme::COLOR_BLUE_HEADER,
        UiTheme::COLOR_CYAN,
        UiTheme::COLOR_WHITE,
        F("SUBIR Z1 (+JOG)"),
        1,
        true);

    UiWidgets::drawButton(
        tft_,
        UiLayout::RECOVERY_BTN_JOG_CAP_UP,
        UiTheme::COLOR_BLUE_HEADER,
        UiTheme::COLOR_CYAN,
        UiTheme::COLOR_WHITE,
        F("SUBIR Z2 (+JOG)"),
        1,
        true);

    UiWidgets::drawButton(
        tft_,
        UiLayout::RECOVERY_BTN_RELEASE_ARMS,
        UiTheme::COLOR_BLUE_HEADER,
        UiTheme::COLOR_CYAN,
        UiTheme::COLOR_WHITE,
        F("SOLTAR BRAZOS DE RETENCION"),
        1,
        true);

    UiWidgets::drawButton(
        tft_,
        UiLayout::RECOVERY_BTN_LINE_CLEAR,
        UiTheme::COLOR_GREEN_OK,
        UiTheme::COLOR_WHITE,
        UiTheme::COLOR_WHITE,
        F("LINEA DESPEJADA"),
        2,
        true);
}

void DisplayManager::renderRecoveryDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (fullRedraw || (snapshot.error != renderedSnapshot_.error))
    {
        uint16_t dummyBg = 0;
        uint16_t dummyFg = 0;
        const __FlashStringHelper* errStr = UiStrings::getErrorProgmem(snapshot.error, dummyBg, dummyFg);
        tft_.setTextSize(2);
        tft_.setTextColor(UiTheme::COLOR_RED_ALARM, UiTheme::COLOR_BG);
        tft_.setCursor(6, 49);
        tft_.print(errStr);
    }

    const uint16_t diffIn = snapshot.inputFlags ^ renderedSnapshot_.inputFlags;

    if (fullRedraw ||
        (snapshot.fillZSteps != renderedSnapshot_.fillZSteps) ||
        ((diffIn & InputFlags::LIMIT_FILL) != 0U))
    {
        drawZRecoveryRow(tft_, 77, snapshot.fillZSteps, (snapshot.inputFlags & InputFlags::LIMIT_FILL) != 0U);
    }

    if (fullRedraw ||
        (snapshot.capZSteps != renderedSnapshot_.capZSteps) ||
        ((diffIn & InputFlags::LIMIT_CAP) != 0U))
    {
        drawZRecoveryRow(tft_, 89, snapshot.capZSteps, (snapshot.inputFlags & InputFlags::LIMIT_CAP) != 0U);
    }

    if (fullRedraw || ((diffIn & InputFlags::ESTOP_ACTIVE) != 0U))
    {
        UiWidgets::drawInputBadge(
            tft_,
            214,
            77,
            (snapshot.inputFlags & InputFlags::ESTOP_ACTIVE) != 0U,
            UiTheme::COLOR_RED_ALARM);
    }
}