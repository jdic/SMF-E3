/**
 * @file ui_theme.hpp
 * @brief Paleta de colores RGB565, geometría de rectángulos (UiRect), layout de botones y widgets primitivos.
 */

#pragma once
#include <Arduino.h>
#include <MCUFRIEND_kbv.h>
#include <avr/pgmspace.h>

/**
 * @struct UiRect
 * @brief Rectángulo 2D en coordenadas de pantalla con evaluación de colisión táctil en tiempo de compilación.
 */
struct UiRect
{
    int16_t x; ///< Coordenada X de la esquina superior izquierda [px]
    int16_t y; ///< Coordenada Y de la esquina superior izquierda [px]
    int16_t w; ///< Ancho del rectángulo [px]
    int16_t h; ///< Alto del rectángulo [px]
};

/**
 * @brief Evalúa por valor (0 bytes en SRAM) si el punto táctil (px, py) cae dentro del rectángulo.
 */
__attribute__((always_inline)) constexpr inline bool inRect(int16_t px, int16_t py, UiRect r)
{
    return (px >= r.x) && (px < (r.x + r.w)) && (py >= r.y) && (py < (r.y + r.h));
}

/**
 * @namespace UiTheme
 * @brief Paleta de colores RGB565 estandarizada para la interfaz industrial.
 */
namespace UiTheme
{
    constexpr uint16_t COLOR_BG          = 0x0000U; ///< Negro (fondo general)
    constexpr uint16_t COLOR_WHITE       = 0xFFFFU; ///< Blanco
    constexpr uint16_t COLOR_LIGHT_GRAY  = 0xC618U; ///< Gris claro (etiquetas estáticas)
    constexpr uint16_t COLOR_DARK_GRAY   = 0x3186U; ///< Gris oscuro (contornos de tarjetas y botones inactivos)
    constexpr uint16_t COLOR_CYAN        = 0x07FFU; ///< Cian (Modo Boot / Homing / acentos)
    constexpr uint16_t COLOR_BLUE_HEADER = 0x0255U; ///< Azul industrial (encabezados y botones activos)
    constexpr uint16_t COLOR_GREEN_OK    = 0x05E0U; ///< Verde operativo (Modo Running / OK)
    constexpr uint16_t COLOR_RED_ALARM   = 0xD800U; ///< Rojo alarma (Modo ErrorStop / fallos críticos)
    constexpr uint16_t COLOR_ORANGE_REC  = 0xFC00U; ///< Ámbar/Naranja (Modo ManualRecovery / TestMode)
    constexpr uint16_t COLOR_YELLOW_WARN = 0xFFE0U; ///< Amarillo (Advertencia ERR-902 / Overlay V-I)
}

/**
 * @namespace UiLayout
 * @brief Coordenadas únicas para botones y tarjetas interactivas (compartidas entre renderizado y touch).
 */
namespace UiLayout
{
    // --- Pantalla HOME ---
    constexpr UiRect HOME_BTN_TEST_BELT        = {   8,  48, 148, 56 };
    constexpr UiRect HOME_BTN_TEST_BUZZER      = { 164,  48, 148, 56 };
    constexpr UiRect HOME_BTN_TEST_TOWER_GREEN = {   8, 114, 148, 56 };
    constexpr UiRect HOME_BTN_TEST_TOWER_RED   = { 164, 114, 148, 56 };
    constexpr UiRect HOME_BANNER_ALARM_ACK     = {   6, 144, 308, 34 };

    // --- Pantallas AUTO 1 (Llenado) y AUTO 2 (Tapado) en TestMode ---
    constexpr UiRect AUTO_BTN_TEST_STOP_ARM    = {   8,  34, 148, 44 };
    constexpr UiRect AUTO_BTN_TEST_CLAMP_ARM   = { 164,  34, 148, 44 };
    constexpr UiRect AUTO_BTN_TEST_ACTUATOR    = {   8,  84, 304, 42 };
    constexpr UiRect AUTO_BTN_TEST_JOG_UP      = {   8, 132, 148, 48 };
    constexpr UiRect AUTO_BTN_TEST_JOG_DOWN    = { 164, 132, 148, 48 };

