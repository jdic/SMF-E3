#include "display_manager.hpp"
#include <avr/pgmspace.h>

// Instancia llamada `tft` para que el código exportado de Lopaka.app
// compile directamente sin necesidad de renombrar `tft.`
static MCUFRIEND_kbv tft;

namespace
{
    // Paleta de colores RGB565 para interfaz industrial
    constexpr uint16_t COLOR_BG          = 0x0000U; // Negro
    constexpr uint16_t COLOR_WHITE       = 0xFFFFU; // Blanco
    constexpr uint16_t COLOR_LIGHT_GRAY  = 0xC618U; // Gris claro
    constexpr uint16_t COLOR_DARK_GRAY   = 0x3186U; // Gris oscuro (contornos/botones inactivos)
    constexpr uint16_t COLOR_PANEL_BG    = 0x10A2U; // Azul grisáceo muy oscuro para tarjetas
    constexpr uint16_t COLOR_CYAN        = 0x07FFU; // Cian (Modo Boot / Homing)
    constexpr uint16_t COLOR_BLUE_HEADER = 0x0255U; // Azul industrial (encabezado Boot / botones)
    constexpr uint16_t COLOR_GREEN_OK    = 0x05E0U; // Verde operativo (Modo Running)
    constexpr uint16_t COLOR_RED_ALARM   = 0xD800U; // Rojo alarma (Modo ErrorStop)
    constexpr uint16_t COLOR_ORANGE_REC  = 0xFC00U; // Ámbar/Naranja (Modo ManualRecovery)
    constexpr uint16_t COLOR_YELLOW_WARN = 0xFFE0U; // Amarillo (Advertencia ERR-902 / Rearme)

    // ========================================================================
    // CATÁLOGO DE CADENAS EN PROGMEM (0 BYTES DE CONSUMO EN SRAM)
    // ========================================================================

    // Modos globales de la estación
    const char STR_MODE_BOOT[]     PROGMEM = "MODO: ARRANQUE (HOMING)";
    const char STR_MODE_RUNNING[]  PROGMEM = "MODO: OPERANDO EN LINEA";
    const char STR_MODE_ERRSTOP[]  PROGMEM = "! PARO DE SEGURIDAD !  ";
    const char STR_MODE_RECOVERY[] PROGMEM = "MODO: RECUPERACION MAN.";

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
    const char STR_ERR_901[]       PROGMEM = "E901: PARO DE EMERGENCIA  ";
    const char STR_ERR_902[]       PROGMEM = "A902: SIN ENLACE SCADA    ";

    const __FlashStringHelper* getModeProgmem(StationMode mode, uint16_t& outBgColor, uint16_t& outFgColor)
    {
        switch (mode)
        {
            case StationMode::Boot:
                outBgColor = COLOR_BLUE_HEADER;
                outFgColor = COLOR_CYAN;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_BOOT);

            case StationMode::Running:
                outBgColor = COLOR_GREEN_OK;
                outFgColor = COLOR_WHITE;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_RUNNING);

            case StationMode::ErrorStop:
                outBgColor = COLOR_RED_ALARM;
                outFgColor = COLOR_WHITE;
                return reinterpret_cast<const __FlashStringHelper*>(STR_MODE_ERRSTOP);

            case StationMode::ManualRecovery:
            default:
                outBgColor = COLOR_ORANGE_REC;
                outFgColor = COLOR_BG;
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
            outBgColor = COLOR_PANEL_BG;
            outFgColor = COLOR_GREEN_OK;
            return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_NONE);
        }

        if (error == ErrorCode::NetworkLinkLost)
        {
            outBgColor = COLOR_PANEL_BG;
            outFgColor = COLOR_YELLOW_WARN;
            return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_902);
        }

        outBgColor = COLOR_RED_ALARM;
        outFgColor = COLOR_WHITE;

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
            case ErrorCode::EmergencyStopPressed:
            default:                              return reinterpret_cast<const __FlashStringHelper*>(STR_ERR_901);
        }
    }

    void drawLabeledButtonBox(
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        uint16_t fillColor,
        uint16_t borderColor,
        uint16_t textColor,
        const __FlashStringHelper* label,
        uint8_t textSize = 1)
    {
        tft.fillRoundRect(x, y, w, h, 4, fillColor);
        tft.drawRoundRect(x, y, w, h, 4, borderColor);

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
}

// ============================================================================
// RECURSOS PROGMEM Y LAYOUT COMPARTIDO (FIGMA layout_base.svg)
// ============================================================================

