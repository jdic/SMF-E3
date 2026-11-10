/**
 * @file display_manager.hpp
 * @brief Controlador HMI para el shield TFT 2.4" ILI9341 (320x240) con barra de navegación Lopaka y renderizado incremental.
 */

#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include "config/hardware_pins.hpp"
#include "config/process_params.hpp"
#include "types/station_types.hpp"
#include "ui_icons.hpp"

/**
 * @enum ScreenId
 * @brief Identificadores de las pantallas de contenido que se renderizan sobre el Layout base.
 */
enum class ScreenId : uint8_t
{
    None = 0, ///< Sin pantalla activa
    Home,     ///< Pantalla principal de supervisión de proceso ("HOME")
    AutoFill, ///< Pantalla de supervisión y control del Autómata 1 ("AUTO 1" - Llenado)
    AutoCap,  ///< Pantalla de supervisión y control del Autómata 2 ("AUTO 2" - Tapado)
    Debug,    ///< Pantalla de diagnóstico en vivo e I/O ("DEBUG")
    Settings  ///< Pantalla de perfil de botella y recuperación manual ("SETTINGS")
};

/**
 * @struct DisplayTouchConfig
 * @brief Pines compartidos del panel resistivo, parámetros de calibración y banderas de depuración táctil.
 */
struct DisplayTouchConfig
{
    uint8_t xpPin;           ///< Terminal digital X+ del panel táctil
    uint8_t xmPin;           ///< Terminal analógico X- compartido con bus de control
    uint8_t ypPin;           ///< Terminal analógico Y+ compartido con bus de control
    uint8_t ymPin;           ///< Terminal digital Y- del panel táctil
    int16_t tsMinX;          ///< Lectura ADC en borde izquierdo (px = 0) [0..1023]
    int16_t tsMaxX;          ///< Lectura ADC en borde derecho (px = 319) [0..1023]
    int16_t tsMinY;          ///< Lectura ADC en borde superior (py = 0) [0..1023]
    int16_t tsMaxY;          ///< Lectura ADC en borde inferior (py = 239) [0..1023]
    int16_t minPressure;     ///< Presión mínima válida Z [cuentas]
    int16_t maxPressure;     ///< Presión máxima válida Z [cuentas]
    bool    debugShowCoords; ///< Flag: muestra en pantalla las coordenadas RAW(x,y,z) y PX(x,y)
    bool    debugShowMarker; ///< Flag: muestra en pantalla únicamente el marcador del último touch
};

/**
 * @class DisplayManager
 * @brief Administra el Layout inferior fijo (barra de navegación Lopaka), el tablero Home y la vista Settings/Recuperación.
 *
 * El Layout inferior (Y = 183..239) se dibuja una sola vez y permanece fijo. Al cambiar entre
 * ScreenId::Home y ScreenId::Settings únicamente se limpia el área de contenido superior (Y = 0..182)
 * para evitar parpadeos en la barra inferior. Dentro de cada vista sólo se redibujan los campos
 * modificados (dirty flags).
 */
class DisplayManager
{
public:
    using ScreenDrawFn = void (*)(void);

    static constexpr int16_t  SCREEN_WIDTH      = 320;     ///< Ancho total en rotación horizontal [px]
    static constexpr int16_t  SCREEN_HEIGHT     = 240;     ///< Alto total en rotación horizontal [px]
    static constexpr int16_t  LAYOUT_BAR_Y      = 190;     ///< Coordenada Y donde inicia la barra inferior fija de navegación [px]
    static constexpr int16_t  CONTENT_HEIGHT    = 190;     ///< Altura disponible para el contenido de cada Screen [px]
    static constexpr int16_t  MARKER_RADIUS     = 4;       ///< Radio en píxeles de la mira del último touch [px]
    static constexpr int16_t  RAW_GLITCH_BAND   = 12;      ///< Diferencia máxima admisible entre 2 muestras ADC seguidas
    static constexpr uint8_t  LANDSCAPE_ROT     = 1;       ///< Rotación horizontal estándar (320x240)
    static constexpr uint16_t TOUCH_RX_OHMS     = 300U;    ///< Resistencia nominal en placa X del panel táctil [ohms]
    static constexpr uint16_t FALLBACK_TFT_ID   = 0x9341U; ///< ID de respaldo ILI9341 si readID() devuelve valor inválido

