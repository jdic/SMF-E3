# Arquitectura de Software y Hardware: Estación 3 (Llenado y Tapado)

Este documento define la arquitectura técnica integral, los estándares de codificación, los patrones de diseño embebido, el subsistema de diagnóstico con códigos de error y pantalla local, el modelo de concurrencia cooperativa, el acondicionamiento eléctrico y el catálogo de mejoras técnicas para la **Estación 3 (Llenado y Tapado)** en una línea de Sistemas de Manufactura Flexible (FMS).

El controlador principal de la estación es un **Arduino Mega 2560 (ATmega2560, 8 bits AVR @ 16 MHz, 256 KB Flash, 8 KB SRAM, 4 KB EEPROM, lógica de 5 V)**.

## 0. Descripción Física de la Estación

La estación es **lineal**: una banda transportadora que gira en un único sentido lleva las botellas a través de dos autómatas en serie. No existe mesa indexadora.

| Elemento | Descripción |
| :---- | :---- |
| **Banda transportadora** | Motor DC de **24 V** con puente H BTS7960 (un solo sentido de giro) y **encoder de cuadratura** (control de velocidad y detección de atasco). |
| **Autómata 1: Llenado** | Sensor de presencia de entrada; 2 brazos con servomotor (*brazo de tope*, el del final, y *brazo de sujeción*, el primero); bomba ON/OFF de llenado (con válvula de sobreflujo, el llenado es por tiempo); motor a pasos que sube y baja la boquilla/válvula, con su propio fin de carrera superior (*home*). |
| **Autómata 2: Tapado** | Sensor de presencia de entrada; 2 brazos con servomotor (tope y sujeción); motor DC (BTS7960) con encoder que gira para apretar la tapa; motor a pasos que sube y baja el cabezal de tapado. Su *home* es el sensor de fin de carrera superior. |
| **Flexibilidad** | La estación admite **dos tamaños de botella** (alturas distintas) mediante perfiles de botella que ajustan las cotas Z y el tiempo de llenado. |

**Secuencia de brazos y banda (idéntica en ambos autómatas):**

1. Al detectar una botella en su sensor de entrada, baja el **brazo de tope** (el del final de la zona).
2. La **banda no se detiene**: sigue girando y la botella, retenida por el brazo de tope, **resbala sobre la banda**. Tras `CLAMP_DELAY_MS` (1 s) baja el **brazo de sujeción**, que inmoviliza la botella.
3. Se ejecuta el proceso (llenado o tapado) y, al terminar, ambos brazos suben y la botella continúa arrastrada por la banda.

Como la banda gira de forma continua en modo `Running`, una botella retenida en un autómata **no afecta** al otro: las botellas de los demás tramos siguen avanzando. Para evitar saturar el tramo intermedio si el tapado demora más que el llenado, `StationController` administra una **cola intermedia (`interstageQueueCount_`, limitada por `MAX_INTERSTAGE_QUEUE`)**: si la cola está llena, el autómata de llenado aguarda en `WaitingQueueSlot` (con la boquilla ya arriba y los brazos abajo reteniendo la botella llena) hasta que el autómata de tapado libere un lugar.

## 1. Reglas de Idioma y Nomenclatura del Código

### 1.1 Políticas de Idioma

* **Identificadores del código (Inglés técnico obligatorio):** Clases, métodos, funciones, variables, estructuras, enumeraciones, macros, archivos y constantes deben declararse exclusivamente en inglés.

* **Comentarios y Documentación (Español técnico):** Los comentarios de código, descripciones en cabeceras Doxygen y manuales de arquitectura se redactan en español.

* **Textos dirigidos al usuario / HMI / Pantalla Local (Español):** Mensajes e interfaz gráfica en la pantalla táctil local TFT 2.4" (ILI9341), códigos descriptivos de fallo, terminal serie de diagnóstico y alarmas enviadas al panel SCADA de Node-RED deben estar redactados en español.

### 1.2 Formato y Estilo de Llaves (Allman Style)

El proyecto adopta estrictamente el estilo **Allman** (BSD style). Las llaves de apertura `{` y cierre `}` se colocarán siempre en su propia línea, alineadas verticalmente con la indentación de la estructura de control, clase o función asociada:

```cpp
// Correcto (Allman Style)
void executeCappingProcess()
{
    if (isBottleSeated)
    {
        startChuckMotor();
    }
    else
    {
        triggerAlarm(ErrorCode::CapBottleLost);
    }
}

// Prohibido (K&R / Java Style)
void executeCappingProcess() {
    if (isBottleSeated) {
        // ...
    }
}
```

### 1.3 Convenciones de Nomenclatura

| Elemento | Convención | Ejemplo |
| :---- | :---- | :---- |
| **Clases / Structs** | `PascalCase` | `StepperAxis`, `DisplayManager`, `StationController` |
| **Métodos y Funciones** | `camelCase` | `moveToTarget()`, `render()`, `update()` |
| **Variables locales y parámetros** | `camelCase` | `targetSpeed`, `pulseDelta` |
| **Miembros privados** | `camelCase` con sufijo `_` | `currentState_`, `encoderPulseCount_` |
| **Constantes y Macros** | `UPPER_SNAKE_CASE` | `MAX_SERVO_ANGLE`, `TFT_BG_COLOR`, `DEBOUNCE_MS` |
| **Valores de Enumeraciones (`enum class`)** | `PascalCase` | `CappingState::TighteningCap`, `ErrorCode::CapZHomeFailed` |
| **Nombres de Archivos** | `snake_case` uniforme | `display_manager.hpp`, `station_controller.cpp` |

### 1.4 Estándar de Documentación con Doxygen

Para mantener la claridad del código y habilitar la asistencia contextual (tooltips de IntelliSense) en VS Code / PlatformIO:

1. **Ubicación exclusiva en encabezados (`.hpp`):** Los bloques Doxygen se redactan **únicamente en los archivos `.hpp`**, documentando la interfaz pública y el contrato de cada clase. Los archivos `.cpp` se reservan para la implementación interna y solo llevan comentarios puntuales para lógica algorítmica compleja.
2. **Prohibición de redundancias:** No documentar lo obvio (ej. `@brief Obtiene el valor. @return El valor`).
3. **Énfasis en magnitudes, unidades y bloqueos:** Especificar explícitamente las unidades de los parámetros (pasos, pulsos, milisegundos, RPM, mA), el rango de operación admisible, si una función es no bloqueante y si es segura para invocarse desde una ISR.

#### Plantilla Oficial de Documentación Doxygen (Allman Style)

```cpp
/**
 * @file stepper_axis.hpp
 * @brief Controlador no bloqueante para actuadores lineales y rotativos a pasos.
 */

#pragma once
#include <Arduino.h>

/**
 * @class StepperAxis
 * @brief Gestiona el perfil de movimiento, homing y límites de carrera de un eje paso a paso.
 */
class StepperAxis
{
public:
    /**
     * @brief Configura el desplazamiento hacia una posición absoluta objetivo.
     *
     * Calcula el perfil trapezoidal sin bloquear la CPU. Los pulsos se emiten
     * desde la ISR del Timer3; no requiere llamadas desde el super-loop.
     *
     * @param targetSteps Posición de destino expresada en micropasos absolutos.
     * @param maxSpeed Velocidad límite de crucero en pasos por segundo [step/s] (máx. 8000).
     * @return true si la consigna está dentro de los límites por software; false si se rechazó.
     */
    bool moveToTarget(int32_t targetSteps, uint16_t maxSpeed);

    /**
     * @brief Consulta si el actuador se encuentra actualmente ejecutando una trayectoria.
     * @return true si el eje está en movimiento; false si alcanzó la cota de reposo.
     */
    bool isBusy() const;
};
```

### 1.5 Regla de Archivos de Configuración (Sin Números Mágicos)

Toda cifra del proyecto vive en **uno de dos archivos** dentro de `include/config/`:

| Archivo | Contenido | Quién lo edita |
| :---- | :---- | :---- |
| **`hardware_pins.hpp`** | **Solo pines**: asignación eléctrica de cada GPIO (§7.2). | Quien cablea la estación. |
| **`process_params.hpp`** | **Todo lo cuantitativo**: pasos, velocidades, aceleraciones, límites de carrera, tiempos, ángulos de servo, parámetros de la banda y del apriete, perfiles de botella y calibración del touch (§5.5). | Quien calibra el proceso. |

Está **prohibido** escribir en `.cpp` / `.hpp` de lógica literales numéricos de pines, tiempos, pasos o ángulos. Separar ambos archivos permite recablear sin tocar la calibración y recalibrar sin tocar el cableado.

## 2. Decisiones Clave de Infraestructura y Datos

### 2.1 Ecosistema de Compilación y Plataforma

* **Plataforma:** PlatformIO (`platform = atmelavr`, `framework = arduino`, `board = megaatmega2560`).

* **Frecuencia de CPU:** 16 MHz (cristal externo fijo). Todo el código de control debe dimensionarse para un microcontrolador de 8 bits sin FPU: preferir aritmética entera / punto fijo sobre `float` en las rutas críticas.

* **Bootloader y Watchdog:** El bootloader original del Mega 2560 **no desactiva el watchdog** tras un reinicio por WDT y puede quedar en un bucle de reinicios. Antes de habilitar el watchdog en producción se debe:
  1. Reemplazar el bootloader por uno compatible con WDT (**Optiboot para Mega**, p. ej. el de MegaCore) mediante un programador ISP, **o** bien
  2. Ejecutar la **prueba de aceptación**: forzar un reinicio por WDT (bucle infinito deliberado) y comprobar que la placa vuelve a arrancar. Si queda en bucle, **no se habilita el watchdog** hasta cambiar el bootloader.

* **Control de Versiones de Dependencias:** El archivo `platformio.ini` debe fijar versiones específicas para librerías externas:

```ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson @ 6.21.5
    knolleary/PubSubClient @ 2.8
    arduino-libraries/Ethernet @ 2.0.2
    adafruit/Adafruit GFX Library @ 1.11.9
    prenticedavid/MCUFRIEND_kbv @ 3.1.0-Beta
    adafruit/Adafruit TouchScreen @ 1.1.5
    arduino-libraries/Servo @ 1.2.1
```

> ArduinoJson se fija en la rama 6.x porque permite `StaticJsonDocument` (memoria en pila, sin heap), crítico con 8 KB de SRAM.

### 2.2 Temporización y Concurrencia (Super-Loop Cooperativo + ISR)

El ATmega2560 tiene **un solo núcleo** y no se usará FreeRTOS: el costo de pila por tarea (≥ 200 B cada una) y el consumo del kernel no se justifican con 8 KB de SRAM.