// Layout compartido: barra inferior de 5 pestañas presente en todas las Screens (Y = 190..239)
// Recreado con base en assets/ui/layout_base.svg y ui_icons.hpp
static void drawLayoutBottomBar(ScreenId activeScreen)
{
    // Limpia el área de la barra inferior (Y = 190..239, H = 50)
    tft.fillRect(0, DisplayManager::LAYOUT_BAR_Y, DisplayManager::SCREEN_WIDTH,
                 DisplayManager::SCREEN_HEIGHT - DisplayManager::LAYOUT_BAR_Y, COLOR_BG);

    // Contorno exterior: path d="M0.5 190.5H319.5V239.5H0.5V190.5Z" stroke="white"
    tft.drawRect(0, 190, 320, 50, COLOR_WHITE);

    // Separadores verticales:
    // path d="M64.5 190V240" stroke="white"
    tft.drawFastVLine(64, 190, 50, COLOR_WHITE);
    // path d="M128.5 190V240" stroke="white"
    tft.drawFastVLine(128, 190, 50, COLOR_WHITE);
    // path d="M192.5 190V240" stroke="white"
    tft.drawFastVLine(192, 190, 50, COLOR_WHITE);
    // path d="M254.5 190V240" stroke="white"
    tft.drawFastVLine(254, 190, 50, COLOR_WHITE);

    // Colores según pestaña activa (Cian activo, Blanco inactivo)
    const uint16_t colHome     = (activeScreen == ScreenId::Home)     ? COLOR_CYAN : COLOR_WHITE;
    const uint16_t colAutoFill = (activeScreen == ScreenId::AutoFill) ? COLOR_CYAN : COLOR_WHITE;
    const uint16_t colAutoCap  = (activeScreen == ScreenId::AutoCap)  ? COLOR_CYAN : COLOR_WHITE;
    const uint16_t colDebug    = (activeScreen == ScreenId::Debug)    ? COLOR_CYAN : COLOR_WHITE;
    const uint16_t colSettings = (activeScreen == ScreenId::Settings) ? COLOR_CYAN : COLOR_WHITE;

    // Resaltado de pestaña activa (línea inferior cian de 2 px de alto)
    if (activeScreen == ScreenId::Home)     { tft.fillRect(1,   237, 63, 2, COLOR_CYAN); }
    if (activeScreen == ScreenId::AutoFill) { tft.fillRect(65,  237, 63, 2, COLOR_CYAN); }
    if (activeScreen == ScreenId::AutoCap)  { tft.fillRect(129, 237, 63, 2, COLOR_CYAN); }
    if (activeScreen == ScreenId::Debug)    { tft.fillRect(193, 237, 61, 2, COLOR_CYAN); }
    if (activeScreen == ScreenId::Settings) { tft.fillRect(255, 237, 64, 2, COLOR_CYAN); }

    // Iconos de navegación (coordenadas exactas de layout_base.svg)
    // Tab 0: rect x="8" y="191" width="48" height="48" -> Home
    tft.drawBitmap(8, 191, UIIcons::ICON_HOME_48x48, 48, 48, colHome);

    // Tab 1: rect x="72" y="191" width="48" height="48" -> Drop (Auto 1)
    tft.drawBitmap(72, 191, UIIcons::ICON_FILL_48x48, 48, 48, colAutoFill);

    // Tab 2: rect x="136" y="191" width="48" height="48" -> Bottle Wine (Auto 2)
    tft.drawBitmap(136, 191, UIIcons::ICON_CAP_48x48, 48, 48, colAutoCap);

    // Tab 3: rect x="199" y="191" width="48" height="48" -> Bug 2 (Debug)
    tft.drawBitmap(199, 191, UIIcons::ICON_DEBUG_48x48, 48, 48, colDebug);

    // Tab 4: rect x="267" y="191" width="48" height="48" -> Settings Cog
    tft.drawBitmap(267, 191, UIIcons::ICON_SETTINGS_48x48, 48, 48, colSettings);
}

// Limpia únicamente la zona de contenido (Y = 0..189) para no parpadear la barra inferior
static void clearScreenContentArea(void)
{
    tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, DisplayManager::CONTENT_HEIGHT, COLOR_BG);
}

// ============================================================================
// IMPLEMENTACIÓN DE DisplayManager
// ============================================================================

DisplayTouchConfig DisplayManager::defaultTouchConfig()
{
    DisplayTouchConfig cfg =
    {
        HardwarePins::PIN_TOUCH_XP,
        HardwarePins::PIN_TOUCH_XM,
        HardwarePins::PIN_TOUCH_YP,
        HardwarePins::PIN_TOUCH_YM,
        ProcessParams::TS_MINX,
        ProcessParams::TS_MAXX,
        ProcessParams::TS_MINY,
        ProcessParams::TS_MAXY,
        ProcessParams::TS_MIN_PRESSURE,
        ProcessParams::TS_MAX_PRESSURE,
        ProcessParams::TOUCH_DEBUG_COORDS,
        ProcessParams::TOUCH_DEBUG_MARKER
    };
    return cfg;
}