    /**
     * @brief Construye el controlador de pantalla con los pines y calibración del proyecto.
     *
     * @param config Configuración de pines, umbrales y banderas de depuración del panel táctil.
     */
    explicit DisplayManager(const DisplayTouchConfig& config = defaultTouchConfig());

    /**
     * @brief Detecta el controlador (ILI9341), inicializa el bus paralelo de 8 bits y configura los botones del Layout.
     */
    void init();

    /**
     * @brief Activa o desactiva en tiempo de ejecución las banderas de depuración táctil.
     *
     * @param showCoords true para imprimir en pantalla las coordenadas RAW(x,y,z) y PX(x,y).
     * @param showMarker true para dibujar únicamente el marcador del último punto tocado.
     */
    void setDebugFlags(bool showCoords, bool showMarker);

    /**
     * @brief Cambia la pantalla activa conservando la barra inferior fija (Layout) sin parpadeo.
     *
     * @param screen Identificador de pantalla (ScreenId::Home o ScreenId::Settings).
     * @param forceRedraw true para redibujar también el Layout inferior; false para actualizar sólo el contenido.
     */
    void showScreen(ScreenId screen, bool forceRedraw = false);

    /**
     * @brief Proyecta de forma incremental (dirty flags) una instantánea del estado de la estación en la pantalla activa.
     *
     * Función no bloqueante. Sólo redibuja las regiones cuyos datos cambiaron respecto a la última instantánea.
     *
     * @param snapshot Estado actual de la estación suministrado por StationController.
     */
    void render(const DisplaySnapshot& snapshot);

    /**
     * @brief Muestrea el panel táctil, gestiona la navegación inferior y devuelve el comando operativo pulsado.
     *
     * Función no bloqueante. Debe invocarse periódicamente desde la tarea de interfaz (cada 50 ms).
     *
     * @return TouchCommand disparado en el ciclo actual, o TouchCommand::None si no hubo pulsación de comando.
     */
    TouchCommand pollTouchCommand();

    /**
     * @brief Lee el panel táctil y cambia entre Home y Settings al tocar los iconos de la barra inferior.
     *
     * Función no bloqueante mantenida por compatibilidad; invoca internamente pollTouchCommand().
     */
    void updateTouchNavigation();

    /**
     * @brief Consulta cuál es el identificador de pantalla actualmente en visualización.
     * @return Identificador ScreenId activo.
     */
    ScreenId getCurrentScreen() const;

    /**
     * @brief Muestrea el panel táctil resistivo con doble lectura de estabilidad, convierte a píxeles y restaura pines.
     *
     * Función no bloqueante. Si debugShowCoords o debugShowMarker están activos, actualiza la información en pantalla.
     *
     * @param px Referencia donde se escribe la coordenada X en píxeles [0..319] si hay toque válido.
     * @param py Referencia donde se escribe la coordenada Y en píxeles [0..239] si hay toque válido.
     * @return true si la presión Z y la estabilidad están dentro de rango; false si no se está tocando la pantalla.
     */
    bool readTouchPixel(int16_t& px, int16_t& py);

    /**
     * @brief Proporciona acceso directo a la instancia de MCUFRIEND_kbv.
     * @return Referencia al objeto gráfico MCUFRIEND_kbv.
     */
    MCUFRIEND_kbv& getTft();

    /**
     * @brief Genera la configuración táctil por defecto a partir de HardwarePins y ProcessParams.
     * @return Estructura DisplayTouchConfig inicializada con las constantes oficiales de la celda.
     */
    static DisplayTouchConfig defaultTouchConfig();

private:
    void initLayoutButtons();
    void restoreSharedTftPins();
    void drawCalibrationTargets();
    void updateDebugOverlay(int16_t rawX, int16_t rawY, int16_t rawZ, int16_t px, int16_t py);
    void drawTouchMarker(int16_t x, int16_t y, uint16_t color);

    void drawHomeStaticFrame();
    void drawAutoFillStaticFrame();
    void drawAutoCapStaticFrame();
    void drawDebugStaticFrame();
    void drawSettingsStaticFrame();
    void renderHomeDynamic(const DisplaySnapshot& snapshot, bool fullRedraw);
    void renderAutoFillDynamic(const DisplaySnapshot& snapshot, bool fullRedraw);
    void renderAutoCapDynamic(const DisplaySnapshot& snapshot, bool fullRedraw);
    void renderDebugDynamic(const DisplaySnapshot& snapshot, bool fullRedraw);
    void renderSettingsDynamic(const DisplaySnapshot& snapshot, bool fullRedraw);
    void renderBottomActionArea(const DisplaySnapshot& snapshot, bool fullRedraw);