* **Prohibición de funciones bloqueantes:** Prohibido el uso de `delay()` en cualquier parte del firmware posterior a `setup()`. Las temporizaciones de estado se realizan con aritmética no bloqueante sobre `millis()` (con resta sin signo para tolerar el desbordamiento a los 49.7 días).

* **Modelo de dos niveles:**

  * **Nivel de interrupción (tiempo real duro):** ISR del **Timer3** a 20 kHz para la generación de pulsos `STEP` de los dos ejes Z (llenado y tapado) **y la vigilancia de sus fines de carrera y límites por software**, e ISRs de **INT0–INT3** para el conteo de cuadratura de los dos encoders (cabezal de apriete y banda).

  * **Nivel de super-loop (tiempo real blando):** Planificador cooperativo por tiempos en `loop()` que despacha los dos autómatas, el muestreo de sensores, el control de velocidad de la banda, el renderizado incremental de la pantalla TFT + lectura táctil y la pila de red, cada uno con su propio periodo.

* **Renderizado incremental en TFT paralela (*Dirty-Region Pattern*):** Un borrado completo (`fillScreen()`) sobre 320×240 píxeles (153.6 KB en RGB565) por bus paralelo de 8 bits toma ~40–80 ms. Para no penalizar el super-loop:
  1. `DisplayManager` dibuja el marco estático (barra de título, bordes, etiquetas fijas y botones táctiles) **únicamente al cambiar de vista** (Operación, Alarma o Recuperación Manual).
  2. En cada ciclo periódico de 50 ms solo sobreescribe el campo de texto que haya cambiado (estado de cada autómata, contador o código de error) usando color de fondo opaco (`setTextColor(fg, bg)`), acotando el tiempo de dibujo a **< 5 ms**.
  3. Como los pulsos `STEP` se emiten por ISR en el Timer3 y los fines de carrera se vigilan en esa misma ISR, ni siquiera el redibujado de un cambio de vista provoca pérdida de pasos ni sobrecarrera.

### 2.3 Conteo de Encoders por Interrupción Externa

* El ATmega2560 no dispone de un periférico contador de cuadratura. Se usan las **interrupciones externas** `INT0`–`INT3` con decodificación x2 o x4 en la ISR: **encoder del cabezal de apriete** (`INT0`/`INT1`) y **encoder del motor de la banda** (`INT2`/`INT3`).

* **Pines con interrupción externa en el Mega:** 2, 3, 18, 19, 20, 21. Como la pantalla TFT utiliza interfaz paralela (`D2-D9` y `A0-A4`) y no requiere bus I2C, los pines **21 (`INT0`) y 20 (`INT1`) quedan libres para el encoder del cabezal de apriete**, mientras que **19 (`INT2`) y 18 (`INT3`) se asignan al encoder de la banda** (inhabilitando `Serial1`). Los pines 2 y 3 quedan dedicados al bus de datos de la pantalla (`LCD_D2` y `LCD_D3`).

* Las ISRs de encoder deben leer los canales mediante acceso directo a registros (`PIND`) en lugar de `digitalRead()` (≈ 4 µs por llamada), y la lectura de los contadores de 32 bits desde el super-loop debe protegerse con `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`.

* **Límite de diseño:** con ISR en C optimizada (~3 µs), la tasa máxima sostenible es del orden de 50 000 flancos/s sumando ambos encoders. Se recomiendan encoders de ≤ 600 PPR.

* **Ventajas del encoder de la banda** (implementadas en `ConveyorController`, §4.1):
  1. **Velocidad constante:** lazo PI sobre las RPM medidas, de modo que la velocidad no dependa de la carga (fricción de botellas retenidas resbalando sobre la banda) ni de la tensión de la fuente de 24 V.
  2. **Detección de atasco:** con PWM aplicado y sin pulsos durante `BELT_STALL_MS`, se declara `ERR-303` y se evita calentar el motor y el BTS7960.

### 2.4 Modelo de Datos y Telemetría

* **Capa de Comunicación:** El Mega 2560 carece de conectividad inalámbrica. La integración con **Node-RED** vía MQTT se realiza mediante un **módulo Ethernet W5500 independiente** conectado por jumpers al bus SPI del Mega (pines ICSP/50-53), con las librerías `Ethernet` y `PubSubClient`. La inicialización debe declarar explícitamente el pin de selección: `Ethernet.init(HardwarePins::PIN_ETH_CS)`.

* **Cableado del módulo W5500:**

| Pin módulo W5500 | Pin Mega 2560 | Notas |
| :---- | :---- | :---- |
| `MISO` | 50 | Salida de 3.3 V; el AVR la lee correctamente como nivel alto. |
| `MOSI` | 51 | Las entradas del W5500 toleran 5 V. |
| `SCLK` | 52 | |
| `SCS` / `CS` | 53 | SS de hardware; garantiza que el SPI permanezca en modo maestro. |
| `RST` | 49 | Reset por hardware desde el firmware (activo en bajo). |
| `INT` | Sin conectar | La librería `Ethernet` trabaja por sondeo. |
| `3.3V` / `5V` | Fuente externa | Ver §7.1 (Alimentación). |
| `GND` | GND | Tierra común con el Mega. |

* **Integridad de señal con jumpers:** Mantener los cables SPI **por debajo de 15 cm** y trenzar o agrupar cada señal con un GND. Si aparecen fallos de comunicación (`Ethernet.hardwareStatus() == EthernetNoHardware`), reducir el reloj SPI de la librería (por defecto 14 MHz) a 4 MHz redefiniendo `SPI_ETHERNET_SETTINGS` en `build_flags`.

* **Secuencia de arranque:** En `setup()`, mantener `RST` en bajo ≥ 1 ms, liberarlo y esperar ~150 ms antes de `Ethernet.begin()` (única espera bloqueante admitida, antes de habilitar el watchdog). Si el enlace se pierde de forma prolongada, `ScadaClient` puede repetir este reset por hardware.

* **Formato de Carga Útil (Payload):** Tramas en formato JSON compacto (`StaticJsonDocument<256>`) enviando telemetría de producción, estado de sensores, velocidad de la banda, códigos de error activos y recibiendo consignas de operación.

* **Gestión de memoria:** Prohibido el uso de la clase `String` y de `new`/`malloc` después de `setup()`. Todos los literales de texto se almacenan en Flash con `F("...")` o `PROGMEM`.

## 3. Taxonomía de Códigos de Error y Sistema de Diagnóstico Local

Para facilitar la rápida intervención de los operarios en planta sin requerir una computadora conectada al puerto serie, la estación incorpora una taxonomía estructurada de códigos de error numéricos indexados en el manual de usuario de la celda.

### 3.1 Estructura Numérica de Errores

Los errores se clasifican en cuatro rangos numéricos bien definidos dentro del enum class `ErrorCode`:

* **`100 - 199`:** Sensores e Insumos (Botella perdida en el autómata de llenado o de tapado tras bajar los brazos).

* **`200 - 299`:** Cinemática y Recorridos. Cada eje Z tiene su propia decena: **`201-209` eje Z de llenado** y **`211-219` eje Z de tapado** (fallo de homing, soft limit rebasado, timeout de bajada, timeout de subida).

* **`300 - 399`:** Actuadores y Potencia (Sobrecorriente de apriete, stall anómalo del cabezal, atasco de la banda, banda que no se detiene).

* **`900 - 999`:** Seguridad y Enlace (Seta E-STOP activada, enlace MQTT/SCADA desconectado).

### 3.2 Catálogo Oficial de Códigos de Error y Tipos de la Estación (`station_types.hpp`)

```cpp
#pragma once
#include <Arduino.h>

enum class ErrorCode : uint16_t
{
    None                    = 0,

    // 100s: Sensores y Presencia de Botella
    FillBottleLost          = 101, // ERR-101: Botella no detectada en llenado tras bajar los brazos
    CapBottleLost           = 102, // ERR-102: Botella no detectada en tapado tras bajar los brazos

    // 200s: Cinemática y Movimiento - Eje Z de Llenado (201-209)
    FillZHomeFailed         = 201, // ERR-201: Eje Z de llenado no halló fin de carrera
    FillZSoftLimitHit       = 202, // ERR-202: Cota máxima de llenado rebasada (Soft limit)
    FillZLoweringTimeout    = 203, // ERR-203: Boquilla no alcanzó la cota de llenado a tiempo
    FillZRaisingTimeout     = 204, // ERR-204: Boquilla no alcanzó el fin de carrera superior a tiempo

    // 200s: Cinemática y Movimiento - Eje Z de Tapado (211-219)
    CapZHomeFailed          = 211, // ERR-211: Eje Z de tapado no halló fin de carrera
    CapZSoftLimitHit        = 212, // ERR-212: Cota máxima de tapado rebasada (Soft limit)
    CapZLoweringTimeout     = 213, // ERR-213: Cabezal no alcanzó la cota de tapado a tiempo
    CapZRaisingTimeout      = 214, // ERR-214: Cabezal no alcanzó el fin de carrera superior a tiempo

    // 300s: Apriete, Banda y Potencia
    ChuckOvercurrent        = 301, // ERR-301: Corriente excesiva en motor de apriete
    ChuckStallPremature     = 302, // ERR-302: Bloqueo prematuro (rosca trasroscada)
    ConveyorStall           = 303, // ERR-303: Banda atascada (PWM aplicado y encoder sin pulsos)

    // 900s: Seguridad y Red
    EmergencyStopPressed    = 901, // ERR-901: Parada de emergencia por hardware activada
    NetworkLinkLost         = 902  // ERR-902: Pérdida prolongada de enlace con SCADA
};

/**
 * @brief Modo global de la estación. Un error en cualquier autómata detiene ambos.
 */
enum class StationMode : uint8_t
{
    Boot,           // Homing de ambos ejes Z; brazos arriba; bomba y banda apagadas
    Running,        // Banda en giro continuo (lazo PI); ambos autómatas operan en paralelo
    ErrorStop,      // Bomba, PWM y drivers deshabilitados
    ManualRecovery  // Comandos manuales por pantalla táctil
};

/**
 * @brief Perfil de botella (dos tamaños). Define cotas Z y tiempo de llenado.
 */
enum class BottleProfile : uint8_t
{
    Small,
    Large
};

enum class FillState : uint8_t
{
    Idle,
    StopArmDown,      // Baja el brazo de tope; espera CLAMP_DELAY_MS (1 s) con la banda girando
    ClampArmDown,     // Baja el brazo de sujeción y confirma presencia
    LoweringNozzle,
    Filling,
    Draining,         // Espera de goteo con la bomba apagada
    RaisingNozzle,
    WaitingQueueSlot, // Retiene la botella llena si la cola hacia Tapado alcanzó MAX_INTERSTAGE_QUEUE
    ReleasingBottle   // Sube brazo de sujeción y después el de tope
};

enum class CappingState : uint8_t
{
    Idle,
    StopArmDown,
    ClampArmDown,
    LoweringHead,
    TighteningCap,
    RaisingHead,
    ReleasingBottle
};

/**
 * @brief Comandos originados por los botones táctiles de la pantalla TFT.
 * Todo comando se descarta mientras la seta E-STOP siga activa (PIN_ESTOP_AUX).
 */
enum class TouchCommand : uint8_t
{
    None = 0,
    CycleBottleProfile, // En Running con ambos autómatas en Idle: alterna Small/Large
    AcknowledgeAlarm,   // En ErrorStop: pasar a ManualRecovery
    JogFillZUp,         // En ManualRecovery: subir eje Z de llenado por pasos seguros
    JogCapZUp,          // En ManualRecovery: subir eje Z de tapado por pasos seguros
    ReleaseArms,        // En ManualRecovery: subir ambos pares de brazos
    ConfirmLineClear    // En ManualRecovery: confirmar despeje y volver a Boot
};

/**
 * @brief Instantánea del estado a proyectar en la pantalla TFT.
 * DisplayManager traduce cada enum a su texto en español desde tablas PROGMEM.
 */
struct DisplaySnapshot
{
    ErrorCode error;
    StationMode mode;
    FillState fillState;
    CappingState capState;
    BottleProfile profile;
    uint8_t queueCount;
    uint32_t bottleCount;
};
```

