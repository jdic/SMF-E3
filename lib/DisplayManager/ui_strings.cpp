/**
 * @file ui_strings.cpp
 * @brief Catálogo único de cadenas en memoria Flash (PROGMEM) y traductores de estados/errores.
 */

#include "ui_strings.hpp"
#include <avr/pgmspace.h>
#include "ui_theme.hpp"

namespace UiStrings
{
    namespace
    {
        // Modos globales de la estación
        const char STR_MODE_BOOT[]     PROGMEM = "MODO: ARRANQUE (HOMING)";
        const char STR_MODE_RUNNING[]  PROGMEM = "MODO: OPERANDO EN LINEA";
        const char STR_MODE_ERRSTOP[]  PROGMEM = "! PARO DE SEGURIDAD !  ";
        const char STR_MODE_RECOVERY[] PROGMEM = "MODO: RECUPERACION MAN.";
        const char STR_MODE_TEST[]     PROGMEM = "MODO: TEST MANUAL ACT. ";

        // Estados del Autómata de Llenado
        const char STR_FILL_IDLE[]     PROGMEM = "ESPERA BOTELLA ";
        const char STR_FILL_STOP_DN[]  PROGMEM = "TOPE ABAJO     ";
        const char STR_FILL_CLAMP_DN[] PROGMEM = "SUJETANDO      ";
        const char STR_FILL_LOWER_Z[]  PROGMEM = "BAJANDO BOQUILL";
        const char STR_FILL_PUMPING[]  PROGMEM = "BOMBEANDO      ";
        const char STR_FILL_DRAINING[] PROGMEM = "ESCURRIENDO    ";
        const char STR_FILL_RAISE_Z[]  PROGMEM = "SUBIENDO BOQUIL";
        const char STR_FILL_WAIT_Q[]   PROGMEM = "COLA LLENA     ";
        const char STR_FILL_RELEASE[]  PROGMEM = "LIBERANDO      ";

        // Estados del Autómata de Tapado
        const char STR_CAP_IDLE[]      PROGMEM = "ESPERA BOTELLA ";
        const char STR_CAP_STOP_DN[]   PROGMEM = "TOPE ABAJO     ";
        const char STR_CAP_CLAMP_DN[]  PROGMEM = "SUJETANDO      ";
        const char STR_CAP_LOWER_Z[]   PROGMEM = "BAJANDO CABEZAL";
        const char STR_CAP_TIGHTEN[]   PROGMEM = "APRETANDO TAPA ";
        const char STR_CAP_RAISE_Z[]   PROGMEM = "SUBIENDO CABEZA";
        const char STR_CAP_VISION[]    PROGMEM = "VERIFICANDO TAP";
        const char STR_CAP_RELEASE[]   PROGMEM = "LIBERANDO      ";

        // Perfiles de botella
        const char STR_PROF_SMALL[]    PROGMEM = "PEQUENA";
        const char STR_PROF_LARGE[]    PROGMEM = "GRANDE ";

        // Mensajes de diagnóstico y códigos de error (ERR-xxx)
        const char STR_ERR_NONE[]      PROGMEM = "OK: SISTEMA SIN ALARMAS   ";
        const char STR_ERR_101[]       PROGMEM = "E101: BOTELLA PERDIDA LLEN";
        const char STR_ERR_102[]       PROGMEM = "E102: BOTELLA PERDIDA TAP ";
        const char STR_ERR_201[]       PROGMEM = "E201: FALLO HOME Z LLENADO";
        const char STR_ERR_202[]       PROGMEM = "E202: LIMITE SOFT Z LLEN  ";
        const char STR_ERR_203[]       PROGMEM = "E203: TIMEOUT BAJADA LLEN ";
        const char STR_ERR_204[]       PROGMEM = "E204: TIMEOUT SUBIDA LLEN ";
        const char STR_ERR_211[]       PROGMEM = "E211: FALLO HOME Z TAPADO ";
        const char STR_ERR_212[]       PROGMEM = "E212: LIMITE SOFT Z TAPADO";
        const char STR_ERR_213[]       PROGMEM = "E213: TIMEOUT BAJADA TAP  ";
        const char STR_ERR_214[]       PROGMEM = "E214: TIMEOUT SUBIDA TAP  ";
        const char STR_ERR_301[]       PROGMEM = "E301: SOBRECORRIENTE TAP  ";
        const char STR_ERR_302[]       PROGMEM = "E302: ROSCA TRASROSCADA   ";
        const char STR_ERR_303[]       PROGMEM = "E303: ATASCO EN BANDA 24V ";
        const char STR_ERR_304[]       PROGMEM = "E304: BOTELLA SIN TAPA    ";
        const char STR_ERR_901[]       PROGMEM = "E901: PARO DE EMERGENCIA  ";
        const char STR_ERR_902[]       PROGMEM = "A902: SIN ENLACE SCADA    ";
    }