    DisplayTouchConfig  config_;              ///< Configuración de pines y calibración del panel táctil
    TouchScreen         ts_;                  ///< Instancia del lector resistivo de 4 hilos
    ScreenId            currentScreen_;       ///< Pantalla actualmente visible (Home, AutoFill, AutoCap, Debug, Settings)
    bool                layoutDrawn_;         ///< Indica si la barra inferior fija ya fue pintada
    bool                contentFrameDrawn_;   ///< Indica si el marco estático de la pantalla activa ya fue pintado
    bool                hasRenderedSnapshot_; ///< Indica si ya existe una instantánea previa en caché
    DisplaySnapshot     renderedSnapshot_;    ///< Última instantánea proyectada en la pantalla TFT
    bool                hasLastMarker_;       ///< Indica si hay un marcador de depuración dibujado
    int16_t             lastMarkerX_;         ///< Última coordenada X de la mira de depuración [px]
    int16_t             lastMarkerY_;         ///< Última coordenada Y de la mira de depuración [px]

    // Botones táctiles de las 5 pestañas de navegación inferior (layout_base.svg)
    Adafruit_GFX_Button btnNavHome_;          ///< Pestaña 0: HOME (X = 0..63, Y = 190..239)
    Adafruit_GFX_Button btnNavAutoFill_;      ///< Pestaña 1: AUTO 1 - Llenado (X = 64..127, Y = 190..239)
    Adafruit_GFX_Button btnNavAutoCap_;       ///< Pestaña 2: AUTO 2 - Tapado (X = 128..191, Y = 190..239)
    Adafruit_GFX_Button btnNavDebug_;         ///< Pestaña 3: DEBUG (X = 192..253, Y = 190..239)
    Adafruit_GFX_Button btnNavSettings_;      ///< Pestaña 4: SETTINGS (X = 254..319, Y = 190..239)

    // Botones de Settings
    Adafruit_GFX_Button btnTestMode_;         ///< Botón/Checkbox en Settings: activar/desactivar TEST MODE
    Adafruit_GFX_Button btnSetProfile_;       ///< Botón en Settings: alternar perfil de botella
    Adafruit_GFX_Button btnJogFillUp_;        ///< Botón en Settings: subir eje Z de llenado en ManualRecovery
    Adafruit_GFX_Button btnJogCapUp_;         ///< Botón en Settings: subir eje Z de tapado en ManualRecovery
    Adafruit_GFX_Button btnReleaseArms_;      ///< Botón en Settings: levantar brazos inmediatamente en ManualRecovery
    Adafruit_GFX_Button btnConfirmClear_;     ///< Botón en Settings: confirmar despeje de línea y re-homing

    // Botones de control manual en Home (TestMode) y alarma en Home (ErrorStop)
    Adafruit_GFX_Button btnHomeBelt_;         ///< Botón en Home (TestMode): Banda 7V ON/OFF
    Adafruit_GFX_Button btnHomeBuzzer_;       ///< Botón en Home (TestMode): Buzzer ON/OFF
    Adafruit_GFX_Button btnHomeTowerGreen_;   ///< Botón en Home (TestMode): Torreta Verde ON/OFF
    Adafruit_GFX_Button btnHomeTowerRed_;     ///< Botón en Home (TestMode): Torreta Roja ON/OFF
    Adafruit_GFX_Button btnBottomAction_;     ///< Botón contextual de alerta/rearme rápido en Home

    // Botones de control manual en Auto 1 / Auto 2 (TestMode)
    Adafruit_GFX_Button btnActStopArm_;       ///< Botón en Auto 1/2 (TestMode): Brazo de Tope Arriba/Abajo
    Adafruit_GFX_Button btnActClampArm_;      ///< Botón en Auto 1/2 (TestMode): Brazo de Sujeción Arriba/Abajo
    Adafruit_GFX_Button btnActMainPower_;     ///< Botón en Auto 1/2 (TestMode): Bomba / Motor Apriete ON/OFF
    Adafruit_GFX_Button btnActJogZUp_;        ///< Botón en Auto 1/2 (TestMode): Subir eje Z (+JOG)
    Adafruit_GFX_Button btnActJogZDown_;      ///< Botón en Auto 1/2 (TestMode): Bajar eje Z (-JOG)
};