DisplayManager::DisplayManager(const DisplayTouchConfig& config)
    : config_(config),
      ts_(config.xpPin, config.ypPin, config.xmPin, config.ymPin, TOUCH_RX_OHMS),
      currentScreen_(ScreenId::None),
      layoutDrawn_(false),
      contentFrameDrawn_(false),
      hasRenderedSnapshot_(false),
      renderedSnapshot_{
          ErrorCode::None,
          StationMode::Boot,
          FillState::Idle,
          CappingState::Idle,
          BottleProfile::Small,
          0,
          0},
      hasLastMarker_(false),
      lastMarkerX_(0),
      lastMarkerY_(0),
      btnNavHome_(),
      btnNavAutoFill_(),
      btnNavAutoCap_(),
      btnNavDebug_(),
      btnNavSettings_(),
      btnBottomAction_(),
      btnSetProfile_(),
      btnJogFillUp_(),
      btnJogCapUp_(),
      btnReleaseArms_(),
      btnConfirmClear_()
{
}

void DisplayManager::init()
{
    uint16_t id = tft.readID();
    if ((id == 0x0000U) || (id == 0xFFFFU) || (id == 0xD3D3U))
    {
        id = FALLBACK_TFT_ID;
    }

    tft.begin(id);
    tft.setRotation(LANDSCAPE_ROT);
    tft.setTextWrap(false);
    initLayoutButtons();

    currentScreen_       = ScreenId::None;
    layoutDrawn_         = false;
    contentFrameDrawn_   = false;
    hasRenderedSnapshot_ = false;
    hasLastMarker_       = false;
}

void DisplayManager::setDebugFlags(bool showCoords, bool showMarker)
{
    config_.debugShowCoords = showCoords;
    config_.debugShowMarker = showMarker;
}

void DisplayManager::showScreen(ScreenId screen, bool forceRedraw)
{
    if (!forceRedraw && (screen == currentScreen_))
    {
        return;
    }

    hasLastMarker_ = false;

    if (!layoutDrawn_ || forceRedraw || (screen != currentScreen_))
    {
        if (!layoutDrawn_ || forceRedraw)
        {
            tft.fillScreen(COLOR_BG);
        }
        else
        {
            clearScreenContentArea();
        }
        drawLayoutBottomBar(screen);
        layoutDrawn_ = true;
    }
    else
    {
        clearScreenContentArea();
    }

    currentScreen_     = screen;
    contentFrameDrawn_ = false;

    switch (screen)
    {
        case ScreenId::Home:
            drawHomeStaticFrame();
            contentFrameDrawn_ = true;
            if (hasRenderedSnapshot_)
            {
                renderHomeDynamic(renderedSnapshot_, true);
            }
            break;

        case ScreenId::AutoFill:
            drawAutoFillStaticFrame();
            contentFrameDrawn_ = true;
            if (hasRenderedSnapshot_)
            {
                renderAutoFillDynamic(renderedSnapshot_, true);
            }
            break;

        case ScreenId::AutoCap:
            drawAutoCapStaticFrame();
            contentFrameDrawn_ = true;
            if (hasRenderedSnapshot_)
            {
                renderAutoCapDynamic(renderedSnapshot_, true);
            }
            break;

        case ScreenId::Debug:
            drawDebugStaticFrame();
            contentFrameDrawn_ = true;
            if (hasRenderedSnapshot_)
            {
                renderDebugDynamic(renderedSnapshot_, true);
            }
            break;

        case ScreenId::Settings:
            drawSettingsStaticFrame();
            contentFrameDrawn_ = true;
            if (hasRenderedSnapshot_)
            {
                renderSettingsDynamic(renderedSnapshot_, true);
            }
            break;

        case ScreenId::None:
        default:
            currentScreen_ = ScreenId::None;
            break;
    }

    if (config_.debugShowCoords)
    {
        drawCalibrationTargets();
    }
}

void DisplayManager::render(const DisplaySnapshot& snapshot)
{
    if (currentScreen_ == ScreenId::None)
    {
        showScreen(ScreenId::Home);
    }

    // Cambiar automáticamente a Settings al entrar en ManualRecovery para presentar los mandos de despeje
    if (hasRenderedSnapshot_ &&
        (renderedSnapshot_.mode != snapshot.mode))
    {
        if (snapshot.mode == StationMode::ManualRecovery && currentScreen_ != ScreenId::Settings)
        {
            renderedSnapshot_    = snapshot;
            hasRenderedSnapshot_ = true;
            showScreen(ScreenId::Settings);
            return;
        }
        if (snapshot.mode == StationMode::Boot &&
            renderedSnapshot_.mode == StationMode::ManualRecovery &&
            currentScreen_ != ScreenId::Home)
        {
            renderedSnapshot_    = snapshot;
            hasRenderedSnapshot_ = true;
            showScreen(ScreenId::Home);
            return;
        }
    }

    const bool fullRedraw = !hasRenderedSnapshot_ || !contentFrameDrawn_;

    switch (currentScreen_)
    {
        case ScreenId::Home:
            renderHomeDynamic(snapshot, fullRedraw);
            break;

        case ScreenId::AutoFill:
            renderAutoFillDynamic(snapshot, fullRedraw);
            break;

        case ScreenId::AutoCap:
            renderAutoCapDynamic(snapshot, fullRedraw);
            break;

        case ScreenId::Debug:
            renderDebugDynamic(snapshot, fullRedraw);
            break;

        case ScreenId::Settings:
            renderSettingsDynamic(snapshot, fullRedraw);
            break;

        default:
            break;
    }

    renderedSnapshot_    = snapshot;
    hasRenderedSnapshot_ = true;
}

