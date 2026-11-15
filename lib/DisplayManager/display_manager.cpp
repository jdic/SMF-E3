/**
 * @file display_manager.cpp
 * @brief Núcleo del controlador HMI: inicialización TFT, barra de navegación, lectura táctil y despacho de pantallas.
 */

#include "display_manager.hpp"
#include "ui_theme.hpp"
#include "ui_strings.hpp"

void DisplayManager::drawSingleNavTab(ScreenId tab, bool active)
{
    const uint16_t iconCol = active ? UiTheme::COLOR_CYAN : UiTheme::COLOR_WHITE;
    const uint16_t lineCol = active ? UiTheme::COLOR_CYAN : UiTheme::COLOR_BG;

    switch (tab)
    {
        case ScreenId::Home:
            tft_.drawBitmap(8, 191, UIIcons::ICON_HOME_48x48, 48, 48, iconCol);
            tft_.fillRect(1, 237, 63, 2, lineCol);
            break;

        case ScreenId::AutoFill:
            tft_.drawBitmap(72, 191, UIIcons::ICON_FILL_48x48, 48, 48, iconCol);
            tft_.fillRect(65, 237, 63, 2, lineCol);
            break;

        case ScreenId::AutoCap:
            tft_.drawBitmap(136, 191, UIIcons::ICON_CAP_48x48, 48, 48, iconCol);
            tft_.fillRect(129, 237, 63, 2, lineCol);
            break;

        case ScreenId::Debug:
            tft_.drawBitmap(199, 191, UIIcons::ICON_DEBUG_48x48, 48, 48, iconCol);
            tft_.fillRect(193, 237, 61, 2, lineCol);
            break;

        case ScreenId::Settings:
            tft_.drawBitmap(267, 191, UIIcons::ICON_SETTINGS_48x48, 48, 48, iconCol);
            tft_.fillRect(255, 237, 64, 2, lineCol);
            break;

        case ScreenId::None:
        default:
            break;
    }
}

void DisplayManager::drawLayoutBottomBar(ScreenId activeScreen)
{
    tft_.fillRect(0, LAYOUT_BAR_Y, SCREEN_WIDTH, SCREEN_HEIGHT - LAYOUT_BAR_Y, UiTheme::COLOR_BG);

    tft_.drawRect(0, 190, 320, 50, UiTheme::COLOR_WHITE);
    tft_.drawFastVLine(64, 190, 50, UiTheme::COLOR_WHITE);
    tft_.drawFastVLine(128, 190, 50, UiTheme::COLOR_WHITE);
    tft_.drawFastVLine(192, 190, 50, UiTheme::COLOR_WHITE);
    tft_.drawFastVLine(254, 190, 50, UiTheme::COLOR_WHITE);

    drawSingleNavTab(ScreenId::Home,     activeScreen == ScreenId::Home);
    drawSingleNavTab(ScreenId::AutoFill, activeScreen == ScreenId::AutoFill);
    drawSingleNavTab(ScreenId::AutoCap,  activeScreen == ScreenId::AutoCap);
    drawSingleNavTab(ScreenId::Debug,    activeScreen == ScreenId::Debug);
    drawSingleNavTab(ScreenId::Settings, activeScreen == ScreenId::Settings);
}

void DisplayManager::clearScreenContentArea(int16_t startY)
{
    tft_.fillRect(0, startY, SCREEN_WIDTH, CONTENT_HEIGHT - startY, UiTheme::COLOR_BG);
}

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
    : tft_(),
      config_(config),
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
          0,
          0U,
          0,
          0,
          0U,
          0U,
          0U,
          0U,
          0U},
      hasLastMarker_(false),
      lastMarkerX_(0),
      lastMarkerY_(0),
      touchHeld_(false),
      touchReleaseCount_(0U),
      lastTouchPx_(0),
      lastTouchPy_(0),
      lastCommandMs_(0UL),
      showPowerOverlay_(false)
{
}