    // --- Pantalla SETTINGS ---
    constexpr UiRect SETTINGS_CARD_TEST_MODE   = {   6,  32, 152, 52 };
    constexpr UiRect SETTINGS_CARD_OVERLAY     = { 162,  32, 152, 52 };
    constexpr UiRect SETTINGS_BTN_PROFILE      = { 190,  90, 122, 30 };
    constexpr UiRect SETTINGS_BTN_JOG_FILL_UP  = {   8, 126, 148, 27 };
    constexpr UiRect SETTINGS_BTN_JOG_CAP_UP   = { 164, 126, 148, 27 };
    constexpr UiRect SETTINGS_BTN_RELEASE_ARMS = {   8, 157, 148, 27 };
    constexpr UiRect SETTINGS_BTN_LINE_CLEAR   = { 164, 157, 148, 27 };
}

/**
 * @namespace UiWidgets
 * @brief Primitivas de dibujo reutilizables sobre MCUFRIEND_kbv.
 */
namespace UiWidgets
{
    /**
     * @brief Dibuja un botón rectangular con etiqueta PROGMEM centrada.
     */
    inline void drawButtonCoords(
        MCUFRIEND_kbv& tft,
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        uint16_t fillColor,
        uint16_t borderColor,
        uint16_t textColor,
        const __FlashStringHelper* label,
        uint8_t textSize = 1,
        bool skipBgFillIfBlack = false)
    {
        if (!(skipBgFillIfBlack && (fillColor == UiTheme::COLOR_BG)))
        {
            tft.fillRect(x + 1, y + 1, w - 2, h - 2, fillColor);
        }
        tft.drawRect(x, y, w, h, borderColor);

        const char* progmemPtr = reinterpret_cast<const char*>(label);
        const size_t len = strlen_P(progmemPtr);
        const int16_t charW = static_cast<int16_t>(6 * textSize);
        const int16_t charH = static_cast<int16_t>(8 * textSize);
        const int16_t textW = static_cast<int16_t>(len) * charW;

        const int16_t cursorX = x + ((w > textW) ? ((w - textW) / 2) : 4);
        const int16_t cursorY = y + ((h - charH) / 2);

        tft.setTextSize(textSize);
        tft.setTextColor(textColor, fillColor);
        tft.setCursor(cursorX, cursorY);
        tft.print(label);
    }

    /**
     * @brief Wrapper inline que desempaqueta UiRect en registros (0 bytes en SRAM en AVR).
     */
    __attribute__((always_inline)) inline void drawButton(
        MCUFRIEND_kbv& tft,
        UiRect rect,
        uint16_t fillColor,
        uint16_t borderColor,
        uint16_t textColor,
        const __FlashStringHelper* label,
        uint8_t textSize = 1,
        bool skipBgFillIfBlack = false)
    {
        drawButtonCoords(
            tft, rect.x, rect.y, rect.w, rect.h,
            fillColor, borderColor, textColor, label, textSize, skipBgFillIfBlack);
    }

    /**
     * @brief Dibuja un indicador compacto [ON ] / [OFF] para sensores y entradas digitales.
     */
    inline void drawInputBadge(
        MCUFRIEND_kbv& tft,
        int16_t x,
        int16_t y,
        bool active,
        uint16_t activeColor = UiTheme::COLOR_GREEN_OK)
    {
        tft.setTextSize(1);
        tft.setTextColor(active ? activeColor : UiTheme::COLOR_DARK_GRAY, UiTheme::COLOR_BG);
        tft.setCursor(x, y);
        tft.print(active ? F("[ON ]") : F("[OFF]"));
    }
}