TouchCommand DisplayManager::pollTouchCommand()
{
    int16_t px = 0;
    int16_t py = 0;
    const bool touched = readTouchPixel(px, py);

    // 1. Pestañas de navegación fija en la barra inferior (layout_base.svg)
    btnNavHome_.press(touched && btnNavHome_.contains(px, py));
    btnNavAutoFill_.press(touched && btnNavAutoFill_.contains(px, py));
    btnNavAutoCap_.press(touched && btnNavAutoCap_.contains(px, py));
    btnNavDebug_.press(touched && btnNavDebug_.contains(px, py));
    btnNavSettings_.press(touched && btnNavSettings_.contains(px, py));

    if (btnNavHome_.justPressed())
    {
        showScreen(ScreenId::Home);
        return TouchCommand::None;
    }

    if (btnNavAutoFill_.justPressed())
    {
        showScreen(ScreenId::AutoFill);
        return TouchCommand::None;
    }

    if (btnNavAutoCap_.justPressed())
    {
        showScreen(ScreenId::AutoCap);
        return TouchCommand::None;
    }

    if (btnNavDebug_.justPressed())
    {
        showScreen(ScreenId::Debug);
        return TouchCommand::None;
    }

    if (btnNavSettings_.justPressed())
    {
        showScreen(ScreenId::Settings);
        return TouchCommand::None;
    }

    // 2. Botones exclusivos de la pantalla Settings (perfil y recuperación manual)
    if (currentScreen_ == ScreenId::Settings)
    {
        btnSetProfile_.press(touched && btnSetProfile_.contains(px, py));
        btnJogFillUp_.press(touched && btnJogFillUp_.contains(px, py));
        btnJogCapUp_.press(touched && btnJogCapUp_.contains(px, py));
        btnReleaseArms_.press(touched && btnReleaseArms_.contains(px, py));
        btnConfirmClear_.press(touched && btnConfirmClear_.contains(px, py));

        if (btnSetProfile_.justPressed())
        {
            return TouchCommand::CycleBottleProfile;
        }
        if (btnJogFillUp_.justPressed())
        {
            return TouchCommand::JogFillZUp;
        }
        if (btnJogCapUp_.justPressed())
        {
            return TouchCommand::JogCapZUp;
        }
        if (btnReleaseArms_.justPressed())
        {
            return TouchCommand::ReleaseArms;
        }
        if (btnConfirmClear_.justPressed())
        {
            return TouchCommand::ConfirmLineClear;
        }
    }
    else
    {
        btnSetProfile_.press(false);
        btnJogFillUp_.press(false);
        btnJogCapUp_.press(false);
        btnReleaseArms_.press(false);
        btnConfirmClear_.press(false);
    }

    return TouchCommand::None;
}

void DisplayManager::updateTouchNavigation()
{
    static_cast<void>(pollTouchCommand());
}

ScreenId DisplayManager::getCurrentScreen() const
{
    return currentScreen_;
}

bool DisplayManager::readTouchPixel(int16_t& px, int16_t& py)
{
    // Doble muestreo consecutivo para descartar lecturas inestables al apoyar o levantar el lápiz
    const TSPoint p1 = ts_.getPoint();
    const TSPoint p2 = ts_.getPoint();
    restoreSharedTftPins();

    if ((p1.z < config_.minPressure) || (p1.z > config_.minPressure && p1.z > config_.maxPressure) ||
        (p2.z < config_.minPressure) || (p2.z > config_.maxPressure))
    {
        return false;
    }

    const int16_t diffX = (p1.x > p2.x) ? (p1.x - p2.x) : (p2.x - p1.x);
    const int16_t diffY = (p1.y > p2.y) ? (p1.y - p2.y) : (p2.y - p1.y);
    if ((diffX > RAW_GLITCH_BAND) || (diffY > RAW_GLITCH_BAND))
    {
        return false;
    }

    const int16_t rawX = static_cast<int16_t>((p1.x + p2.x) / 2);
    const int16_t rawY = static_cast<int16_t>((p1.y + p2.y) / 2);
    const int16_t rawZ = static_cast<int16_t>((p1.z + p2.z) / 2);

    // En setRotation(1) de este shield:
    // - La coordenada horizontal px (0..319) proviene de rawY (945 en izq -> 101 en der)
    // - La coordenada vertical   py (0..239) proviene de rawX (925 arriba -> 105 abajo)
    const int16_t mappedX = static_cast<int16_t>(map(rawY, config_.tsMinX, config_.tsMaxX, 0, SCREEN_WIDTH - 1));
    const int16_t mappedY = static_cast<int16_t>(map(rawX, config_.tsMinY, config_.tsMaxY, 0, SCREEN_HEIGHT - 1));

    px = constrain(mappedX, 0, SCREEN_WIDTH - 1);
    py = constrain(mappedY, 0, SCREEN_HEIGHT - 1);

    updateDebugOverlay(rawX, rawY, rawZ, px, py);
    return true;
}