### 3.3 Interfaz Gráfica Táctil en Pantalla Local (TFT 2.4" 320×240 ILI9341)

La pantalla opera en orientación horizontal (`setRotation(1)`, 320×240 píxeles, color RGB565) dividida en tres zonas con botones táctiles contextuales para operación con lápiz (*stylus*). Con la fuente por defecto a tamaño 2 (12 px por carácter) caben **26 columnas**; los textos se redactan sin tildes ni `ñ` (la fuente 5x7 integrada no las incluye):

```text
+--------------------------+
|ESTACION 3 LLENADO/TAPADO | <- Cabecera fija (320x32 px)
+--------------------------+
|MODO: ERROR               | <- Banda de color según modo (Verde / Ambar / Rojo)
|LLENADO: BAJANDO BOQUILLA |
|TAPADO : EN ESPERA        |
|ERR-102 BOTELLA PERDIDA   | <- Textos leídos desde PROGMEM
|REVISE SENSOR DE TAPADO   |
|PRODUCCION: 00142         |
|PERFIL: BOTELLA PEQUENA   |
+--------------------------+
|        [ REARME ]        | <- Barra táctil (320x56 px)
+--------------------------+
```

* **Botones táctiles según el modo de la estación:**
  * **Vista Operativa (`Running`):** botón `[ PERFIL ]` (`CycleBottleProfile`), **habilitado únicamente cuando ambos autómatas están en `Idle`**, para evitar cambiar de perfil con una botella en proceso. En el resto del ciclo la barra inferior es informativa.
  * **Vista de Alarma (`ErrorStop`):** muestra el botón `[ REARME ]`, que emite `TouchCommand::AcknowledgeAlarm` (equivalente en paralelo al pulsador físico `PIN_BTN_RESET`).
  * **Vista de Recuperación (`ManualRecovery`):** muestra cuatro botones táctiles: `[ Z LLENADO ]` (`JogFillZUp`), `[ Z TAPADO ]` (`JogCapZUp`), `[ SOLTAR ]` (`ReleaseArms`) y `[ DESPEJADO ]` (`ConfirmLineClear`) para liberar envases atascados antes de reiniciar el homing.

* **Interbloqueo con la seta E-STOP:** `StationController::handleTouchCommand()` **descarta todo comando táctil (y el pulsador físico de rearme) mientras `PIN_ESTOP_AUX` indique la seta activa**. Así, ni el rearme ni los jog pueden volver a habilitar drivers con la seta pulsada.