    const __FlashStringHelper* getModeProgmem(StationMode mode, uint16_t& outBgColor, uint16_t& outFgColor)
    {
        switch (mode)
        {
            case StationMode::Boot:
                outBgColor = UiTheme::COLOR_BLUE_HEADER;
                outFgColor = UiTheme::COLOR_CYAN;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_BOOT);

            case StationMode::Running:
                outBgColor = UiTheme::COLOR_GREEN_OK;
                outFgColor = UiTheme::COLOR_WHITE;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_RUNNING);

            case StationMode::ErrorStop:
                outBgColor = UiTheme::COLOR_RED_ALARM;
                outFgColor = UiTheme::COLOR_WHITE;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_ERRSTOP);

            case StationMode::TestMode:
                outBgColor = UiTheme::COLOR_ORANGE_REC;
                outFgColor = UiTheme::COLOR_BG;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_TEST);

            case StationMode::ManualRecovery:
            default:
                outBgColor = UiTheme::COLOR_ORANGE_REC;
                outFgColor = UiTheme::COLOR_BG;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_RECOVERY);
        }
    }

    const __FlashStringHelper* getFillStateProgmem(FillState state)
    {
        switch (state)
        {
            case FillState::Idle:             return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_IDLE);
            case FillState::StopArmDown:      return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_STOP_DN);
            case FillState::ClampArmDown:     return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_CLAMP_DN);
            case FillState::LoweringNozzle:   return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_LOWER_Z);
            case FillState::Filling:          return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_PUMPING);
            case FillState::Draining:         return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_DRAINING);
            case FillState::RaisingNozzle:    return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_RAISE_Z);
            case FillState::WaitingQueueSlot: return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_WAIT_Q);
            case FillState::ReleasingBottle:
            default:                          return reinterpret_cast<const __FlashStringHelper*>(STR_FILL_RELEASE);
        }
    }

    const __FlashStringHelper* getCapStateProgmem(CappingState state)
    {
        switch (state)
        {
            case CappingState::Idle:            return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_IDLE);
            case CappingState::StopArmDown:     return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_STOP_DN);
            case CappingState::ClampArmDown:    return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_CLAMP_DN);
            case CappingState::LoweringHead:    return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_LOWER_Z);
            case CappingState::TighteningCap:   return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_TIGHTEN);
            case CappingState::RaisingHead:     return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_RAISE_Z);
            case CappingState::VisionInspect:   return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_VISION);
            case CappingState::ReleasingBottle:
            default:                            return reinterpret_cast<const __FlashStringHelper*>(STR_CAP_RELEASE);
        }
    }

    const __FlashStringHelper* getProfileProgmem(BottleProfile profile)
    {
        return (profile == BottleProfile::Large)
                   ? reinterpret_cast<const __FlashStringHelper*>(STR_PROF_LARGE)
                   : reinterpret_cast<const __FlashStringHelper*>(STR_PROF_SMALL);
    }

    const __FlashStringHelper* getErrorProgmem(ErrorCode error, uint16_t& outBgColor, uint16_t& outFgColor)
    {
        if (error == ErrorCode::None)
        {
            outBgColor = UiTheme::COLOR_BG;
            outFgColor = UiTheme::COLOR_GREEN_OK;
            return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_NONE);
        }

        if (error == ErrorCode::NetworkLinkLost)
        {
            outBgColor = UiTheme::COLOR_BG;
            outFgColor = UiTheme::COLOR_YELLOW_WARN;
            return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_902);
        }

        outBgColor = UiTheme::COLOR_RED_ALARM;
        outFgColor = UiTheme::COLOR_WHITE;

        switch (error)
        {
            case ErrorCode::FillBottleLost:       return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_101);
            case ErrorCode::CapBottleLost:        return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_102);
            case ErrorCode::FillZHomeFailed:      return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_201);
            case ErrorCode::FillZSoftLimitHit:    return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_202);
            case ErrorCode::FillZLoweringTimeout: return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_203);
            case ErrorCode::FillZRaisingTimeout:  return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_204);
            case ErrorCode::CapZHomeFailed:       return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_211);
            case ErrorCode::CapZSoftLimitHit:     return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_212);
            case ErrorCode::CapZLoweringTimeout:  return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_213);
            case ErrorCode::CapZRaisingTimeout:   return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_214);
            case ErrorCode::ChuckOvercurrent:     return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_301);
            case ErrorCode::ChuckStallPremature:  return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_302);
            case ErrorCode::ConveyorStall:        return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_303);
            case ErrorCode::CapMissingVision:     return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_304);
            case ErrorCode::EmergencyStopPressed:
            default:                              return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_901);
        }
    }
}