MCUFRIEND_kbv& DisplayManager::getTft()
{
    return tft;
}

void DisplayManager::initLayoutButtons()
{
    char emptyLabel[] = "";

    // Pestañas de la barra inferior (layout_base.svg)
    // Tab 0: HOME (X = 0..63, centro X = 32, Y = 215, W = 64, H = 50)
    btnNavHome_.initButton(&tft, 32, 215, 64, 50, 0, 0, 0, emptyLabel, 1);

    // Tab 1: AUTO 1 / LLENADO (X = 64..127, centro X = 96, Y = 215, W = 64, H = 50)
    btnNavAutoFill_.initButton(&tft, 96, 215, 64, 50, 0, 0, 0, emptyLabel, 1);

    // Tab 2: AUTO 2 / TAPADO (X = 128..191, centro X = 160, Y = 215, W = 64, H = 50)
    btnNavAutoCap_.initButton(&tft, 160, 215, 64, 50, 0, 0, 0, emptyLabel, 1);

    // Tab 3: DEBUG (X = 192..253, centro X = 223, Y = 215, W = 62, H = 50)
    btnNavDebug_.initButton(&tft, 223, 215, 62, 50, 0, 0, 0, emptyLabel, 1);

    // Tab 4: SETTINGS (X = 254..319, centro X = 287, Y = 215, W = 66, H = 50)
    btnNavSettings_.initButton(&tft, 287, 215, 66, 50, 0, 0, 0, emptyLabel, 1);

    // Botón contextual de alerta/rearme rápido
    btnBottomAction_.initButton(&tft, 160, 100, 200, 40, 0, 0, 0, emptyLabel, 1);

    // Botón de cambio de perfil en Settings: X = 190..312, Y = 34..68 (centro X = 251, Y = 51)
    btnSetProfile_.initButton(&tft, 251, 51, 122, 34, 0, 0, 0, emptyLabel, 1);

    // Cuadrícula de 4 botones de recuperación manual en Settings:
    // Fila 1 (Y = 92..130): [SUBIR Z LLEN] (X = 8..156) y [SUBIR Z TAP] (X = 164..312)
    btnJogFillUp_.initButton(&tft, 82, 111, 148, 38, 0, 0, 0, emptyLabel, 1);
    btnJogCapUp_.initButton(&tft, 238, 111, 148, 38, 0, 0, 0, emptyLabel, 1);

    // Fila 2 (Y = 136..174): [SOLTAR BRAZOS] (X = 8..156) y [LINEA DESPEJADA] (X = 164..312)
    btnReleaseArms_.initButton(&tft, 82, 155, 148, 38, 0, 0, 0, emptyLabel, 1);
    btnConfirmClear_.initButton(&tft, 238, 155, 148, 38, 0, 0, 0, emptyLabel, 1);
}

void DisplayManager::drawHomeStaticFrame()
{
    // Tarjeta Etapa 1: Llenado (Y = 34..68)
    tft.fillRoundRect(6, 34, 308, 34, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 34, 308, 34, 4, COLOR_DARK_GRAY);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 47);
    tft.print(F("LLENADO:"));

    // Tarjeta Etapa 2: Tapado (Y = 72..106)
    tft.fillRoundRect(6, 72, 308, 34, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 72, 308, 34, 4, COLOR_DARK_GRAY);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 85);
    tft.print(F("TAPADO :"));

    // Tarjeta de Contadores y Perfil (Y = 110..140)
    tft.fillRoundRect(6, 110, 308, 30, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 110, 308, 30, 4, COLOR_DARK_GRAY);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 121);
    tft.print(F("COLA:"));
    tft.setCursor(104, 121);
    tft.print(F("PERFIL:"));
    tft.setCursor(218, 121);
    tft.print(F("PROD:"));
}

void DisplayManager::drawSettingsStaticFrame()
{
    // Encabezado fijo de Settings y Asistencia Manual
    tft.fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE_HEADER);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_WHITE, COLOR_BLUE_HEADER);
    tft.setCursor(10, 6);
    tft.print(F("AJUSTES Y RECUPERACION"));

    // Tarjeta de formato de botella (Y = 34..68)
    tft.fillRoundRect(6, 34, 178, 34, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 34, 178, 34, 4, COLOR_DARK_GRAY);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(12, 47);
    tft.print(F("FORMATO:"));

    // Etiqueta de sección de mandos manuales
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_BG);
    tft.setCursor(8, 76);
    tft.print(F("MANDOS DE DESPEJE (SOLO EN MODO RECUPERACION):"));
}

