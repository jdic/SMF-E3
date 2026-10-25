#include "display_manager.hpp"

// Instancia llamada `tft` para que el código exportado de Lopaka.app
// compile directamente sin necesidad de renombrar `tft.`
static MCUFRIEND_kbv tft;

// ============================================================================
// RECURSOS PROGMEM Y LAYOUT COMPARTIDO (LOPAKA.APP)
// ============================================================================

// [BEGIN lopaka generated - Bitmaps]
static const unsigned char PROGMEM image_menu_home_bits[] =
{
    0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,
    0xe0,0xe7,0x00,0x00,0x00,0xe0,0xe7,0x00,0x00,0x00,0xff,0x00,0xc0,0x00,0x00,0xff,
    0x00,0xe0,0x00,0x00,0xff,0x00,0xe0,0x00,0x00,0xf8,0x18,0x18,0x00,0x00,0xf8,0x18,
    0x18,0x00,0x00,0xf8,0x18,0x18,0x00,0x00,0xe0,0xe7,0x07,0x00,0x00,0xe0,0xe7,0x07,
    0x00,0x03,0x03,0x00,0xc0,0xc0,0x07,0x07,0x00,0xe0,0xe0,0x07,0x07,0x00,0xe0,0xe0,
    0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0xe0,
    0xe0,0x00,0x07,0x07,0xe0,0xe0,0x00,0x07,0x07,0xff,0xc3,0xc0,0x03,0xff,0x1f,0x07,
    0xe0,0x00,0xf8,0x1f,0x07,0xe0,0x00,0xf8,0x07,0x07,0xe0,0x00,0xe0,0x07,0x07,0xe0,
    0x00,0xe0,0x07,0x07,0xe0,0x00,0xe0,0x07,0x00,0x07,0xf8,0xe0,0x07,0x00,0x07,0xf8,
    0xe0,0x07,0x00,0x07,0xf8,0xe0,0x07,0x00,0x07,0x18,0xe0,0x07,0x00,0x07,0x18,0xe0,
    0x07,0x00,0x07,0xf8,0xe0,0x07,0x00,0x07,0xf8,0xe0,0x07,0x00,0x07,0xf8,0xe0,0x07,
    0x00,0x07,0x18,0xe0,0x07,0x00,0x07,0x18,0xe0,0x07,0x00,0x07,0x3c,0xe0,0x07,0xff,
    0xff,0xff,0xe0,0x07,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const unsigned char PROGMEM image_menu_settings_gear_bits[] =
{
    0x00,0x01,0xff,0x80,0x00,0x00,0x01,0xff,0x80,0x00,0x00,0xc1,0xff,0x83,0x00,0x01,
    0xc1,0xc3,0x83,0x80,0x01,0xc1,0xc3,0x83,0x80,0x07,0x3e,0x00,0x7c,0xe0,0x07,0x3e,
    0x00,0x7c,0xe0,0x1e,0x3e,0x00,0x7c,0x78,0x38,0x00,0x00,0x00,0x1c,0x38,0x00,0x00,
    0x00,0x1c,0x07,0x01,0xff,0x80,0xe0,0x07,0x01,0xff,0x80,0xe0,0x07,0x07,0xff,0xe0,
    0xe0,0x07,0x0e,0x00,0x70,0xe0,0x07,0x0e,0x00,0x70,0xe0,0xf8,0x38,0x00,0x1c,0x1f,
    0xf8,0x38,0x00,0x1c,0x1f,0xf8,0x38,0x00,0x1c,0x1f,0xe0,0x38,0x00,0x1c,0x07,0xe0,
    0x38,0x00,0x1c,0x07,0xe0,0x38,0x00,0x1c,0x07,0xe0,0x38,0x00,0x1c,0x07,0xf8,0x38,
    0x00,0x1c,0x1f,0xf8,0x38,0x00,0x1c,0x1f,0xf8,0x38,0x00,0x1c,0x1f,0x07,0x0e,0x00,
    0x70,0xe0,0x07,0x0e,0x00,0x70,0xe0,0x07,0x07,0xff,0xe0,0xe0,0x07,0x01,0xff,0x80,
    0xe0,0x07,0x01,0xff,0x80,0xe0,0x38,0x00,0x00,0x00,0x1c,0x38,0x00,0x00,0x00,0x1c,
    0x1e,0x3e,0x00,0x7c,0x78,0x07,0x3e,0x00,0x7c,0xe0,0x07,0x3e,0x00,0x7c,0xe0,0x01,
    0xc1,0xc3,0x83,0x80,0x01,0xc1,0xc3,0x83,0x80,0x00,0xc1,0xff,0x83,0x00,0x00,0x01,
    0xff,0x80,0x00,0x00,0x01,0xff,0x80,0x00
};
// [END lopaka generated - Bitmaps]

// Layout compartido: barra inferior presente en todas las Screens (Y = 183..239)
static void drawLayoutBottomBar(void)
{
    tft.fillRect(0, DisplayManager::LAYOUT_BAR_Y, DisplayManager::SCREEN_WIDTH,
                 DisplayManager::SCREEN_HEIGHT - DisplayManager::LAYOUT_BAR_Y, 0x0000);
    // menu_home
    tft.drawBitmap(6, 190, image_menu_home_bits, 40, 43, 0xFFFF);
    // menu_settings_gear
    tft.drawBitmap(70, 190, image_menu_settings_gear_bits, 40, 40, 0xFFFF);
    // rect 3
    tft.drawRect(0, 183, 320, 57, 0xFFFF);
    // line 4
    tft.drawLine(56, 183, 56, 239, 0xFFFF);
}

// Limpia únicamente la zona de contenido (Y = 0..182) para no parpadear la barra inferior
static void clearScreenContentArea(void)
{
    tft.fillRect(0, 0, DisplayManager::SCREEN_WIDTH, DisplayManager::CONTENT_HEIGHT, 0x0000);
}

// ============================================================================
// CONTENIDO DE CADA SCREEN (LOPAKA.APP)
// ============================================================================

// [BEGIN lopaka generated - Screen Home]
static void drawContent_Home(void)
{
    // string 5
    tft.setTextColor(0xFFFF);
    tft.setTextSize(4);
    tft.setTextWrap(false);
    tft.setCursor(16, 14);
    tft.print("HOME");
}
// [END lopaka generated - Screen Home]

// [BEGIN lopaka generated - Screen Settings]
static void drawContent_Settings(void)
{
    // string 5
    tft.setTextColor(0xFFFF);
    tft.setTextSize(4);
    tft.setTextWrap(false);
    tft.setCursor(16, 14);
    tft.print("SETTINGS");
}
// [END lopaka generated - Screen Settings]

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
      hasLastMarker_(false),
      lastMarkerX_(0),
      lastMarkerY_(0),
      btnNavHome_(),
      btnNavSettings_()
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
    initLayoutButtons();

    currentScreen_ = ScreenId::None;
    layoutDrawn_ = false;
    hasLastMarker_ = false;
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

    if (!layoutDrawn_ || forceRedraw)
    {
        tft.fillScreen(0x0000);
        drawLayoutBottomBar();
        layoutDrawn_ = true;
    }
    else
    {
        clearScreenContentArea();
    }

    switch (screen)
    {
        case ScreenId::Home:
            drawContent_Home();
            currentScreen_ = ScreenId::Home;
            break;

        case ScreenId::Settings:
            drawContent_Settings();
            currentScreen_ = ScreenId::Settings;
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

void DisplayManager::updateTouchNavigation()
{
    int16_t px = 0;
    int16_t py = 0;
    const bool touched = readTouchPixel(px, py);

    btnNavHome_.press(touched && btnNavHome_.contains(px, py));
    btnNavSettings_.press(touched && btnNavSettings_.contains(px, py));

    if (btnNavHome_.justPressed())
    {
        showScreen(ScreenId::Home);
    }
    else if (btnNavSettings_.justPressed())
    {
        showScreen(ScreenId::Settings);
    }
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

    // Área táctil del icono Home: X = 0..56, Y = 183..239 (centro X = 28, centro Y = 211)
    btnNavHome_.initButton(&tft, 28, 211, 56, 57, 0, 0, 0, emptyLabel, 1);

    // Área táctil del icono Settings: X = 57..120, Y = 183..239 (centro X = 88, centro Y = 211)
    btnNavSettings_.initButton(&tft, 88, 211, 64, 57, 0, 0, 0, emptyLabel, 1);
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
            drawTouchMarker(lastMarkerX_, lastMarkerY_, 0x0000);

            // Si el marcador anterior tocó la barra inferior o las miras, restaurarlas
            if (lastMarkerY_ >= (LAYOUT_BAR_Y - MARKER_RADIUS - 1))
            {
                drawLayoutBottomBar();
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
        tft.setTextColor(0x07E0, 0x0000);
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
