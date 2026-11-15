/**
 * @file ui_strings.hpp
 * @brief Declaraciones de traductores de estados, perfiles y códigos de error a texto PROGMEM y color.
 */

#pragma once
#include <Arduino.h>
#include "types/station_types.hpp"

/**
 * @namespace UiStrings
 * @brief Selectores de texto en memoria Flash (PROGMEM) y formato cromático para la interfaz TFT.
 */
namespace UiStrings
{
    const __FlashStringHelper* getModeProgmem(StationMode mode, uint16_t& outBgColor, uint16_t& outFgColor);
    const __FlashStringHelper* getFillStateProgmem(FillState state);
    const __FlashStringHelper* getCapStateProgmem(CappingState state);
    const __FlashStringHelper* getProfileProgmem(BottleProfile profile);
    const __FlashStringHelper* getErrorProgmem(ErrorCode error, uint16_t& outBgColor, uint16_t& outFgColor);
}