void DisplayManager::renderHomeDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    // 1. Banner superior de Modo Operativo (Y = 0..28)
    if (fullRedraw || (snapshot.mode != renderedSnapshot_.mode))
    {
        uint16_t bgColor = COLOR_BLUE_HEADER;
        uint16_t fgColor = COLOR_WHITE;
        const __FlashStringHelper* modeStr = getModeProgmem(snapshot.mode, bgColor, fgColor);

        tft.fillRect(0, 0, SCREEN_WIDTH, 28, bgColor);
        tft.setTextSize(2);
        tft.setTextColor(fgColor, bgColor);
        tft.setCursor(10, 6);
        tft.print(modeStr);
    }

    // 2. Fase activa del Autómata de Llenado
    if (fullRedraw || (snapshot.fillState != renderedSnapshot_.fillState))
    {
        const uint16_t stateColor =
            (snapshot.fillState == FillState::Idle) ? COLOR_LIGHT_GRAY : COLOR_CYAN;
        tft.setTextSize(2);
        tft.setTextColor(stateColor, COLOR_PANEL_BG);
        tft.setCursor(74, 43);
        tft.print(getFillStateProgmem(snapshot.fillState));
    }

    // 3. Fase activa del Autómata de Tapado
    if (fullRedraw || (snapshot.capState != renderedSnapshot_.capState))
    {
        const uint16_t stateColor =
            (snapshot.capState == CappingState::Idle) ? COLOR_LIGHT_GRAY : COLOR_CYAN;
        tft.setTextSize(2);
        tft.setTextColor(stateColor, COLOR_PANEL_BG);
        tft.setCursor(74, 81);
        tft.print(getCapStateProgmem(snapshot.capState));
    }

    // 4. Cola intermedia (WIP Llenado -> Tapado)
    if (fullRedraw || (snapshot.queueCount != renderedSnapshot_.queueCount))
    {
        char qBuf[8];
        snprintf(qBuf, sizeof(qBuf), "%u/%u ", snapshot.queueCount, ProcessParams::MAX_INTERSTAGE_QUEUE);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_WHITE, COLOR_PANEL_BG);
        tft.setCursor(48, 121);
        tft.print(qBuf);
    }

    // 5. Perfil de botella activo
    if (fullRedraw || (snapshot.profile != renderedSnapshot_.profile))
    {
        tft.setTextSize(1);
        tft.setTextColor(COLOR_CYAN, COLOR_PANEL_BG);
        tft.setCursor(150, 121);
        tft.print(getProfileProgmem(snapshot.profile));
    }

    // 6. Conteo acumulado de producción
    if (fullRedraw || (snapshot.bottleCount != renderedSnapshot_.bottleCount))
    {
        char prodBuf[10];
        snprintf(prodBuf, sizeof(prodBuf), "%5lu", static_cast<unsigned long>(snapshot.bottleCount));
        tft.setTextSize(1);
        tft.setTextColor(COLOR_GREEN_OK, COLOR_PANEL_BG);
        tft.setCursor(252, 121);
        tft.print(prodBuf);
    }

    // 7. Banner inferior de diagnóstico / código de error (Y = 144..178)
    if (fullRedraw || (snapshot.error != renderedSnapshot_.error))
    {
        uint16_t errBg = COLOR_PANEL_BG;
        uint16_t errFg = COLOR_GREEN_OK;
        const __FlashStringHelper* errStr = getErrorProgmem(snapshot.error, errBg, errFg);

        tft.fillRoundRect(6, 144, 308, 34, 4, errBg);
        tft.drawRoundRect(6, 144, 308, 34, 4,
                          (snapshot.error == ErrorCode::None) ? COLOR_DARK_GRAY : COLOR_WHITE);
        tft.setTextSize(2);
        tft.setTextColor(errFg, errBg);
        tft.setCursor(12, 153);
        tft.print(errStr);
    }
}