void DisplayManager::init()
{
    uint16_t id = tft_.readID();
    if ((id == 0x0000U) || (id == 0xFFFFU) || (id == 0xD3D3U))
    {
        id = FALLBACK_TFT_ID;
    }

    tft_.begin(id);
    tft_.setRotation(LANDSCAPE_ROT);
    tft_.setTextWrap(false);

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
    const ScreenId prevScreen = currentScreen_;

    const bool hasFullTopBanner =
        (screen == ScreenId::Home) ||
        (screen == ScreenId::Settings) ||
        ((screen == ScreenId::AutoFill || screen == ScreenId::AutoCap) &&
          hasRenderedSnapshot_ &&
          (renderedSnapshot_.mode == StationMode::TestMode));

    const int16_t clearStartY = hasFullTopBanner ? 28 : 0;

    if (!layoutDrawn_)
    {
        tft_.fillScreen(UiTheme::COLOR_BG);
        drawLayoutBottomBar(screen);
        layoutDrawn_ = true;
    }
    else
    {
        clearScreenContentArea(clearStartY);
        if (screen != prevScreen)
        {
            drawSingleNavTab(prevScreen, false);
            drawSingleNavTab(screen, true);
        }
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

    if (hasRenderedSnapshot_ && showPowerOverlay_)
    {
        renderPowerOverlay(renderedSnapshot_, true);
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

    // Cambiar o redibujar el marco estático cuando entra/sale de TestMode o ManualRecovery
    if (hasRenderedSnapshot_ && (renderedSnapshot_.mode != snapshot.mode))
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

        if ((snapshot.mode == StationMode::TestMode || renderedSnapshot_.mode == StationMode::TestMode) &&
            (currentScreen_ == ScreenId::Home || currentScreen_ == ScreenId::AutoFill || currentScreen_ == ScreenId::AutoCap))
        {
            renderedSnapshot_    = snapshot;
            hasRenderedSnapshot_ = true;
            showScreen(currentScreen_, true);
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

    if (showPowerOverlay_)
    {
        renderPowerOverlay(snapshot, fullRedraw);
    }

    renderedSnapshot_    = snapshot;
    hasRenderedSnapshot_ = true;
}

TouchCommand DisplayManager::pollTouchCommand()
{
    int16_t rawPx = 0;
    int16_t rawPy = 0;
    const bool rawTouched   = readTouchPixel(rawPx, rawPy);
    const bool wasTouchHeld = touchHeld_;

    // Filtro de retención táctil: evita falsos sueltes por ruido eléctrico
    if (rawTouched)
    {
        touchHeld_         = true;
        touchReleaseCount_ = 0U;
        lastTouchPx_       = rawPx;
        lastTouchPy_       = rawPy;
    }
    else if (touchHeld_)
    {
        touchReleaseCount_++;
        if (touchReleaseCount_ >= TOUCH_RELEASE_TICKS)
        {
            touchHeld_         = false;
            touchReleaseCount_ = 0U;
        }
    }

    const bool justTouched = (!wasTouchHeld && touchHeld_);
    if (!justTouched)
    {
        return TouchCommand::None;
    }

    const int16_t px = lastTouchPx_;
    const int16_t py = lastTouchPy_;

    // 1. Pestañas de navegación fija en la barra inferior (Y = 190..239)
    if (py >= LAYOUT_BAR_Y)
    {
        if (px < 64)       { showScreen(ScreenId::Home); }
        else if (px < 128) { showScreen(ScreenId::AutoFill); }
        else if (px < 192) { showScreen(ScreenId::AutoCap); }
        else if (px < 254) { showScreen(ScreenId::Debug); }
        else               { showScreen(ScreenId::Settings); }
        return TouchCommand::None;
    }

    const bool isTestMode = hasRenderedSnapshot_ && (renderedSnapshot_.mode == StationMode::TestMode);
    TouchCommand cmd = TouchCommand::None;

    // 2. Zonas táctiles de la pantalla Settings
    if (currentScreen_ == ScreenId::Settings)
    {
        if (inRect(px, py, UiLayout::SETTINGS_CARD_OVERLAY))
        {
            const uint32_t nowMs = millis();
            if ((nowMs - lastCommandMs_) >= TOUCH_COOLDOWN_MS)
            {
                lastCommandMs_    = nowMs;
                showPowerOverlay_ = !showPowerOverlay_;
                tft_.fillRect(166, 34, 48, 48, UiTheme::COLOR_BG);
                if (showPowerOverlay_)
                {
                    tft_.drawBitmap(166, 34, UIIcons::ICON_CHECKBOX_ON_48x48, 48, 48, UiTheme::COLOR_YELLOW_WARN);
                }
                else
                {
                    tft_.drawBitmap(166, 34, UIIcons::ICON_CHECKBOX_OFF_48x48, 48, 48, UiTheme::COLOR_WHITE);
                }
            }
            return TouchCommand::None;
        }

        if (inRect(px, py, UiLayout::SETTINGS_CARD_TEST_MODE))        { cmd = TouchCommand::ToggleTestMode; }
        else if (inRect(px, py, UiLayout::SETTINGS_BTN_PROFILE))      { cmd = TouchCommand::CycleBottleProfile; }
        else if (inRect(px, py, UiLayout::SETTINGS_BTN_JOG_FILL_UP))  { cmd = TouchCommand::JogFillZUp; }
        else if (inRect(px, py, UiLayout::SETTINGS_BTN_JOG_CAP_UP))   { cmd = TouchCommand::JogCapZUp; }
        else if (inRect(px, py, UiLayout::SETTINGS_BTN_RELEASE_ARMS)) { cmd = TouchCommand::ReleaseArms; }
        else if (inRect(px, py, UiLayout::SETTINGS_BTN_LINE_CLEAR))   { cmd = TouchCommand::ConfirmLineClear; }
    }
    // 3. Zonas táctiles de la pantalla Home
    else if (currentScreen_ == ScreenId::Home)
    {
        if (isTestMode)
        {
            if (inRect(px, py, UiLayout::HOME_BTN_TEST_BELT))             { cmd = TouchCommand::TestToggleBelt; }
            else if (inRect(px, py, UiLayout::HOME_BTN_TEST_BUZZER))      { cmd = TouchCommand::TestToggleBuzzer; }
            else if (inRect(px, py, UiLayout::HOME_BTN_TEST_TOWER_GREEN)) { cmd = TouchCommand::TestToggleTowerGreen; }
            else if (inRect(px, py, UiLayout::HOME_BTN_TEST_TOWER_RED))   { cmd = TouchCommand::TestToggleTowerRed; }
        }
        else if (hasRenderedSnapshot_ &&
                 (renderedSnapshot_.mode == StationMode::ErrorStop) &&
                 inRect(px, py, UiLayout::HOME_BANNER_ALARM_ACK))
        {
            cmd = TouchCommand::AcknowledgeAlarm;
        }
    }
    // 4. Zonas táctiles de Autómata 1 y Autómata 2 en TestMode
    else if ((currentScreen_ == ScreenId::AutoFill || currentScreen_ == ScreenId::AutoCap) && isTestMode)
    {
        const bool isFill = (currentScreen_ == ScreenId::AutoFill);
        if (inRect(px, py, UiLayout::AUTO_BTN_TEST_STOP_ARM))
        {
            cmd = isFill ? TouchCommand::TestToggleFillStopArm : TouchCommand::TestToggleCapStopArm;
        }
        else if (inRect(px, py, UiLayout::AUTO_BTN_TEST_CLAMP_ARM))
        {
            cmd = isFill ? TouchCommand::TestToggleFillClampArm : TouchCommand::TestToggleCapClampArm;
        }
        else if (inRect(px, py, UiLayout::AUTO_BTN_TEST_ACTUATOR))
        {
            cmd = isFill ? TouchCommand::TestToggleFillPump : TouchCommand::TestToggleChuckMotor;
        }
        else if (inRect(px, py, UiLayout::AUTO_BTN_TEST_JOG_UP))
        {
            cmd = isFill ? TouchCommand::JogFillZUp : TouchCommand::JogCapZUp;
        }
        else if (inRect(px, py, UiLayout::AUTO_BTN_TEST_JOG_DOWN))
        {
            cmd = isFill ? TouchCommand::TestJogFillZDown : TouchCommand::TestJogCapZDown;
        }
    }

    if (cmd != TouchCommand::None)
    {
        const uint32_t nowMs = millis();
        if ((nowMs - lastCommandMs_) < TOUCH_COOLDOWN_MS)
        {
            return TouchCommand::None;
        }
        lastCommandMs_ = nowMs;
    }

    return cmd;
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
    const TSPoint p1 = ts_.getPoint();
    const TSPoint p2 = ts_.getPoint();
    restoreSharedTftPins();

    if ((p1.z < config_.minPressure) || (p1.z > config_.maxPressure) ||
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

    const int16_t mappedX = static_cast<int16_t>(map(rawY, config_.tsMinX, config_.tsMaxX, 0, SCREEN_WIDTH - 1));
    const int16_t mappedY = static_cast<int16_t>(map(rawX, config_.tsMinY, config_.tsMaxY, 0, SCREEN_HEIGHT - 1));

    px = constrain(mappedX, 0, SCREEN_WIDTH - 1);
    py = constrain(mappedY, 0, SCREEN_HEIGHT - 1);

    updateDebugOverlay(rawX, rawY, rawZ, px, py);
    return true;
}

MCUFRIEND_kbv& DisplayManager::getTft()
{
    return tft_;
}

void DisplayManager::restoreSharedTftPins()
{
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
    constexpr uint16_t TARGET_COLOR = 0xFFE0;
    tft_.drawFastHLine(14, 20, 13, TARGET_COLOR);
    tft_.drawFastVLine(20, 14, 13, TARGET_COLOR);

    tft_.drawFastHLine(294, 160, 13, TARGET_COLOR);
    tft_.drawFastVLine(300, 154, 13, TARGET_COLOR);
}

void DisplayManager::updateDebugOverlay(int16_t rawX, int16_t rawY, int16_t rawZ, int16_t px, int16_t py)
{
    if (config_.debugShowMarker)
    {
        if (hasLastMarker_ && ((lastMarkerX_ != px) || (lastMarkerY_ != py)))
        {
            drawTouchMarker(lastMarkerX_, lastMarkerY_, UiTheme::COLOR_BG);

            if (lastMarkerY_ >= (LAYOUT_BAR_Y - MARKER_RADIUS - 1))
            {
                drawLayoutBottomBar(currentScreen_);
            }
            if (config_.debugShowCoords)
            {
                drawCalibrationTargets();
            }
        }

        drawTouchMarker(px, py, 0xF800);
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

        tft_.setTextSize(1);
        tft_.setTextColor(0x07E0, UiTheme::COLOR_BG);
        tft_.setCursor(8, 120);
        tft_.print(line1);
        tft_.setCursor(8, 132);
        tft_.print(line2);
    }
}

void DisplayManager::drawTouchMarker(int16_t x, int16_t y, uint16_t color)
{
    tft_.drawCircle(x, y, MARKER_RADIUS, color);
    tft_.drawFastHLine(x - MARKER_RADIUS, y, (MARKER_RADIUS * 2) + 1, color);
    tft_.drawFastVLine(x, y - MARKER_RADIUS, (MARKER_RADIUS * 2) + 1, color);
}