* **Flujo de diseño visual con Lopaka.app + `Adafruit_GFX_Button`:**
  1. **Maquetado en [Lopaka.app](https://lopaka.app/):** las vistas estáticas (marcos, separadores, iconos y etiquetas fijas) se diseñan sobre un lienzo de **320×240 px** seleccionando el perfil **`Adafruit_GFX`**.
  2. **Bitmaps en Flash (`PROGMEM`):** todo icono o gráfico exportado desde Lopaka debe declararse como `const uint8_t bitmap[] PROGMEM` e invocarse con `tft.drawBitmap(...)` para residir en los 256 KB de Flash y consumir **0 bytes de SRAM**.
  3. **Botones táctiles con `Adafruit_GFX_Button`:** se utiliza la clase `Adafruit_GFX_Button` incluida de serie en `Adafruit GFX Library` (~23 bytes de SRAM por botón). Sus etiquetas internas admiten hasta **9 caracteres** (`char[10]`), por lo que se usan textos cortos (`"REARME"`, `"PERFIL"`, `"Z LLENADO"`, `"Z TAPADO"`, `"SOLTAR"`, `"DESPEJADO"`) o bien se dibuja la estética con las primitivas de Lopaka y se delega a `Adafruit_GFX_Button` la detección de área (`contains(px, py)`) y de flanco de liberación (`justReleased()`).

## 4. Patrones de Diseño Recomendados

### 4.1 Hardware Abstraction Layer (HAL) / Component Object Pattern

Aísla el silicio del ATmega2560 y el hardware de potencia de la lógica de proceso. Las FSM nunca invocan directamente primitivas como `digitalWrite()`, `analogRead()` ni manipulan registros periféricos.

1. **Clase `DigitalSensor`:**

   * Encapsula sensores infrarrojos (IR) de presencia, finales de carrera mecánicos y la entrada auxiliar de la seta.

   * Filtro de rebotes (*debounce*) no bloqueante mediante `millis()`.

   * Polaridad configurable para entradas activas en alto (`Active HIGH`) o activas en bajo (`Active LOW`); habilita `INPUT_PULLUP` interno cuando aplica.

   * Métodos: `isTriggered()`, `hasChanged()`, `onRisingEdge()`, `onFallingEdge()`.

2. **Clase `DcMotorBts7960`:**

   * Controla módulos de puente H BTS7960 mediante `analogWrite()` sobre pines PWM por hardware.

   * Administra pines de habilitación (`EN`) y canales de modulación (`RPWM`, `LPWM`). El motor de la banda usa solo `RPWM` (un sentido de giro).

   * Incorpora frenado dinámico (`brake()`) y parada libre (`stop()`). El tiempo muerto lo resuelve internamente el BTS7960; la clase solo garantiza que nunca se modulen `RPWM` y `LPWM` simultáneamente.

3. **Clase `InterruptEncoder`:**

   * Abstrae un encoder de cuadratura conectado a dos pines con interrupción externa. Hay **dos instancias**: cabezal de apriete (`INT0`/`INT1`) y banda (`INT2`/`INT3`).

   * Expone métodos de lectura atómica: `getPulses()`, `reset()`, `getSpeedRpm()`.

4. **Clase `StepperAxis`:**

   * Controla drivers de paso/dirección (TB6600, DM542) para los ejes Z de llenado y de tapado.

   * Rampas de aceleración trapezoidales calculadas en el super-loop; emisión de pulsos desde la ISR compartida del **Timer3** mediante escritura directa a puerto (`PORTC`).

   * **Protección en ISR:** en cada tick, la ISR lee directamente el fin de carrera del eje (`PINA`) cuando éste se mueve hacia *home*, compara el contador de pasos contra el *soft limit* (`setSoftLimits()`) y **detiene el eje sin esperar al super-loop**, levantando una bandera `volatile` que el autómata convierte en `ERR-202` / `ERR-212`.

   * Métodos: `moveToTarget()`, `isBusy()`, `home()`, `setSoftLimits()`, `stop()`.

5. **Clase `BottleGate`:**

   * Agrupa los dos servomotores de un autómata: **brazo de tope** (el del final) y **brazo de sujeción** (el primero). Existen dos instancias: `fillGate` y `capGate`.

   * Servos sin retroalimentación: los movimientos se confían a tiempos y ángulos de `process_params.hpp`, sin bloquear.

   * Métodos: `lowerStopArm()`, `lowerClampArm()`, `releaseBottle()` (sujeción primero, tope después tras `RELEASE_GAP_MS`), `isSettled()`.

6. **Clase `FillPump`:**

   * Salida digital ON/OFF hacia la etapa de potencia de la bomba de llenado.

   * **Protección propia:** apaga la bomba si lleva encendida más de `MAX_PUMP_ON_MS`, aunque la FSM no lo ordene. `off()` se invoca en la fase Exit de `Filling` y en `ErrorStop`.

   * Métodos: `on()`, `off()`, `isOn()`, `update()`.

7. **Clase `ConveyorController`:**

   * Integra una instancia de `DcMotorBts7960` (un solo sentido, motor de 24 V) con una instancia de `InterruptEncoder`.

   * **Lazo PI de velocidad** sobre las RPM medidas (`BELT_TARGET_RPM`), ejecutado en la tarea de perfiles de movimiento (cada 20 ms). Compensa el rozamiento cuando hay botellas retenidas resbalando sobre la banda. El PWM resultante se limita a 0-255.

   * **Detección de atasco:** si el PWM supera `BELT_STALL_MIN_PWM` y no hay pulsos durante `BELT_STALL_MS`, activa una bandera que el mediador convierte en `ERR-303`.

   * Métodos: `start()`, `stop()`, `brake()`, `update()`, `isStalled()`, `getSpeedRpm()`.

8. **Clase `DisplayManager`:**

   * Encapsula el controlador gráfico `MCUFRIEND_kbv` (ILI9341 en bus paralelo de 8 bits), el lector táctil resistivo de 4 hilos `TouchScreen` y las instancias de `Adafruit_GFX_Button` (`btnProfile_`, `btnReset_`, `btnJogFill_`, `btnJogCap_`, `btnRelease_`, `btnClear_`).

   * Integra las funciones de dibujo de pantallas generadas desde **Lopaka.app** (marcos y bitmaps en `PROGMEM`) y aplica renderizado incremental (*dirty flags*): solo redibuja el marco completo al cambiar de vista y actualiza únicamente las regiones cuyos valores cambiaron.

   * Procesa los toques convirtiendo las lecturas analógicas crudas del panel (`TSPoint`) a coordenadas de píxeles (0..319, 0..239) con `map()`, actualiza cada botón activo mediante `btn.press(isTouched && btn.contains(px, py))`, invierte el color del botón al presionar (`justPressed()` → `drawButton(true)`) y dispara el `TouchCommand` únicamente al soltar (`justReleased()` → `drawButton(false)`).

   * **Regla crítica de pines compartidos (Touch + TFT):** El panel resistivo comparte los pines analógicos `XM` y `YP` con las líneas de control `LCD_RS` / `LCD_WR` / `LCD_CS` del TFT. Inmediatamente después de cada llamada a `TouchScreen::getPoint()`, `DisplayManager` **debe restaurar** `pinMode(XM, OUTPUT)` y `pinMode(YP, OUTPUT)` antes de invocar cualquier primitiva gráfica o `drawButton()`.

   * Métodos: `init()`, `showBoot()`, `render(const DisplaySnapshot&)`, `pollTouchCommand()`, `update()`.

### 4.2 Mediator Pattern (Coordinador de Estación)

Elimina el acoplamiento cruzado entre actuadores, sensores y pantallas.

* La clase mediadora **`StationController`**:

  * Contiene las instancias de todos los sensores y actuadores, los dos autómatas (`FillingAutomaton` y `CappingAutomaton`), el modo global (`StationMode`) y el perfil de botella activo. Publica una `DisplaySnapshot` cuando cambia algún estado.

  * Inspecciona periódicamente el estado de los componentes (`fillZ.isBusy()`, `presenceFill.isTriggered()`, `conveyor.isStalled()`).

  * Despacha comandos a los módulos de hardware según el estado activo de cada autómata, evitando que los periféricos se conozcan o se invoquen entre sí.

  * **Gobierna la banda y la cola intermedia:** mantiene la banda en giro continuo durante el modo `Running` y la detiene en `Boot`, `ErrorStop` y `ManualRecovery`. Administra el contador `interstageQueueCount_` (incrementa cuando `FillingAutomaton` entra en `ReleasingBottle` y decrementa cuando `CappingAutomaton` entra en `ReleasingBottle`, reiniciándose a `0` en `Boot`), autorizando la salida de `WaitingQueueSlot` únicamente mientras `interstageQueueCount_ < MAX_INTERSTAGE_QUEUE`.

  * **Detiene ambos autómatas** ante cualquier error (modo `ErrorStop`) y filtra los comandos táctiles según la seta E-STOP (§3.3).

* Los autómatas **`FillingAutomaton`** y **`CappingAutomaton`** no se conocen entre sí: cada uno reacciona únicamente a su propio sensor de presencia, mientras las botellas retenidas por los brazos resbalan sobre la banda en movimiento.

* **Protección contra doble procesado:** al entrar en `ReleasingBottle`, el autómata activa la bandera `needsClear_`; mientras ésta siga activa (la botella aún frente al sensor), el estado `Idle` **no acepta un nuevo disparo**. La bandera se desactiva cuando el sensor de presencia se libera.

### 4.3 Enum FSM con Ciclo de Vida de Tres Fases

Para evitar la sobrecarga polimórfica (`vtables` en SRAM) y el riesgo de fragmentación de memoria dinámica del patrón *State* clásico de GoF, cada autómata se implementa como una **FSM basada en `enum class` con control explícito de tres fases**:

* **Fase Entry:** Se ejecuta una única vez al entrar al estado (movimiento de brazos, arranque de motores o bomba, inicio de temporizadores, configuración de consignas).

* **Fase During:** Se ejecuta continuamente en cada ciclo de actualización mientras persista el estado activo. Monitorea timeouts de guardia.

* **Fase Exit:** Se ejecuta una única vez al detectar una transición de estado, inmediatamente antes de cambiar de fase (parada de actuadores, apagado de bomba, limpieza de banderas).

#### Estructura de Control de la FSM (Allman Style)

Los enums `FillState` y `CappingState` están definidos en `station_types.hpp` (§3.2). `CappingAutomaton::update()` es idéntico, con `CappingState`.

```cpp
void FillingAutomaton::update()
{
    // 1. Detección y ejecución de la fase de salida (Exit)
    if (nextState_ != currentState_)
    {
        onExit(currentState_);
        previousState_ = currentState_;
        currentState_ = nextState_;
        isFirstRun_ = true;
    }

    // 2. Ejecución de la fase de entrada (Entry)
    if (isFirstRun_)
    {
        onEntry(currentState_);
        isFirstRun_ = false;
    }

    // 3. Ejecución continua de la fase activa (During)
    onDuring(currentState_);
}
```

## 5. Autómatas de Llenado y Tapado (FSM)

### 5.1 Modo Global de la Estación

```text
[Boot] ──(Homing exitoso de ambos ejes Z; brazos arriba; bomba OFF)──► [Running]

═════════════════ RUTAS DE ALARMA ════════════════════
Cualquier Modo ──(Timeout / Fallo / E-STOP)──► [ErrorStop]
[ErrorStop] ──(Rearme, con seta liberada)──► [ManualRecovery] ──(Despeje confirmado)──► [Boot]
```

En `Running`, el mediador mantiene la banda girando de forma continua. El brazo de tope detiene físicamente cada botella frente a su autómata (la botella retenida resbala sobre la banda en movimiento).

### 5.2 Autómata 1: Llenado

```text
[Idle] ◄──────────────────────────────────────────────────────┐
      │                                                       │
      ▼ (Presencia de llenado = Activo y needsClear_ = false) │
[StopArmDown]                                                 │
      │                                                       │
      ▼ (Transcurre CLAMP_DELAY_MS)                           │
[ClampArmDown]                                                │
      │                                                       │
      ▼ (Transcurre ARM_MOVE_MS y la presencia se mantiene)   │
[LoweringNozzle]                                              │
      │                                                       │
      ▼ (Eje Z alcanza la cota de llenado del perfil)         │
[Filling]                                                     │
      │                                                       │
      ▼ (Transcurre fillMs del perfil; bomba OFF)             │
[Draining]                                                    │
      │                                                       │
      ▼ (Transcurre DRIP_MS)                                  │
[RaisingNozzle]                                               │
      │                                                       │
      ▼ (Fin de carrera superior activado)                    │
[WaitingQueueSlot]                                            │
      │                                                       │
      ▼ (interstageQueueCount_ < MAX_INTERSTAGE_QUEUE)        │
[ReleasingBottle]  (interstageQueueCount_++)                  │
      │                                                       │
      └────── (Transcurre RELEASE_MS; needsClear_ = true) ────┘
```

### 5.3 Autómata 2: Tapado

```text
[Idle] ◄──────────────────────────────────────────────────────┐
      │                                                       │
      ▼ (Presencia de tapado = Activo y needsClear_ = false)  │
[StopArmDown]                                                 │
      │                                                       │
      ▼ (Transcurre CLAMP_DELAY_MS)                           │
[ClampArmDown]                                                │
      │                                                       │
      ▼ (Transcurre ARM_MOVE_MS y la presencia se mantiene)   │
[LoweringHead]                                                │
      │                                                       │
      ▼ (Eje Z alcanza la cota de tapado del perfil)          │
[TighteningCap]                                               │
      │                                                       │
      ▼ (Transcurre TIGHTEN_MS; motor DC en freno)            │
[RaisingHead]                                                 │
      │                                                       │
      ▼ (Fin de carrera superior activado)                    │
[ReleasingBottle]                                             │
      │                                                       │
      └── (Transcurre RELEASE_MS; bottleCount++ en Entry;     │
           needsClear_ = true) ───────────────────────────────┘
```

### 5.4 Matrices de Transición de Estados y Timeouts de Guardia

#### Modo global

| Modo | Fase Entry | Condición de Salida (During) | Timeout de Guardia | Fallo / Siguiente Modo |
| :---- | :---- | :---- | :---- | :---- |
| `Boot` | Brazos arriba, bomba OFF, banda detenida, iniciar homing de ambos ejes Z. | `fillZ.isHome() && capZ.isHome()` | 10000 ms | `ERR-201` / `ERR-211` → `ErrorStop` |
| `Running` | Ambos autómatas en `Idle`; la banda arranca y gira continuamente. | Operación continua. | Sin timeout | Cualquier fallo → `ErrorStop` |
| `ErrorStop` | Bomba OFF, PWM a cero, banda frenada, drivers deshabilitados. | Rearme recibido **y** seta liberada. | Sin timeout | → `ManualRecovery` |
| `ManualRecovery` | Habilitar comandos manuales por pantalla táctil. | Operador confirma despeje de línea. | Sin timeout | → `Boot` |

Vigilancia continua en `Running`: `conveyor.isStalled()` → `ERR-303` → `ErrorStop`.

#### Autómata de Llenado (`FillState`)

| Estado Origen | Fase Entry | Condición de Salida (During) | Timeout de Guardia | Fallo / Siguiente Estado |
| :---- | :---- | :---- | :---- | :---- |
| `Idle` | Brazos arriba, bomba OFF; vigilar entrada. | `presenceFill.isTriggered() && !needsClear_` | Sin timeout | → `StopArmDown` |
| `StopArmDown` | Bajar brazo de tope (la botella se asienta contra el tope). | `millis() - stageTimer_ >= CLAMP_DELAY_MS` | Sin timeout | → `ClampArmDown` |
| `ClampArmDown` | Bajar brazo de sujeción. | `millis() - stageTimer_ >= ARM_MOVE_MS` y `presenceFill.isTriggered()` | `ARM_MOVE_MS + 500 ms` | Presencia perdida: `ERR-101` → `ErrorStop` |
| `LoweringNozzle` | Mover eje Z a `profile.fillZSteps`. | `fillZ.isTargetReached()` | 5000 ms | `ERR-203` → `ErrorStop` (soft limit: `ERR-202`) |
| `Filling` | Bomba ON. | `millis() - stageTimer_ >= profile.fillMs` | No aplica (salida por tiempo; `FillPump` impone `MAX_PUMP_ON_MS`) | → `Draining` |
| `Draining` | Bomba OFF (en Exit de `Filling`); esperar goteo. | `millis() - stageTimer_ >= DRIP_MS` | Sin timeout | → `RaisingNozzle` |
| `RaisingNozzle` | Subir eje Z hacia fin de carrera. | `limitSwitchFill.isTriggered()` | 5000 ms | `ERR-204` → `ErrorStop` (éxito → `WaitingQueueSlot`) |
| `WaitingQueueSlot` | Mantener brazos abajo reteniendo la botella llena con la boquilla arriba. | `canReleaseBottle_` (`interstageQueueCount_ < MAX_INTERSTAGE_QUEUE`) | Sin timeout | → `ReleasingBottle` |
| `ReleasingBottle` | `interstageQueueCount_++`; subir brazo de sujeción; tras `RELEASE_GAP_MS`, subir el de tope; activar `needsClear_`. | `millis() - stageTimer_ >= RELEASE_MS` | Sin timeout | → `Idle` |

#### Autómata de Tapado (`CappingState`)

| Estado Origen | Fase Entry | Condición de Salida (During) | Timeout de Guardia | Fallo / Siguiente Estado |
| :---- | :---- | :---- | :---- | :---- |
| `Idle` | Brazos arriba, motor DC detenido; vigilar entrada. | `presenceCap.isTriggered() && !needsClear_` | Sin timeout | → `StopArmDown` |
| `StopArmDown` | Bajar brazo de tope (la botella se asienta contra el tope). | `millis() - stageTimer_ >= CLAMP_DELAY_MS` | Sin timeout | → `ClampArmDown` |
| `ClampArmDown` | Bajar brazo de sujeción. | `millis() - stageTimer_ >= ARM_MOVE_MS` y `presenceCap.isTriggered()` | `ARM_MOVE_MS + 500 ms` | Presencia perdida: `ERR-102` → `ErrorStop` |
| `LoweringHead` | Mover eje Z a `profile.capZSteps`. | `capZ.isTargetReached()` | 5000 ms | `ERR-213` → `ErrorStop` (soft limit: `ERR-212`) |
| `TighteningCap` | Encender motor DC con goma de apriete. | `millis() - stageTimer_ >= TIGHTEN_MS` | No aplica (salida por tiempo; fallos durante la fase) | Corriente excesiva: `ERR-301`; bloqueo prematuro: `ERR-302` → `ErrorStop` |
| `RaisingHead` | Frenar motor DC; subir eje Z hacia fin de carrera. | `limitSwitchCap.isTriggered()` | 5000 ms | `ERR-214` → `ErrorStop` |
| `ReleasingBottle` | `bottleCount++`; si `interstageQueueCount_ > 0` entonces `interstageQueueCount_--`; subir brazo de sujeción; tras `RELEASE_GAP_MS`, subir el de tope; activar `needsClear_`. | `millis() - stageTimer_ >= RELEASE_MS` | Sin timeout | → `Idle` |

### 5.5 Parámetros de Proceso (`process_params.hpp`)

Archivo exclusivo para **cantidades**: pasos, velocidades, tiempos, ángulos, umbrales y calibración (los pines viven en `hardware_pins.hpp`, §7.2). Los valores son **iniciales y deben calibrarse** durante la puesta en marcha; los marcados `AJUSTAR` dependen del hardware real.

```cpp
#pragma once
#include <Arduino.h>

namespace ProcessParams
{
    // ==========================================
    // COLA INTERMEDIA ENTRE AUTÓMATAS (Buffer WIP Llenado -> Tapado)
    // ==========================================
    constexpr uint8_t  MAX_INTERSTAGE_QUEUE    = 2;     // Máximo de botellas en tránsito/espera hacia Tapado (AJUSTAR)

    // ==========================================
    // EJES Z (motores a pasos de llenado y tapado)
    // ==========================================
    constexpr uint16_t Z_STEPS_PER_MM          = 400;   // 200 pasos/rev x 16 micropasos / 8 mm de avance (AJUSTAR)
    constexpr uint16_t Z_CRUISE_SPEED          = 4000;  // Velocidad de crucero [step/s] (máx. 8000)
    constexpr uint16_t Z_HOMING_SPEED          = 1000;  // Búsqueda del fin de carrera [step/s]
    constexpr uint16_t Z_ACCEL                 = 8000;  // Aceleración [step/s^2]
    constexpr uint16_t Z_HOME_BACKOFF_STEPS    = 200;   // Retroceso tras tocar el fin de carrera [step]
    constexpr uint16_t Z_JOG_STEPS             = 400;   // Paso de jog en ManualRecovery [step]
    constexpr int32_t  FILL_Z_MAX_TRAVEL_STEPS = 24000; // Soft limit eje de llenado [step] (AJUSTAR)
    constexpr int32_t  CAP_Z_MAX_TRAVEL_STEPS  = 24000; // Soft limit eje de tapado [step] (AJUSTAR)
    constexpr uint16_t Z_MOVE_TIMEOUT_MS       = 5000;  // Guardia de bajada / subida [ms]
    constexpr uint16_t Z_HOMING_TIMEOUT_MS     = 10000; // Guardia de Boot [ms]

    // ==========================================
    // BANDA TRANSPORTADORA (motor 24 V + encoder)
    // ==========================================
    constexpr uint16_t BELT_TARGET_RPM         = 60;    // Consigna del lazo PI [RPM] (AJUSTAR)
    constexpr uint16_t BELT_ENCODER_PPR        = 600;   // Pulsos por vuelta del encoder (AJUSTAR)
    constexpr uint8_t  BELT_PI_KP              = 20;    // Ganancia proporcional (punto fijo, /10)
    constexpr uint8_t  BELT_PI_KI              = 5;     // Ganancia integral (punto fijo, /10)
    constexpr uint16_t BELT_STALL_MS           = 500;   // PWM aplicado sin pulsos (ERR-303)
    constexpr uint8_t  BELT_STALL_MIN_PWM      = 40;    // PWM mínimo (0-255) para evaluar atasco

    // ==========================================
    // BRAZOS CON SERVOMOTOR (ángulos en grados, tiempos en ms)
    // ==========================================
    constexpr uint8_t  FILL_STOP_UP_DEG        = 10;    // Brazo de tope - llenado (AJUSTAR)
    constexpr uint8_t  FILL_STOP_DOWN_DEG      = 100;
    constexpr uint8_t  FILL_CLAMP_UP_DEG       = 10;    // Brazo de sujeción - llenado (AJUSTAR)
    constexpr uint8_t  FILL_CLAMP_DOWN_DEG     = 95;
    constexpr uint8_t  CAP_STOP_UP_DEG         = 10;    // Brazo de tope - tapado (AJUSTAR)
    constexpr uint8_t  CAP_STOP_DOWN_DEG       = 100;
    constexpr uint8_t  CAP_CLAMP_UP_DEG        = 10;    // Brazo de sujeción - tapado (AJUSTAR)
    constexpr uint8_t  CAP_CLAMP_DOWN_DEG      = 95;
    constexpr uint16_t CLAMP_DELAY_MS          = 1000;  // Espera entre bajar el tope y bajar la sujeción
    constexpr uint16_t ARM_MOVE_MS             = 600;   // Recorrido de un servo sin retroalimentación
    constexpr uint16_t RELEASE_GAP_MS          = 500;   // Entre subir sujeción y subir tope
    constexpr uint16_t RELEASE_MS              = 1500;  // Hasta que la botella libera el sensor

    // ==========================================
    // LLENADO (bomba)
    // ==========================================
    constexpr uint16_t DRIP_MS                 = 1000;  // Goteo con bomba apagada
    constexpr uint16_t MAX_PUMP_ON_MS          = 8000;  // Límite absoluto de bomba encendida (FillPump)

    // ==========================================
    // TAPADO (motor DC del cabezal)
    // ==========================================
    constexpr uint16_t TIGHTEN_MS              = 2500;  // Tiempo de apriete [ms]
    constexpr uint8_t  CHUCK_PWM               = 200;   // PWM de apriete (0-255) (AJUSTAR)
    constexpr uint16_t CHUCK_CURRENT_LIMIT_ADC = 600;   // Umbral ADC (0-1023) de sobrecorriente (AJUSTAR)
    constexpr uint16_t CHUCK_STALL_MS          = 150;   // PWM activo sin pulsos del encoder (ERR-302)

    // ==========================================
    // PANTALLA TÁCTIL (calibrar con setRotation(1) ya aplicada)
    // ==========================================
    constexpr int16_t  TS_MINX                 = 120;   // AJUSTAR con TouchScreen_Calibr_native
    constexpr int16_t  TS_MAXX                 = 900;
    constexpr int16_t  TS_MINY                 = 70;
    constexpr int16_t  TS_MAXY                 = 920;
    constexpr int16_t  TS_MIN_PRESSURE         = 200;
    constexpr int16_t  TS_MAX_PRESSURE         = 1000;

    /**
     * @brief Parámetros dependientes del tamaño de botella.
     */
    struct BottleProfileParams
    {
        int32_t  fillZSteps; // Cota de llenado desde home [step]
        int32_t  capZSteps;  // Cota de tapado desde home [step]
        uint16_t fillMs;     // Tiempo de bombeo [ms]
    };

    // Índice = static_cast<uint8_t>(BottleProfile)
    constexpr BottleProfileParams BOTTLE_PROFILES[] =
    {
        { 0, 0, 0 }, // Small: calibrar
        { 0, 0, 0 }  // Large: calibrar
    };
}
```

## 6. Concurrencia Cooperativa en un Solo Núcleo

Para evitar pérdidas de pasos en los motores y retardos en los autómatas provocados por el dibujo en la pantalla TFT y la pila de red, la ejecución se reparte entre **interrupciones de hardware** (tiempo real duro) y un **planificador cooperativo** con periodos fijos (tiempo real blando):

```text
                ┌──────────────────────────────────────────┐
                │        ATmega2560 (núcleo único)         │
                └──────────────────────────────────────────┘
                                     │
          ┌──────────────────────────┴──────────────────────────┐
          ▼                                                     ▼
   ┌──────────────────┐                               ┌───────────────────┐
   │  INTERRUPCIONES  │                               │    SUPER-LOOP     │
   │ (prioridad HW)   │                               │   (cooperativo)   │
   └────────┬─────────┘                               └─────────┬─────────┘
            │                                                   │
            ├─ Timer3 COMPA @ 20 kHz: pulsos STEP (Z llenado    ├─ cada 1 ms   : Sensores + 2 autómatas
            │   + Z tapado) y vigilancia de límites             ├─ cada 20 ms  : Rampas Z + PI de banda
            ├─ INT0/INT1 (21/20): encoder cabezal de apriete    ├─ cada 50 ms  : TFT Dirty-Region + Touch
            ├─ INT2/INT3 (19/18): encoder de la banda           ├─ cada 10 ms  : Ethernet + MQTT loop
            └─ Timer0: millis() (núcleo Arduino)                └─ cada 500 ms : Telemetría JSON
            │                                                   │
            └──────────► [ variables volatile + ATOMIC_BLOCK ] ◄┘
```

### 6.1 Reglas de Concurrencia

1. **ISRs mínimas:** Una ISR solo actualiza contadores, vigila límites y conmuta pines; nunca llama a `Serial`, `millis()` en bucle ni asigna memoria. Objetivo: < 10 µs por ISR.
2. **Datos compartidos:** Toda variable escrita en una ISR y leída en el super-loop se declara `volatile`. Si su tamaño supera 1 byte, se lee dentro de `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` (`<util/atomic.h>`).
3. **Red no bloqueante y presupuesto de tiempo:** `PubSubClient::connect()` puede bloquear hasta 15 s con su timeout por defecto. Se configuran `client.setSocketTimeout(1)` (1 s) y `Ethernet.setRetransmissionTimeout(200)` / `Ethernet.setRetransmissionCount(2)`. La reconexión se intenta como máximo cada 5 s y **ninguna llamada individual del super-loop debe superar ~1.5 s**. Mientras no haya red, la estación sigue operando en modo local.
4. **Watchdog:** Se habilita el watchdog de hardware con `wdt_enable(WDTO_4S)` (periodo superior al peor caso de una reconexión de red o de un redibujado completo de la TFT) y se alimenta (`wdt_reset()`) una vez por vuelta del super-loop, además de justo antes de cada intento de reconexión MQTT. **Condición previa:** superar la prueba de aceptación del bootloader (§2.1).
5. **Seguridad ante bloqueo del super-loop:** las ISR de Timer3 siguen emitiendo pulsos aunque el super-loop se bloquee. Por eso los límites de carrera y los *soft limits* se vigilan **dentro de la ISR** (§4.1, `StepperAxis`), y la seta E-STOP corta la potencia por hardware (§9.4).

### 6.2 Implementación del Planificador y Punto de Entrada

```cpp
#include <Arduino.h>
#include <avr/wdt.h>
#include "station/station_controller.hpp"
#include "scada_client.hpp"
#include "display_manager.hpp"

StationController station;
ScadaClient scadaClient;
DisplayManager display; // TFT 2.4" 320x240 ILI9341 + Touch resistivo

/**
 * @brief Tarea periódica cooperativa basada en millis().
 */
struct PeriodicTask
{
    uint16_t periodMs;
    uint32_t lastRunMs;
    void (*run)();
};

static void runControl()
{
    station.update(); // Modo global + FillingAutomaton + CappingAutomaton + gobierno de la banda
}

static void runMotionProfiles()
{
    station.updateMotionProfiles(); // Rampas de los ejes Z y lazo PI de la banda
}

static void runDisplayAndTouch()
{
    // 1. Muestrear panel táctil y despachar comando si el operador pulsó un botón
    const TouchCommand cmd = display.pollTouchCommand();
    if (cmd != TouchCommand::None)
    {
        station.handleTouchCommand(cmd); // Descarta el comando si la seta E-STOP sigue activa
    }

    // 2. Actualizar vista si la estación publicó un cambio de estado
    DisplaySnapshot snapshot;
    if (station.consumeDisplaySnapshot(snapshot))
    {
        display.render(snapshot);
    }
    display.update(); // Redibuja únicamente las regiones marcadas como modificadas
}

static void runNetwork()
{
    scadaClient.update();
}

static void runTelemetry()
{
    scadaClient.publishTelemetry(station.getTelemetry());
}

static PeriodicTask tasks[] =
{
    {   1, 0, runControl         },
    {  20, 0, runMotionProfiles  },
    {  50, 0, runDisplayAndTouch },
    {  10, 0, runNetwork         },
    { 500, 0, runTelemetry       }
};

void setup()
{
    Serial.begin(115200);

    display.init();
    display.showBoot();

    station.init();          // Configura pines, Timer3 e interrupciones externas
    scadaClient.init();      // Ethernet + MQTT (no bloqueante)

    wdt_enable(WDTO_4S);     // Solo tras superar la prueba de bootloader (§2.1)
}

void loop()
{
    const uint32_t nowMs = millis();

    for (PeriodicTask& task : tasks)
    {
        if (nowMs - task.lastRunMs >= task.periodMs)
        {
            task.lastRunMs = nowMs;
            task.run();
        }
    }

    wdt_reset();
}
```

### 6.3 Asignación de Timers de Hardware

Como el bus paralelo de 8 bits de la pantalla TFT ocupa los pines digitales `2-9`, las salidas PWM de **Timer3** (`2, 3, 5`) y **Timer4** (`6, 7, 8`) ya están dedicadas a la pantalla como GPIO. Por ello se asigna **Timer3** a la ISR de pasos (sin perder ningún pin PWM libre) y se destinan **Timer1** y **Timer2** a los puentes H BTS7960:

| Timer | Uso | Pines PWM afectados |
| :---- | :---- | :---- |
| **Timer0** (8 bits) | `millis()`, `micros()` del núcleo Arduino. **No modificar.** | 4 (ocupado por TFT `LCD_D4`), 13 (libre, sin PWM) |
| **Timer1** (16 bits) | PWM del BTS7960 de apriete (`RPWM` y `LPWM`, 490 Hz). | **11, 12 (PWM activo)** |
| **Timer2** (8 bits) | PWM del BTS7960 de la banda (`RPWM`, 490 Hz). | 9 (ocupado por TFT `LCD_D1`), **10 (PWM activo)** |
| **Timer3** (16 bits) | **ISR de generación de pulsos STEP @ 20 kHz (`TIMER3_COMPA_vect`).** | 2, 3, 5 (usados como GPIO por el bus de datos TFT) |
| **Timer4** (16 bits) | Libre (para temporizaciones auxiliares si se requiere). | 6, 7, 8 (usados como GPIO por el bus de datos TFT) |
| **Timer5** (16 bits) | Reservado por la librería `Servo` (4 servos). | 44, 45, 46 (PWM inhabilitado) |

## 7. Acondicionamiento Eléctrico y Mapeo de Pines (Mega 2560 @ 5 V)

### 7.1 Reglas Críticas de Acondicionamiento Eléctrico

1. **Niveles Lógicos de 5 V:** Los pines del ATmega2560 operan a 5 V y son compatibles directamente con TB6600, BTS7960, servomotores, el shield TFT 2.4" ILI9341 (que incorpora buffers 74HC245 en placa) y la mayoría de sensores IR de 5 V. **No se requieren desplazadores de nivel** para estos dispositivos.
   * Sensores inductivos / ópticos industriales alimentados a 24 V deben aislarse mediante optoacopladores (PC817 o similar) con salida a 5 V.
   * **Encoder de la banda:** verificar la tensión de alimentación y el tipo de salida del encoder del motor de 24 V. Si trabaja a 5 V (colector abierto o *push-pull*), se conecta directo (con pull-up de 4.7 kΩ a 5 V si es colector abierto). Si trabaja a 12-24 V, aislar las dos señales con optoacopladores rápidos (p. ej. 6N137) o un divisor con diodo Zener antes de los pines 18/19.
   * El módulo W5500 opera a 3.3 V pero sus entradas toleran 5 V según la hoja de datos de WIZnet, por lo que se conecta directamente al SPI del Mega.
   * Cualquier otro periférico de 3.3 V que se agregue a futuro **sí requiere** un desplazador de nivel en sus entradas, salvo que su hoja de datos declare tolerancia a 5 V.

2. **Prohibición de Redes Analógicas Resistivas en Sensores:**
   * Queda estrictamente descartado agrupar sensores sobre un único cable analógico mediante escalera de resistencias o divisores de tensión pasivos. En una máquina de manufactura continua:
     * Múltiples sensores pueden activarse al mismo tiempo (ej. botella en llenado mientras otra está en tapado), generando combinaciones en paralelo con caídas de tensión ambiguas.
     * La conmutación de motores de 24 V induce ruido electromagnético (EMI) de ±100 mV, lo que falsearía lecturas analógicas continuas.
     * Los sensores ópticos IR son dispositivos activos (transistores con salida lógica NPN/PNP) y no resistencias pasivas.
     * Cada conversión del ADC del ATmega2560 tarda ~112 µs, latencia que compromete el frenado en finales de carrera.
   * Por tanto, **cada sensor dispone de su propia línea digital discreta**. El Mega dispone de 54 pines digitales, por lo que no se prevé la necesidad de expansores.

3. **Alimentación:**
   * El regulador lineal de 5 V del Mega solo entrega ~500 mA de forma segura con 9 V en `VIN` (menos con 12 V por disipación). **Los cuatro servomotores y la pantalla TFT 2.4" (~80–120 mA con retroiluminación activa) deben alimentarse desde una fuente de 5 V externa** con tierra común al Mega. Dimensionar la fuente para el pico de arranque de los servos y colocar un capacitor de 470–1000 µF cerca de su alimentación.
   * **Módulo W5500:** consume hasta ~150 mA a 3.3 V, mientras que el pin `3.3V` del Mega solo entrega **50 mA**. Si el módulo tiene pin `5V` (con regulador integrado), alimentarlo desde la fuente externa de 5 V; si solo tiene pin `3.3V`, usar un regulador dedicado (AMS1117-3.3 o similar) alimentado desde la fuente de 5 V, con capacitores de 10 µF a la entrada y salida.
   * **BTS7960 de la banda (24 V):** el módulo admite hasta ~27 V de alimentación del motor; colocar un capacitor electrolítico de ≥ 1000 µF / 50 V cerca de sus bornes de potencia y verificar que la corriente de arranque del motor quede dentro de la capacidad del disipador.
   * La potencia de 24 V para drivers de motores se lleva por un circuito separado; solo se comparte la referencia GND en un único punto (topología estrella).

4. **Pantalla TFT 2.4" Touch (Formato Shield conectado por cables):**
   * Al montar el shield separado del Mega mediante jumpers, el pulso `LCD_WR` conmuta a varios MHz. Mantener el mazo de cables **por debajo de 20 cm** (preferiblemente cinta plana Dupont unida) e incluir **al menos dos hilos de GND** entre el Mega y la pantalla para evitar píxeles corruptos o pantalla en blanco.
   * **Cableado 1 a 1 (compatible de fábrica con `MCUFRIEND_kbv` sin editar la librería):**
     * **Control (`A0-A4`):** `LCD_RD` → `A0`, `LCD_WR` → `A1`, `LCD_RS` → `A2`, `LCD_CS` → `A3`, `LCD_RST` → `A4`.
     * **Datos 8 bits (`D2-D9`):** `LCD_D0` → `8`, `LCD_D1` → `9`, `LCD_D2` → `2`, `LCD_D3` → `3`, `LCD_D4` → `4`, `LCD_D5` → `5`, `LCD_D6` → `6`, `LCD_D7` → `7`.
     * **Alimentación:** conectar `5V`, `3.3V` y `GND` del shield.
     * **Pines `D10-D13` del shield (lector microSD): NO CONECTAR.** Dejarlos sin cablear para que los pines `10, 11, 12` del Mega queden libres para las salidas PWM de los puentes H BTS7960.
   * **Panel táctil resistivo integrado:** utiliza 4 líneas compartidas con el propio bus de la pantalla (típicamente `XP = 8`, `XM = A2`, `YP = A3`, `YM = 9` o `XP = 6`, `XM = A2`, `YP = A1`, `YM = 7` según la revisión del PCB; se verifica una sola vez con el sketch `TouchScreen_Calibr_native` de `MCUFRIEND_kbv`; los pines se fijan en `hardware_pins.hpp` y los valores de calibración en `process_params.hpp`). No requiere cables adicionales.

5. **Excitación de Actuadores:**
   * **Drivers TB6600:** Conectar `PUL+`, `DIR+`, `ENA+` a 5 V y modular `PUL-`, `DIR-`, `ENA-` desde los GPIOs (configuración de ánodo común), con corriente nominal para los optoacopladores internos.
   * **BTS7960:** Entradas `RPWM` / `LPWM` / `R_EN` / `L_EN` compatibles directamente a 5 V. En el BTS7960 de la banda (un solo sentido) `LPWM` se conecta a GND.
   * **Bomba de llenado:** no se conecta directamente al GPIO. Se acciona mediante un módulo de relé con optoacoplador, o un MOSFET de nivel lógico con diodo de rueda libre (p. ej. 1N5819) si la carga es DC. Una resistencia pull-down de 10 kΩ en la entrada garantiza que la bomba permanezca **apagada** durante el arranque y el bootloader.
   * **Servomotores (4):** señal directa desde el GPIO con alimentación externa de 5 V (ver Alimentación); la `GND` de la fuente se une a la del Mega.

6. **Pines Reservados (no asignar):**
   * **0 y 1:** `Serial` (USB, programación y terminal de diagnóstico).
   * **2 a 9 y A0 a A4:** Bus paralelo de 8 bits y control de la pantalla TFT 2.4" + panel táctil.
   * **50, 51, 52:** Bus SPI (MISO, MOSI, SCK) del módulo W5500.
   * **53:** Chip Select del W5500 (SS de hardware).
   * **49:** Reset del W5500.

7. **Estado de los Pines Durante el Arranque:** Al energizar o durante el bootloader (~1-2 s), todos los pines quedan en alta impedancia. Las líneas `EN` de los drivers y la entrada de la bomba deben llevar resistencias externas que mantengan los actuadores **deshabilitados** hasta que el firmware tome el control.

### 7.2 Asignación Centralizada de Pines (`hardware_pins.hpp`)

Este archivo contiene **únicamente pines** (ver §1.5). Las cantidades de pasos, tiempos y ángulos están en `process_params.hpp` (§5.5).

```cpp
#pragma once
#include <Arduino.h>

namespace HardwarePins
{
    // ==========================================
    // RESERVADOS: USB y Módulo Ethernet W5500 (SPI)
    // ==========================================
    // 0, 1     : Serial USB
    // 50 - 52  : SPI (MISO, MOSI, SCK) del módulo W5500
    constexpr uint8_t PIN_ETH_CS            = 53; // SS de hardware -> SCS del W5500
    constexpr uint8_t PIN_ETH_RST           = 49; // RST del W5500 (activo en bajo)

    // ==========================================
    // PANTALLA TFT 2.4" ILI9341 (Bus Paralelo 8 bits + Touch Resistivo)
    // ==========================================
    constexpr uint8_t PIN_TFT_RD            = A0;
    constexpr uint8_t PIN_TFT_WR            = A1;
    constexpr uint8_t PIN_TFT_RS            = A2; // Compartido con Touch XM
    constexpr uint8_t PIN_TFT_CS            = A3; // Compartido con Touch YP
    constexpr uint8_t PIN_TFT_RST           = A4;
    // Bus de datos paralelo de 8 bits (cableado 1 a 1 con el shield):
    // LCD_D0=8, LCD_D1=9, LCD_D2=2, LCD_D3=3, LCD_D4=4, LCD_D5=5, LCD_D6=6, LCD_D7=7
    // Pines del panel táctil (ajustar según calibración de MCUFRIEND_kbv):
    constexpr uint8_t PIN_TOUCH_XP          = 8;
    constexpr uint8_t PIN_TOUCH_XM          = A2;
    constexpr uint8_t PIN_TOUCH_YP          = A3;
    constexpr uint8_t PIN_TOUCH_YM          = 9;

    // ==========================================
    // ENTRADAS: Encoders de Cuadratura (Interrupción Externa en PORTD)
    // ==========================================
    constexpr uint8_t PIN_ENC_CHUCK_A       = 21; // INT0 (Encoder cabezal apriete canal A)
    constexpr uint8_t PIN_ENC_CHUCK_B       = 20; // INT1 (Encoder cabezal apriete canal B)
    constexpr uint8_t PIN_ENC_CONV_A        = 19; // INT2 (Encoder banda canal A; inhabilita Serial1)
    constexpr uint8_t PIN_ENC_CONV_B        = 18; // INT3 (Encoder banda canal B; inhabilita Serial1)

    // ==========================================
    // ENTRADAS: Sensores Digitales (INPUT_PULLUP, PORTA)
    // ==========================================
    constexpr uint8_t PIN_PRESENCE_FILL     = 22; // PA0: Presencia de botella en autómata de llenado
    constexpr uint8_t PIN_PRESENCE_CAP      = 23; // PA1: Presencia de botella en autómata de tapado
    constexpr uint8_t PIN_LIMIT_SWITCH_FILL = 24; // PA2: Fin de carrera superior eje Z de llenado (ISR)
    constexpr uint8_t PIN_LIMIT_SWITCH_CAP  = 25; // PA3: Fin de carrera superior eje Z de tapado (ISR)
    constexpr uint8_t PIN_ESTOP_AUX         = 27; // Contacto auxiliar NC de la seta E-STOP
    constexpr uint8_t PIN_BTN_RESET         = 28; // Pulsador físico de rearme (en paralelo al touch)

    // ==========================================
    // ENTRADA ANALÓGICA: Sensado de Corriente (A0-A4 ocupados por TFT)
    // ==========================================
    constexpr uint8_t PIN_ADC_CHUCK_CURRENT = A8; // IS_R / IS_L del BTS7960 de apriete

    // ==========================================
    // SALIDAS: Servomotores de Brazos (librería Servo, Timer5)
    // ==========================================
    constexpr uint8_t PIN_SERVO_FILL_STOP   = 44; // Brazo de tope - llenado
    constexpr uint8_t PIN_SERVO_FILL_CLAMP  = 45; // Brazo de sujeción - llenado
    constexpr uint8_t PIN_SERVO_CAP_STOP    = 46; // Brazo de tope - tapado
    constexpr uint8_t PIN_SERVO_CAP_CLAMP   = 47; // Brazo de sujeción - tapado

    // ==========================================
    // SALIDA: Bomba de Llenado ON/OFF (vía relé u MOSFET; pull-down externo)
    // ==========================================
    constexpr uint8_t PIN_PUMP_FILL         = 43;

    // ==========================================
    // SALIDAS: Motor a Pasos - Eje Z de Llenado - PORTC
    // ==========================================
    constexpr uint8_t PIN_STEPPER_FILL_STEP = 37; // PC0 (escritura directa en ISR de Timer3)
    constexpr uint8_t PIN_STEPPER_FILL_DIR  = 36; // PC1
    constexpr uint8_t PIN_STEPPER_FILL_EN   = 35; // PC2

    // ==========================================
    // SALIDAS: Motor a Pasos - Eje Z de Tapado - PORTC
    // ==========================================
    constexpr uint8_t PIN_STEPPER_CAP_STEP  = 34; // PC3 (escritura directa en ISR de Timer3)
    constexpr uint8_t PIN_STEPPER_CAP_DIR   = 33; // PC4
    constexpr uint8_t PIN_STEPPER_CAP_EN    = 32; // PC5

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Apriete de Tapa) - Timer1 PWM
    // ==========================================
    constexpr uint8_t PIN_BTS_CHUCK_RPWM    = 11; // OC1A (490 Hz)
    constexpr uint8_t PIN_BTS_CHUCK_LPWM    = 12; // OC1B (490 Hz)
    constexpr uint8_t PIN_BTS_CHUCK_EN      = 40; // R_EN y L_EN unificados

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Banda 24 V, un sentido) - Timer2 PWM
    // ==========================================
    constexpr uint8_t PIN_BTS_CONV_RPWM     = 10; // OC2A (490 Hz); LPWM del módulo a GND
    constexpr uint8_t PIN_BTS_CONV_EN       = 41; // R_EN y L_EN unificados

    // ==========================================
    // SALIDA: Indicador de Estado en Placa / Torreta
    // ==========================================
    constexpr uint8_t PIN_STATUS_LED        = 42;
}
```

> Los pines STEP/DIR/EN de ambos ejes Z se agrupan en **PORTC**, y los servomotores se ubican fuera de ese puerto (`PORTL`), para que la ISR del Timer3 emita los pulsos de los dos motores con una sola escritura de registro (`PORTC |= mask`) sin condiciones de carrera con la librería `Servo`. Los dos fines de carrera ocupan `PORTA` (`PA2` / `PA3`) para su lectura directa con `PINA` desde la ISR. Los cuatro canales de encoder ocupan `PORTD` (`PD0-PD3`) para su lectura con `PIND`.

## 8. Estructura de Directorios

Organización modular estandarizada para PlatformIO. Las librerías se ubican en un solo nivel dentro de `lib/`, ya que el *Library Dependency Finder* de PlatformIO no explora subcarpetas anidadas:

```text
station3_filling_capping/
├── platformio.ini
├── ARCHITECTURE.md
├── Doxyfile
├── include/
│   ├── config/
│   │   ├── hardware_pins.hpp        # SOLO pines del Mega 2560 (cableado)
│   │   └── process_params.hpp       # Pasos, velocidades, tiempos, ángulos, perfiles, calibración
│   └── types/
│       └── station_types.hpp        # Enums de modo, autómatas, errores, comandos táctiles y snapshot
├── lib/
│   ├── DigitalSensor/               # HAL: filtro de debounce y detección de flancos
│   ├── DcMotorBts7960/              # HAL: control PWM con frenado dinámico
│   ├── InterruptEncoder/            # HAL: decodificación de cuadratura por INT0-INT3 (2 instancias)
│   ├── StepperAxis/                 # HAL: rampas, soft limits e ISR de Timer3 con vigilancia de límites
│   ├── BottleGate/                  # Controlador: brazos de tope y sujeción (2 servos)
│   ├── FillPump/                    # Controlador: bomba ON/OFF con límite de encendido
│   ├── ConveyorController/          # Controlador: banda 24 V con lazo PI, paro y atasco por encoder
│   ├── DisplayManager/              # HAL: TFT 2.4" ILI9341 (8 bits) + Touch resistivo
│   └── ScadaClient/                 # Red: Ethernet W5500 + MQTT + JSON para Node-RED
└── src/
    ├── station/
    │   ├── station_controller.hpp   # Clase Mediadora: modo global, banda, perfil y filtro de comandos
    │   ├── station_controller.cpp
    │   ├── filling_automaton.hpp    # FSM de 3 fases del autómata de llenado
    │   ├── filling_automaton.cpp
    │   ├── capping_automaton.hpp    # FSM de 3 fases del autómata de tapado
    │   └── capping_automaton.cpp
    └── main.cpp                     # Planificador cooperativo y watchdog
```

## 9. Roadmap de Mejoras a Futuro, Posibles Errores y Soluciones Técnicas

Esta sección consolida las optimizaciones de ingeniería mecatrónica y de software identificadas tras el diseño preliminar. Se documentan como mejoras para la fase de endurecimiento (*hardening*) posterior a la validación del prototipo funcional.

### 9.1 Control de Apriete de Tapa: Detección de Torque / Stall vs. Temporización Ciega

* **Problema Potencial:** El apriete regulado únicamente por tiempo ciego (X s) no absorbe tolerancias mecánicas ni roscas desalineadas. Si la tapa aprieta antes de lo estimado, el motor continuará girando hasta barrer la rosca plástica o entrar en condición de bloqueo (*stall*), demandando picos de corriente elevados al BTS7960. Si el envase entra flojo, el tiempo puede expirar sin garantizar sellado hermético.

* **Solución a Futuro:**

  1. *Sensado de Corriente (Hardware BTS7960):* Monitorear el pin analógico `IS` mediante el ADC de 10 bits (canal `A8`), con un filtro de media móvil sobre 8 muestras. Al rebasar el umbral calibrado de corriente admisible (torque de cierre óptimo), ordenar de forma inmediata el frenado dinámico del motor.

  2. *Detección de Bloqueo por Encoder:* Leer el incremento de pulsos del `InterruptEncoder` del cabezal. Si la modulación PWM está activa pero el conteo de pulsos permanece estático por más de `CHUCK_STALL_MS` (150 ms), inferir tope mecánico de rosca y frenar el cabezal (`brake()`).

### 9.2 Timeouts de Fase en las FSM y Códigos de Error para Operador

* **Problema Potencial:** Si un sensor IR se descalibra, una botella se atasca en la banda o un actuador no completa su recorrido físico, un autómata quedaría aguardando indefinidamente en la fase `During`, provocando un bloqueo silencioso de la estación y deteniendo la línea de manufactura completa.

* **Estrategia:**

  * Cada estado evalúa activamente su temporizador de guardia contra las matrices de tiempos límite (§5.4).

  * Al expirar el tiempo de guardia, el autómata asigna el código correspondiente de la serie `100s` / `200s` / `300s` al evento, detiene los actuadores y el mediador transiciona **toda la estación** al modo seguro `ErrorStop`.

  * `StationController` publica una nueva `DisplaySnapshot` que la tarea de display proyecta en la pantalla TFT y la tarea de red publica en el SCADA.

### 9.3 Protección de Carrera de los Ejes Z (Límites por Software / Soft Limits)

* **Problema Potencial:** Aunque cada husillo vertical ejecuta un *homing* exitoso contra su fin de carrera superior y calcula la carrera descendente por pasos fijos, la presencia de envases con cotas fuera de norma (o un perfil de botella equivocado) puede provocar que el eje golpee un tope rígido, dañando el acoplamiento mecánico o deformando el eje roscado.

* **Solución a Futuro:**

  * Implementar verificación de cotas virtuales (*Soft Limits*) dentro de la clase `StepperAxis` **y evaluarlas en la ISR del Timer3**: denegar y detener cualquier desplazamiento que sobrepase el límite máximo de diseño (`FILL_Z_MAX_TRAVEL_STEPS` / `CAP_Z_MAX_TRAVEL_STEPS`), disparando la alarma `ERR-202` (llenado) o `ERR-212` (tapado).

  * Como salvaguarda física complementaria, intercalar un resorte de compensación mecánica con un microswitch de contacto en el cabezal para absorber sobrecarreras de impacto sin forzar la mecánica.

### 9.4 Parada de Emergencia (E-STOP) por Hardware y Procedimiento de Rearme Seguro

* **Problema Potencial:** Una orden de parada originada exclusivamente por software (vía SCADA o FSM) no satisface los estándares de seguridad ante cuelgues del procesador, bloqueos del bus o fallas en el firmware.

* **Solución a Futuro:**

  * **Corte Eléctrico Directo de Potencia:** La seta de emergencia física debe desconectar físicamente la línea principal de 24 V hacia los drivers de potencia (BTS7960 de la banda y del cabezal, y TB6600), el motor de la banda y la bomba de llenado mediante un relé o contactor de seguridad con enclavamiento, o abriendo por hardware las líneas de habilitación general (`ENABLE`).

  * **Notificación al Microcontrolador:** Un contacto auxiliar NC de la seta (pin 27) informa al Mega para conmutar la estación al modo `ErrorStop` (`ERR-901`) y proyectar el estado en pantalla.

  * **Interbloqueo del Rearme:** mientras el contacto auxiliar indique la seta activa, el firmware ignora el pulsador físico `PIN_BTN_RESET` y todos los comandos táctiles (§3.3).

  * **Modo de Recuperación Manual (*Manual Recovery*):** Tras restablecer la seta de seguridad, el sistema no debe ejecutar un *homing* automático ciego; si un cabezal quedó enganchado en el cuello de una botella, subir repentinamente causaría roturas mecánicas. La estación transicionará a `ManualRecovery`, exigiendo al operario liberar el área y validar el movimiento eje por eje (y la liberación de los brazos) antes de habilitar el ciclo automático.

### 9.5 Presupuesto de Memoria SRAM

* **Problema Potencial:** Con 8 KB de SRAM compartidos entre pila, variables globales, `PubSubClient` (búfer de 256 B por defecto), documentos JSON (`StaticJsonDocument<256>`) y las librerías gráficas, un desbordamiento de pila corrompe datos silenciosamente. Los búferes de transmisión y recepción de Ethernet **residen en el propio chip W5500 (16 KB)** y no consumen SRAM del Mega; la librería `Ethernet` solo usa unos cientos de bytes.

* **Solución:**

  * Revisar el reporte `RAM:` de cada compilación de PlatformIO; mantener el uso estático **por debajo del 70 %**.
  * Instrumentar una función `freeMemory()` y publicarla en la telemetría durante la validación.
  * Mantener todos los textos en Flash (`F()`, `PROGMEM`) y prohibir `String`.

## 10. Notas Pendientes y Supuestos por Confirmar

### 10.1 Supuestos del Rediseño Lineal (confirmar con la estación real)

1. **Banda continua:** en modo `Running`, la banda gira de forma ininterrumpida; cuando un autómata baja su brazo de tope, la botella retenida resbala sobre la banda y, tras `CLAMP_DELAY_MS` (1 s), baja el brazo de sujeción. Esto permite que ambos autómatas trabajen de forma totalmente independiente y simultánea.
2. **Perfil de botella manual:** el tamaño (`Small` / `Large`) se elige por pantalla táctil. Alternativa: detección automática con un sensor de altura.
3. **Contador de producción:** `bottleCount` incrementa al terminar el tapado; no se persiste en EEPROM.
4. **Servos sin retroalimentación:** las posiciones de los brazos se confían a tiempos fijos; no hay códigos de error de brazo atascado.
5. **Presencia al liberar:** si las botellas llegan pegadas (sin espacio entre ellas), el sensor nunca se libera y `needsClear_` impide un nuevo ciclo. Requiere separación entre botellas.
6. **Valores de `process_params.hpp`:** son iniciales; las cotas Z y el tiempo de llenado de cada perfil están en cero y **deben calibrarse** antes de la primera prueba con botellas.

### 10.2 Hallazgos de la Revisión Aún No Aplicados

* **Pantalla y Lopaka:**
  * Fijar fuente y tamaño de texto definitivos; la fuente 5x7 por defecto no tiene tildes ni `ñ` (usar fuente GFX personalizada si se requieren).
  * Con el perfil *Adafruit GFX Color*, Lopaka exporta RGB565 (`drawRGBBitmap`) y `tft.fillScreen()`: desactivar *Clear/Fill Display* y verificar que los arrays salgan con `PROGMEM`. Una función de dibujo por vista.
  * Confirmar con el esquema de la placa si el shield trae buffers en el bus de datos y si debe conectarse su pin `3.3V` (en varios modelos basta con `5V` y `GND`).
* **E-STOP:** el contacto NC con `INPUT_PULLUP` queda en LOW en reposo y pasa a HIGH al pulsarse (polaridad invertida respecto a los demás sensores). Documentarlo y leer el pin por interrupción.
* **Ethernet:**
  * El SPI del Mega llega como máximo a 8 MHz (no 14 MHz); verificar que `SPI_ETHERNET_SETTINGS` pueda redefinirse con `build_flags`.
  * Evitar DHCP en `setup()` (puede bloquear hasta 60 s): usar IP estática y una dirección MAC fija documentada.
* **Encoders:** fijar un límite de RPM junto al límite de PPR (a 3000 RPM, 600 PPR generarían ~120 000 flancos/s por encoder, por encima de los ~50 000 sostenibles en total).
* **`ERR-902`:** debe ser una advertencia que no detenga la estación (la estación opera en modo local sin red).
* **PWM de los puentes H:** 490 Hz es audible; evaluar subir la frecuencia de Timer1 y Timer2 (Timer0 no se debe modificar).
* **Asentamiento por distancia:** como mejora del asentamiento contra el brazo de tope, se podría sustituir `CLAMP_DELAY_MS` por una cantidad de pulsos del encoder de la banda (independiente de la velocidad). Además, el BTS7960 de la banda admite sensado de corriente (`IS`) como redundancia del detector de atasco, usando un canal analógico libre (p. ej. `A9`).
* **Contador de producción:** persistirlo en EEPROM escribiendo solo al cambiar.
* **MQTT:** definir topics de telemetría, estado y comandos, *Last Will* y QoS; los comandos remotos no deben saltarse los interbloqueos locales.
* **Jog táctil:** evaluar avance mientras se mantiene pulsado (en vez de un paso fijo al soltar), siempre respetando fin de carrera y soft limit.
* **Puesta en marcha:** documentar la secuencia de pruebas (pantalla, sensores, servos sin botella, bomba con agua, ejes Z en vacío, banda y encoder en vacío, ciclo completo).