void DisplayManager::renderSettingsDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    const bool canChangeProfile =
        (snapshot.mode == StationMode::Running) &&
        (snapshot.fillState == FillState::Idle) &&
        (snapshot.capState == CappingState::Idle) &&
        (snapshot.queueCount == 0U);

    const bool prevCanChangeProfile =
        (renderedSnapshot_.mode == StationMode::Running) &&
        (renderedSnapshot_.fillState == FillState::Idle) &&
        (renderedSnapshot_.capState == CappingState::Idle) &&
        (renderedSnapshot_.queueCount == 0U);

    // 1. Valor del perfil activo y botón [CAMBIAR PERFIL]
    if (fullRedraw ||
        (snapshot.profile != renderedSnapshot_.profile) ||
        (canChangeProfile != prevCanChangeProfile))
    {
        tft.setTextSize(2);
        tft.setTextColor(COLOR_CYAN, COLOR_PANEL_BG);
        tft.setCursor(66, 43);
        tft.print(getProfileProgmem(snapshot.profile));

        const uint16_t btnFill   = canChangeProfile ? COLOR_BLUE_HEADER : COLOR_PANEL_BG;
        const uint16_t btnBorder = canChangeProfile ? COLOR_CYAN : COLOR_DARK_GRAY;
        const uint16_t btnText   = canChangeProfile ? COLOR_WHITE : COLOR_DARK_GRAY;

        drawLabeledButtonBox(
            190, 34, 122, 34,
            btnFill, btnBorder, btnText,
            F("CAMBIAR PERFIL"), 1);
    }

    // 2. Cuadrícula de 4 botones de Recuperación Manual
    const bool inRecovery     = (snapshot.mode == StationMode::ManualRecovery);
    const bool prevInRecovery = (renderedSnapshot_.mode == StationMode::ManualRecovery);

    if (fullRedraw || (inRecovery != prevInRecovery))
    {
        const uint16_t jogFill   = inRecovery ? COLOR_BLUE_HEADER : COLOR_PANEL_BG;
        const uint16_t jogBorder = inRecovery ? COLOR_CYAN : COLOR_DARK_GRAY;
        const uint16_t jogText   = inRecovery ? COLOR_WHITE : COLOR_DARK_GRAY;

        const uint16_t clrFill   = inRecovery ? COLOR_GREEN_OK : COLOR_PANEL_BG;
        const uint16_t clrBorder = inRecovery ? COLOR_WHITE : COLOR_DARK_GRAY;
        const uint16_t clrText   = inRecovery ? COLOR_WHITE : COLOR_DARK_GRAY;

        drawLabeledButtonBox(
            8, 92, 148, 38,
            jogFill, jogBorder, jogText,
            F("SUBIR Z LLENADO"), 1);

        drawLabeledButtonBox(
            164, 92, 148, 38,
            jogFill, jogBorder, jogText,
            F("SUBIR Z TAPADO"), 1);

        drawLabeledButtonBox(
            8, 136, 148, 38,
            jogFill, jogBorder, jogText,
            F("SOLTAR BRAZOS"), 1);

        drawLabeledButtonBox(
            164, 136, 148, 38,
            clrFill, clrBorder, clrText,
            F("LINEA DESPEJADA"), 1);
    }
}

void DisplayManager::drawAutoFillStaticFrame()
{
    // Encabezado
    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_BG);
    tft.setCursor(8, 10);
    tft.print(F("AUT\x93MATA 1: LLENADO Y DOSIFICACI\x93N"));

    // Tarjeta Estado de la FSM (Y = 30..68)
    tft.fillRoundRect(6, 30, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 30, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 42);
    tft.print(F("FASE:"));

    // Tarjeta Sensores de Presencia (Y = 74..112)
    tft.fillRoundRect(6, 74, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 74, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 86);
    tft.print(F("APROX / RETENC:"));

    // Tarjeta Actuadores (Y = 118..156)
    tft.fillRoundRect(6, 118, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 118, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 130);
    tft.print(F("BOMBA / EJE Z1:"));
}

void DisplayManager::drawAutoCapStaticFrame()
{
    // Encabezado
    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_BG);
    tft.setCursor(8, 10);
    tft.print(F("AUT\x93MATA 2: TAPADO Y APRIETE"));

    // Tarjeta Estado de la FSM (Y = 30..68)
    tft.fillRoundRect(6, 30, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 30, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 42);
    tft.print(F("FASE:"));

    // Tarjeta Sensor de Presencia (Y = 74..112)
    tft.fillRoundRect(6, 74, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 74, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 86);
    tft.print(F("PRESENCIA TAPADO:"));

    // Tarjeta Motor de Apriete (Y = 118..156)
    tft.fillRoundRect(6, 118, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 118, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 130);
    tft.print(F("MOTOR APRIETE (IBT-2):"));
}

void DisplayManager::drawDebugStaticFrame()
{
    // Encabezado
    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_BG);
    tft.setCursor(8, 10);
    tft.print(F("DIAGN\x93STICO EN VIVO Y SE\x91""ALES I/O"));

    // Tarjeta Estado General y Error Activo (Y = 30..68)
    tft.fillRoundRect(6, 30, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 30, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 42);
    tft.print(F("MODO / ERROR:"));

    // Tarjeta Telemetría de Proceso (Y = 74..112)
    tft.fillRoundRect(6, 74, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 74, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 86);
    tft.print(F("BANDA / COLA / PROD:"));

    // Tarjeta Calibración Táctil (Y = 118..156)
    tft.fillRoundRect(6, 118, 308, 38, 4, COLOR_PANEL_BG);
    tft.drawRoundRect(6, 118, 308, 38, 4, COLOR_DARK_GRAY);
    tft.setTextColor(COLOR_LIGHT_GRAY, COLOR_PANEL_BG);
    tft.setCursor(14, 130);
    tft.print(F("CALIBRACI\x93N TOUCH:"));
}

void DisplayManager::renderAutoFillDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (fullRedraw || (snapshot.fillState != renderedSnapshot_.fillState))
    {
        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE, COLOR_PANEL_BG);
        tft.setCursor(60, 40);
        tft.print(getFillStateProgmem(snapshot.fillState));
    }
}

void DisplayManager::renderAutoCapDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (fullRedraw || (snapshot.capState != renderedSnapshot_.capState))
    {
        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE, COLOR_PANEL_BG);
        tft.setCursor(60, 40);
        tft.print(getCapStateProgmem(snapshot.capState));
    }
}

void DisplayManager::renderDebugDynamic(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    if (fullRedraw || (snapshot.mode != renderedSnapshot_.mode) || (snapshot.error != renderedSnapshot_.error))
    {
        tft.setTextSize(1);
        tft.setTextColor(COLOR_CYAN, COLOR_PANEL_BG);
        tft.setCursor(100, 42);
        tft.print(getModeProgmem(snapshot.mode));

        tft.setCursor(14, 54);
        if (snapshot.error != ErrorCode::None)
        {
            tft.setTextColor(COLOR_RED_ALARM, COLOR_PANEL_BG);
            tft.print(getErrorProgmem(snapshot.error));
        }
        else
        {
            tft.setTextColor(COLOR_GREEN_OK, COLOR_PANEL_BG);
            tft.print(F("SIN ALARMAS ACTIVAS (OK)"));
        }
    }

    if (fullRedraw || (snapshot.queueCount != renderedSnapshot_.queueCount) || (snapshot.bottleCount != renderedSnapshot_.bottleCount))
    {
        tft.setTextSize(1);
        tft.setTextColor(COLOR_WHITE, COLOR_PANEL_BG);
        tft.setCursor(150, 86);
        tft.print(F("COLA: "));
        tft.print(snapshot.queueCount);
        tft.print(F(" | TOT: "));
        tft.print(snapshot.bottleCount);
    }
}

void DisplayManager::renderBottomActionArea(const DisplaySnapshot& snapshot, bool fullRedraw)
{
    // En el nuevo layout de 5 pestañas (layout_base.svg), la barra inferior completa (Y = 190..239)
    // está dedicada a la navegación táctil. Las acciones contextuales se gestionan dentro de cada pantalla.
    (void)snapshot;
    (void)fullRedraw;
}

void DisplayManager::restoreSharedTftPins()
{
    // TouchScreen::getPoint() altera los pines compartidos XP (6), YM (7), YP (A1/LCD_WR) y XM (A2/LCD_RS).
    // Se restauran a OUTPUT inmediatamente y se deja WR/CS/RD en reposo (HIGH).
    pinMode(config_.xpPin, OUTPUT);
    pinMode(config_.ymPin, OUTPUT);
    pinMode(config_.xmPin, OUTPUT);
    pinMode(config_.ypPin, OUTPUT);

    digitalWrite(HardwarePins::PIN_TFT_WR, HIGH);
    digitalWrite(HardwarePins::PIN_TFT_RD, HIGH);
    digitalWrite(HardwarePins::PIN_TFT_CS, HIGH);
}

void DisplayManager::drawCalibrationTargets()
{
    // Dibuja 2 miras amarillas de referencia en (20, 20) y (300, 160) para verificar alineación
    constexpr uint16_t TARGET_COLOR = 0xFFE0;
    tft.drawFastHLine(14, 20, 13, TARGET_COLOR);
    tft.drawFastVLine(20, 14, 13, TARGET_COLOR);

    tft.drawFastHLine(294, 160, 13, TARGET_COLOR);
    tft.drawFastVLine(300, 154, 13, TARGET_COLOR);
}

void DisplayManager::updateDebugOverlay(int16_t rawX, int16_t rawY, int16_t rawZ, int16_t px, int16_t py)
{
    if (config_.debugShowMarker)
    {
        if (hasLastMarker_ && ((lastMarkerX_ != px) || (lastMarkerY_ != py)))
        {
            // Borrar únicamente el marcador anterior
            drawTouchMarker(lastMarkerX_, lastMarkerY_, COLOR_BG);

            // Si el marcador anterior tocó la barra inferior o las miras, restaurarlas
            if (lastMarkerY_ >= (LAYOUT_BAR_Y - MARKER_RADIUS - 1))
            {
                drawLayoutBottomBar(currentScreen_);
            }
            if (config_.debugShowCoords)
            {
                drawCalibrationTargets();
            }
        }

        drawTouchMarker(px, py, 0xF800); // Mira roja en el último punto tocado
        lastMarkerX_ = px;
        lastMarkerY_ = py;
        hasLastMarker_ = true;
    }

    if (config_.debugShowCoords)
    {
        char line1[32];
        char line2[32];
        snprintf(line1, sizeof(line1), "RAW X:%4d Y:%4d Z:%4d", rawX, rawY, rawZ);
        snprintf(line2, sizeof(line2), "PX  X:%4d Y:%4d       ", px, py);

        tft.setTextSize(1);
        tft.setTextColor(0x07E0, COLOR_BG);
        tft.setCursor(8, 120);
        tft.print(line1);
        tft.setCursor(8, 132);
        tft.print(line2);
    }
}

void DisplayManager::drawTouchMarker(int16_t x, int16_t y, uint16_t color)
{
    tft.drawCircle(x, y, MARKER_RADIUS, color);
    tft.drawFastHLine(x - MARKER_RADIUS, y, (MARKER_RADIUS * 2) + 1, color);
    tft.drawFastVLine(x, y - MARKER_RADIUS, (MARKER_RADIUS * 2) + 1, color);
}
