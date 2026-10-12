# Arquitectura de Software y Hardware: Estación 3 (Llenado y Tapado)

Este documento define la arquitectura técnica integral, los estándares de codificación, los patrones de diseño embebido, el subsistema de diagnóstico con códigos de error y pantalla local, el modelo de concurrencia multitarea con FreeRTOS, el acondicionamiento eléctrico y el catálogo de mejoras técnicas para la **Estación 3 (Llenado y Tapado)** en una línea de Sistemas de Manufactura Flexible (FMS).

El controlador principal de la estación es un **ESP32 DevKit V1 (Dual Core Tensilica Xtensa @ 240 MHz)**.

## 1\. Reglas de Idioma y Nomenclatura del Código

### 1.1 Políticas de Idioma

* **Identificadores del código (Inglés técnico obligatorio):** Clases, métodos, funciones, variables, estructuras, enumeraciones, macros, archivos y constantes deben declararse exclusivamente en inglés.

* **Comentarios y Documentación (Español técnico):** Los comentarios de código, descripciones en cabeceras Doxygen y manuales de arquitectura se redactan en español.

* **Textos dirigidos al usuario / HMI / Pantalla Local (Español):** Mensajes en la pantalla local LCD/OLED, códigos descriptivos de fallo, terminal serie de diagnóstico y alarmas enviadas al panel SCADA de Node-RED deben estar redactados en español.

### 1.2 Formato y Estilo de Llaves (Allman Style)

El proyecto adopta estrictamente el estilo **Allman** (BSD style). Las llaves de apertura `{` y cierre `}` se colocarán siempre en su propia línea, alineadas verticalmente con la indentación de la estructura de control, clase o función asociada:

```
// Correcto (Allman Style)
void executeCappingProcess()
{
    if (isBottleSeated)
    {
        startChuckMotor();
    }
    else
    {
        triggerAlarm(ErrorCode::TimeoutSeatDetection);
    }
}

// Prohibido (K&R / Java Style)
void executeCappingProcess() {
    if (isBottleSeated) {
        // ...
    }
}

```

&nbsp;

### 1.3 Convenciones de Nomenclatura

| Elemento | Convención | Ejemplo |
| :---- | :---- | :---- |
| **Clases / Structs** | `PascalCase` | `StepperAxis`, `DisplayManager`, `CappingStation` |
| **Métodos y Funciones** | `camelCase` | `moveToTarget()`, `showError()`, `update()` |
| **Variables y Miembros Privados** | `camelCase` con sufijo `_` | `targetSpeed`, `encoderPulseCount`, `currentState_` |
| **Constantes y Macros** | `UPPER_SNAKE_CASE` | `MAX_SERVO_ANGLE`, `I2C_LCD_ADDR`, `DEBOUNCE_MS` |
| **Valores de Enumeraciones (`enum class`)** | `PascalCase` | `StationState::TighteningCap`, `ErrorCode::ZAxisHomeFailed` |
| **Nombres de Archivos** | `snake_case` uniforme | `display_manager.hpp`, `capping_station.cpp` |

&nbsp;

### **1.4 Estándar de Documentación con Doxygen**

Para mantener la claridad del código y habilitar la asistencia contextual (tooltips de IntelliSense) en VS Code / PlatformIO:

1. **Ubicación exclusiva en encabezados (`.hpp`):** Los bloques Doxygen se redactan **únicamente en los archivos `.hpp`**, documentando la interfaz pública y el contrato de cada clase. Los archivos `.cpp` se reservan para la implementación interna y solo llevan comentarios puntuales para lógica algorítmica compleja.  
2. **Prohibición de redundancias:** No documentar lo obvio (ej. `@brief Obtiene el valor. @return El valor`).  
3. **Énfasis en magnitudes, unidades y bloqueos:** Especificar explícitamente las unidades de los parámetros (pasos, pulsos, milisegundos, RPM, mA), el rango de operación admisible y si una función es no bloqueante o tiene efectos colaterales.

#### **Plantilla Oficial de Documentación Doxygen (Allman Style):**

```
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
     * Calcula el perfil trapezoidal sin bloquear la CPU. Requiere llamadas continuas
     * a run() en cada iteración de la tarea de control (Core 1).
     * 
     * @param targetSteps Posición de destino expresada en micropasos absolutos.
     * @param maxSpeed Velocidad límite de crucero en pasos por segundo [step/s].
     * @return true si la consigna está dentro de los límites por software; false si se rechazó.
     */
    bool moveToTarget(int32_t targetSteps, float maxSpeed);

    /**
     * @brief Consulta si el actuador se encuentra actualmente ejecutando una trayectoria.
     * @return true si el eje está en movimiento; false si alcanzó la cota de reposo.
     */
    bool isBusy() const;
};
```

&nbsp;

## 2\. Decisiones Clave de Infraestructura y Datos

### 2.1 Ecosistema de Compilación y Plataforma

* **Plataforma:** PlatformIO (`platform = espressif32`, `framework = arduino`, `board = esp32dev`).

* **Frecuencia de CPU:** $240\text{ MHz}$ fija para garantizar el determinismo temporal en tareas críticas de control.

* **Control de Versiones de Dependencias:** El archivo `platformio.ini` debe fijar versiones específicas para librerías externas (`ArduinoJson`, `PubSubClient`, `LiquidCrystal_I2C` o drivers gráficos OLED).

### 2.2 Temporización y Concurrencia (FreeRTOS Dual-Core)

* **Prohibición de funciones bloqueantes:** Prohibido el uso de `delay()` dentro del ciclo de la FSM o en tareas de control. Las temporizaciones de estado se realizan con aritmética no bloqueante sobre `millis()` o retardos relativos de FreeRTOS (`vTaskDelayUntil()`).

* **Arquitectura Simétrica Dual-Core:**

  * **Core 1 (Prioridad Alta):** Asignado exclusivamente al lazo de control mecatrónico en tiempo real: ejecución de la FSM (Entry, During, Exit), control de pulsos del motor a pasos y muestreo de sensores.

  * **Core 0 (Prioridad Media/Baja):** Asignado a la interfaz de usuario y comunicaciones: refresco de la pantalla local LCD/OLED vía bus I2C, pila Wi-Fi, cliente MQTT, serialización/deserialización JSON y sincronización con Node-RED.

* **Aislamiento del Bus I2C en Core 0:** Las transacciones I2C ($100\text{ kHz} - 400\text{ kHz}$) presentan latencias bloqueantes de entre $2\text{ ms}$ y $15\text{ ms}$. Actualizar una pantalla en el Core 1 provocaría pérdida de pasos en el husillo Acme. Por tanto, el refresco del display se ejecuta únicamente desde el Core 0 mediante una cola de mensajes (`displayQueueHandle`).

### 2.3 Hardware Pulse Counter (PCNT) para Encoders

* Queda prohibido el conteo de pulsos de encoders mediante interrupciones convencionales de software (`attachInterrupt`), ya que saturan la CPU a altas revoluciones.

* Se utiliza el periférico nativo por hardware **PCNT (Pulse Counter)** del ESP32. El conteo de cuadratura se procesa a nivel de registros internos sin intervención de los núcleos, reduciendo el consumo de CPU a un nivel virtualmente nulo.

### 2.4 Modelo de Datos y Telemetría

* **Capa de Comunicación:** Integración de red TCP/IP mediante MQTT con **Node-RED**.

* **Formato de Carga Útil (Payload):** Tramas en formato JSON compacto (procesadas con `ArduinoJson`) enviando telemetría de producción, estado de sensores, códigos de error activos y recibiendo consignas de operación.

## 3\. Taxonomía de Códigos de Error y Sistema de Diagnóstico Local

Para facilitar la rápida intervención de los operarios en planta sin requerir una computadora conectada al puerto serie, la estación incorpora una taxonomía estructurada de códigos de error numéricos indexados en el manual de usuario de la celda.

### 3.1 Estructura Numérica de Errores

Los errores se clasifican en cuatro rangos numéricos bien definidos dentro del enum class `ErrorCode`:

* **`100 - 199`:** Sensores e Insumos (Timeouts por botellas faltantes, desalineaciones, atasco en tolva o salida).

* **`200 - 299`:** Cinemática y Recorridos (Fallo de homing, bloqueo de indexador, soft limits rebasados).

* **`300 - 399`:** Actuadores y Potencia (Sobrecorriente de apriete, stall anómalo, fallo de puente H).

* **`900 - 999`:** Seguridad y Enlace (Seta E-STOP activada, enlace MQTT/SCADA desconectado).

### 3.2 Catálogo Oficial de Códigos de Error (`station_types.hpp`)

```
#pragma once
#include <Arduino.h>

enum class ErrorCode : uint16_t
{
    None                    = 0,

    // 100s: Sensores y Tiempos Límite (Timeouts)
    TimeoutEntryPusher      = 101, // ERR-101: Servo empujador no completó carrera
    TimeoutSeatDetection    = 102, // ERR-102: Botella no asentada en tiempo límite
    TimeoutExitEvacuation   = 103, // ERR-103: Envase no evacuado en la salida

    // 200s: Cinemática y Movimiento
    ZAxisHomeFailed         = 201, // ERR-201: Husillo Acme no halló fin de carrera
    ZAxisSoftLimitHit       = 202, // ERR-202: Cota vertical máxima rebasada (Soft limit)
    RotaryIndexStall        = 203, // ERR-203: Mesa indexadora atascada mecánicamente

    // 300s: Apriete y Potencia
    ChuckOvercurrent        = 301, // ERR-301: Corriente excesiva en motor de apriete
    ChuckStallPremature     = 302, // ERR-302: Bloqueo prematuro (rosca trasroscada)

    // 900s: Seguridad y Red
    EmergencyStopPressed    = 901, // ERR-901: Parada de emergencia por hardware activada
    NetworkLinkLost         = 902  // ERR-902: Pérdida prolongada de enlace con SCADA
};

struct DisplayEvent
{
    ErrorCode error;
    const char* stateName;
    uint32_t bottleCount;
};

```

&nbsp;

### 3.3 Formato Visual en Pantalla Local (LCD 20x4 I2C)

El display ofrece una lectura rápida y estructurada en 4 filas de 20 caracteres:

```
+--------------------+
|ESTACION 3: TAPADO  | -> Fila 1: Identificación de estación
|ESTADO: [ERROR]     | -> Fila 2: Condición operativa
|COD: ERR-102 ASIENTO| -> Fila 3: Código oficial + Mensaje corto
|REVISE MANUAL SEC 4 | -> Fila 4: Instrucción de acción para operador
+--------------------+

```

&nbsp;

## 4\. Patrones de Diseño Recomendados

### 4.1 Hardware Abstraction Layer (HAL) / Component Object Pattern

Aísla el silicio del ESP32 y el hardware de potencia de la lógica de proceso. La FSM nunca invoca directamente primitivas como `digitalWrite()`, `analogRead()` ni manipula registros periféricos.

1. **Clase `DigitalSensor`:**

   * Encapsula sensores infrarrojos (IR) y finales de carrera mecánicos.

   * Filtro de rebotes (*debounce*) no bloqueante mediante `millis()`.

   * Polaridad configurable para entradas activas en alto (`Active HIGH`) o activas en bajo (`Active LOW`).

   * Métodos: `isTriggered()`, `hasChanged()`, `onRisingEdge()`, `onFallingEdge()`.

2. **Clase `DcMotorBts7960`:**

   * Controla módulos de puente H BTS7960 mediante periféricos PWM del ESP32.

   * Administra pines de habilitación (`EN`) y canales de modulación (`RPWM`, `LPWM`).

   * Incorpora frenado regenerativo dinámico (`brake()`), parada libre (`stop()`) y protección de tiempo muerto (*dead-time*).

3. **Clase `PcntEncoder`:**

   * Abstrae una unidad del periférico PCNT del ESP32 configurada para decodificación de cuadratura (canales A y B).

   * Expone métodos de lectura atómica: `getPulses()`, `reset()`, `getSpeedRpm()`.

4. **Clase `StepperAxis`:**

   * Controla drivers de paso/dirección (TB6600, DM542) para la mesa indexadora ($9{0}^{\circ }$) y el husillo vertical Acme.

   * Ejecución no bloqueante de rampas de aceleración trapezoidales y control de límites virtuales.

   * Métodos: `moveTo()`, `run()`, `isBusy()`, `home()`, `setSoftLimits()`.

5. **Clase `ConveyorController`:**

   * Integra una instancia de `DcMotorBts7960` con una instancia de `PcntEncoder`.

   * Controla el avance coordinado por distancia física o desplazamiento continuo con monitoreo de velocidad.

6. **Clase `DisplayManager`:**

   * Encapsula la comunicación I2C con la pantalla local (LCD 20x4 u OLED).

   * Mantiene el formateo de cadenas de texto y renderizado de diagnósticos.

   * Métodos: `init()`, `showOperationalState()`, `showError(ErrorCode code)`, `clear()`.

### 4.2 Mediator Pattern (Coordinador de Estación)

Elimina el acoplamiento cruzado entre actuadores, sensores y pantallas.

* La clase mediadora **`CappingStation`**:

  * Contiene las instancias de todos los sensores, actuadores, la FSM y la cola de telemetría hacia el display.

  * Inspecciona periódicamente el estado de los componentes (`stepperAxis.isBusy()`, `sensorEntry.isTriggered()`).

  * Despacha comandos a los módulos de hardware según el estado activo de la FSM, evitando que los periféricos se conozcan o se invoquen entre sí.

### 4.3 Enum FSM con Ciclo de Vida de Tres Fases

Para evitar la sobrecarga polimórfica (`vtables`) y el riesgo de fragmentación de memoria dinámica del patrón *State* clásico de GoF, se implementa una **FSM basada en `enum class` con control explícito de tres fases**:

* **Fase Entry:** Se ejecuta una única vez al entrar al estado (arranque de motores, inicio de temporizadores, configuración de consignas).

* **Fase During:** Se ejecuta continuamente en cada ciclo de actualización mientras persista el estado activo. Monitorea timeouts de guardia.

* **Fase Exit:** Se ejecuta una única vez al detectar una transición de estado, inmediatamente antes de cambiar de fase (parada de actuadores, limpieza de banderas).

#### Estructura de Control de la FSM (Allman Style):

```
enum class StationState
{
    Boot,
    Idle,
    PushingBottle,
    IndexingToWork,
    CheckSeat,
    LoweringNozzle,
    TighteningCap,
    RaisingNozzle,
    IndexingToExit,
    WaitingExit,
    ManualRecovery,
    ErrorStop
};

void CappingStation::update()
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

&nbsp;

## 5\. Arquitectura del Autómata de Tapado (FSM)

### 5.1 Diagrama de Flujo de Estados

```
[STATE_BOOT]
      │
      ▼ (Homing exitoso de husillo Acme y mesa indexadora)
[STATE_IDLE] ◄──────────────────────────────────────────────┐
      │                                                     │
      ▼ (Sensor IR Entrada = Activo)                        │
[STATE_PUSHING_BOTTLE]                                      │
      │                                                     │
      ▼ (Servo completa carrera de inserción y retorna)     │
[STATE_INDEXING_TO_WORK]                                    │
      │                                                     │
      ▼ (Giro de 90° concluido; tapa montada en trayecto)   │
[STATE_CHECK_SEAT]                                          │
      │                                                     │
      ▼ (Sensor de botella asentada = OK)                   │
[STATE_LOWERING_NOZZLE]                                     │
      │                                                     │
      ▼ (Husillo Acme alcanza cota de trabajo)              │
[STATE_TIGHTENING_CAP]                                      │
      │                                                     │
      ▼ (Tiempo X transcurrido / Condición de parada)       │
[STATE_RAISING_NOZZLE]                                      │
      │                                                     │
      ▼ (Husillo activa sensor de fin de carrera superior)  │
[STATE_INDEXING_TO_EXIT]                                    │
      │                                                     │
      ▼ (Giro de 90° concluido; expulsión mecánica)         │
[STATE_WAITING_EXIT]                                        │
      │                                                     │
      └────── (Sensor IR de Salida confirma evacuación) ────┘

      ═════════════════ RUTAS DE ALARMA ════════════════════
      Cualquier Estado ──(Timeout / Fallo / E-STOP)──► [STATE_ERROR_STOP]

```

&nbsp;

### 5.2 Matriz de Transición de Estados y Timeouts de Guardia

| Estado Origen | Fase Entry | Condición de Salida (During) | Timeout de Guardia | Fallo / Siguiente Estado |
| :---- | :---- | :---- | :---- | :---- |
| `Boot` | Iniciar homing de husillo Acme e indexador. | `nozzleStepper.isHome()` | $10000\text{ ms}$ | `ERR-201` $\rightarrow$ `ErrorStop` |
| `Idle` | Actuadores detenidos; vigilar entrada. | `sensorEntry.isTriggered()` | Sin timeout | $\rightarrow$ `PushingBottle` |
| `PushingBottle` | Desplegar servomotor empujador. | `servoPusher.isStrokeDone()` | $2500\text{ ms}$ | `ERR-101` $\rightarrow$ `ErrorStop` |
| `IndexingToWork` | Comandar giro de $9{0}^{\circ }$ en indexador. | `indexerStepper.isTargetReached()` | $4000\text{ ms}$ | `ERR-203` $\rightarrow$ `ErrorStop` |
| `CheckSeat` | Habilitar lectura de sensor de asiento. | `sensorSeat.isTriggered()` | $2000\text{ ms}$ | `ERR-102` $\rightarrow$ `ErrorStop` |
| `LoweringNozzle` | Bajar husillo Acme a cota de trabajo. | `nozzleStepper.isTargetReached()` | $5000\text{ ms}$ | `ERR-202` $\rightarrow$ `ErrorStop` |
| `TighteningCap` | Encender motor DC con goma de apriete. | `millis() - stageTimer_ >= TIGHTEN_MS` | $3500\text{ ms}$ | `ERR-301` $\rightarrow$ `ErrorStop` |
| `RaisingNozzle` | Subir husillo Acme hacia fin de carrera. | `limitSwitchZ.isTriggered()` | $5000\text{ ms}$ | `ERR-201` $\rightarrow$ `ErrorStop` |
| `IndexingToExit` | Girar indexador otros $9{0}^{\circ }$. | `indexerStepper.isTargetReached()` | $4000\text{ ms}$ | `ERR-203` $\rightarrow$ `ErrorStop` |
| `WaitingExit` | Aguardar detección de evacuación. | `sensorExit.isTriggered()` | $6000\text{ ms}$ | `ERR-103` $\rightarrow$ `ErrorStop` |
| `ErrorStop` | Cortar PWM y deshabilitar todos los drivers. | Señal de rearme manual recibida. | Sin timeout | $\rightarrow$ `ManualRecovery` |
| `ManualRecovery` | Habilitar comandos manuales paso a paso. | Operador confirma despeje de línea. | Sin timeout | $\rightarrow$ `Boot` |

&nbsp;

## 6\. Concurrencia y Multitarea con FreeRTOS

Para erradicar pérdidas de pasos en los motores y retardos en la máquina de estados provocados por el refresco I2C de la pantalla y la pila de red, la ejecución se particiona formalmente entre los dos núcleos del ESP32:

```
                  ┌─────────────────────────────────────────┐
                  │           ESP32 DUAL CORE SYSTEM        │
                  └─────────────────────────────────────────┘
                                       │
            ┌──────────────────────────┴──────────────────────────┐
            ▼                                                     ▼
     ┌──────────────┐                                      ┌──────────────┐
     │    CORE 0    │                                      │    CORE 1    │
     │ Network & UI │                                      │ Control Task │
     └──────┬───────┘                                      └──────┬───────┘
            │                                                     │
            ├─ Conexión Wi-Fi / Reconexión                        ├─ FSM Update (3 Fases)
            ├─ Cliente MQTT (Loop)                                ├─ StepperAxis::run()
            ├─ Serialización JSON                                 ├─ Sensores (Debounce)
            ├─ DisplayManager (LCD/OLED I2C)                      │
            │                                                     │
            └───────────────◄ [ FreeRTOS Queue ] ◄────────────────┘
                             (displayQueueHandle)

```

&nbsp;

### 6.1 Implementación de Tareas y Punto de Entrada

```
#include <Arduino.h>
#include "station/capping_station.hpp"
#include "network/scada_client.hpp"
#include "HAL/display_manager.hpp"

CappingStation station;
ScadaClient scadaClient;
DisplayManager display(0x27, 20, 4); // LCD 20x4 en dirección I2C 0x27

QueueHandle_t displayQueueHandle = nullptr;

// Tarea asignada al Core 1: Control de Alta Prioridad
void taskControlCore1(void* pvParameters)
{
    for (;;)
    {
        station.update();
        vTaskDelay(pdMS_TO_TICKS(1)); // Cede CPU para evitar starvation del watchdog
    }
}

// Tarea asignada al Core 0: Interfaz de Usuario y Red
void taskNetworkAndUiCore0(void* pvParameters)
{
    DisplayEvent event;

    for (;;)
    {
        // 1. Atender eventos de pantalla sin bloquear el lazo de control
        if (xQueueReceive(displayQueueHandle, &event, 0) == pdPASS)
        {
            if (event.error != ErrorCode::None)
            {
                display.showError(event.error, event.stateName);
            }
            else
            {
                display.showOperationalState(event.stateName, event.bottleCount);
            }
        }

        // 2. Atender cliente SCADA / MQTT
        scadaClient.update();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup()
{
    displayQueueHandle = xQueueCreate(5, sizeof(DisplayEvent));

    display.init();
    display.showBoot();

    station.init(displayQueueHandle);
    scadaClient.init();

    // Creación de tareas ancladas a sus respectivos núcleos
    xTaskCreatePinnedToCore(
        taskControlCore1,
        "ControlTask",
        8192,
        NULL,
        2,              // Prioridad superior para control
        NULL,
        1               // Core 1
    );

    xTaskCreatePinnedToCore(
        taskNetworkAndUiCore0,
        "NetworkUiTask",
        8192,
        NULL,
        1,              // Prioridad regular para red y UI
        NULL,
        0               // Core 0
    );
}

void loop()
{
    // Bucle inactivo; el despachador de FreeRTOS administra la ejecución
    vTaskDelete(NULL);
}

```

&nbsp;

## 7\. Acondicionamiento Eléctrico y Mapeo Seguro de GPIOs (ESP32 @ 3.3V)

### 7.1 Reglas Críticas de Acondicionamiento Eléctrico

1. **Incompatibilidad de Voltaje de Entrada:** Los GPIOs del ESP32 operan estrictamente a un nivel de 3.3 V y **no toleran entradas de 5 V ni 24 V**.
   * Sensores inductivos / ópticos industriales alimentados a 24 V deben aislarse mediante optoacopladores (PC817 o similar) o módulos de acondicionamiento NPN/PNP con salida a 3.3 V.
   * Encoders alimentados a 5 V requieren desplazadores de nivel lógicos (*Logic Level Shifters*) bidireccionales de 5 V → 3.3 V antes de ingresar a los pines PCNT.

2. **Prohibición de Redes Analógicas Resistivas en Sensores:**
   * Queda estrictamente descartado agrupar sensores sobre un único cable analógico mediante escalera de resistencias o divisores de tensión pasivos. En una máquina de manufactura continua:
     * Múltiples sensores pueden activarse al mismo tiempo (ej. botella ingresando mientras el husillo toca fin de carrera), generando combinaciones en paralelo con caídas de tensión ambiguas.
     * La conmutación de motores de 24 V induce ruido electromagnético (EMI) de ±100 mV, lo que falsearía lecturas analógicas continuas.
     * Los sensores ópticos IR son dispositivos activos (transistores con salida lógica NPN/PNP) y no resistencias pasivas.
     * La lectura del ADC del ESP32 introduce latencias de conversión y filtrado que comprometen el frenado de emergencia en finales de carrera.
   * Por tanto, **cada sensor dispone de su propia línea digital discreta**. Si a futuro se requiere ampliar entradas, se utilizará un expansor I2C **MCP23017** en el mismo bus del display.

3. **Placa de Expansión (Base Shield G-V-S 30 Pines - UNIT Electronics):**
   * Se utiliza una base shield diseñada específicamente para el ESP32 DevKit V1 de 30 pines, distribuyendo cada GPIO en tripleta G-V-S (Ground, Voltage, Signal).
   * **El jumper selector de voltaje debe configurarse obligatoriamente en 3.3 V** para proteger las entradas del microcontrolador.
   * La placa incluye un conector I2C dedicado de 4 pines (`D21`, `D22`, `VCC`, `GND`) en la esquina inferior derecha, conectando directamente el display LCD 20x4 con un arnés plano de 4 hilos.
   * La potencia para actuadores y servomotores se toma a través del Jack DC externo (6.5 V - 12 V) de la base, protegiendo el puerto USB de la computadora de caídas de tensión.

4. **Bus I2C Seguro:**
   * La pantalla LCD 20x4 suele requerir 5 V para iluminar la luz de fondo. Si el módulo expansor PCF8574 opera a 5 V, sus líneas SDA/SCL tendrán resistencias pull-up hacia 5 V. **Es obligatorio alimentar el módulo PCF8574 a 3.3 V o intercalar un logic level shifter I2C** para no inyectar 5 V en los pines `GPIO 21` y `GPIO 22`.

5. **Excitación de Actuadores (Salidas 3.3 V):**
   * **Drivers TB6600:** Los optoacopladores internos conmutan de forma segura conectando los terminales comunes `PUL+`, `DIR+`, `ENA+` a la línea de 3.3 V del microcontrolador y modulando los cátodos a través de los GPIOs.
   * **BTS7960:** El voltaje umbral de entrada lógica en los pines `RPWM` / `LPWM` / `EN` típicamente acepta 3.3 V. En caso de registrar calentamiento por conmutación incompleta, intercalar un buffer de nivel a 5 V (tipo 74HCT245).

6. **Restricciones de Pines de Arranque (*Strapping Pins*):**
   * Queda terminantemente prohibido conectar finales de carrera o resistencias pull-up/pull-down externas a los pines **GPIO 0, GPIO 2, GPIO 12 y GPIO 15**, ya que condicionan el modo de arranque (*bootloader/flash mode*) del ESP32.

7. **Pines de Solo Entrada (*Input-Only*):**
   * Los pines **GPIO 34, 35, 36 y 39** no disponen de transistores de salida ni resistencias pull-up/pull-down internas. Se destinan exclusivamente a sensores con resistencias de pull-up externas o acondicionadores activos.

### 7.2 Asignación Centralizada de Pines (`hardware_pins.hpp`)

```
#pragma once
#include <Arduino.h>

namespace HardwarePins
{
    // ==========================================
    // BUS I2C: Pantalla Local LCD 20x4 / OLED (Core 0)
    // ==========================================
    constexpr uint8_t PIN_I2C_SDA           = 21; // Línea de datos I2C (Requiere nivel 3.3V)
    constexpr uint8_t PIN_I2C_SCL           = 22; // Línea de reloj I2C (Requiere nivel 3.3V)

    // ==========================================
    // ENTRADAS: Sensores Digitales (GPIO 34-39 con pull-up externo)
    // ==========================================
    constexpr uint8_t PIN_IR_ENTRY          = 34; // Entrada digital (Input-Only, requiere pull-up ext)
    constexpr uint8_t PIN_IR_SEAT           = 35; // Entrada digital (Input-Only, requiere pull-up ext)
    constexpr uint8_t PIN_IR_EXIT           = 36; // Entrada digital (Input-Only, requiere pull-up ext)
    constexpr uint8_t PIN_LIMIT_SWITCH_Z    = 39; // Fin de carrera superior husillo Acme

    // ==========================================
    // ENTRADAS: Canales de Cuadratura PCNT (Encoders por Hardware)
    // ==========================================
    constexpr uint8_t PIN_ENC_CHUCK_A       = 25; // Encoder cabezal apriete canal A
    constexpr uint8_t PIN_ENC_CHUCK_B       = 26; // Encoder cabezal apriete canal B
    constexpr uint8_t PIN_ENC_CONV1_A       = 27; // Encoder banda transportadora 1 A
    constexpr uint8_t PIN_ENC_CONV1_B       = 14; // Encoder banda transportadora 1 B

    // ==========================================
    // ENTRADA ANALÓGICA: Sensado de Corriente
    // ==========================================
    constexpr uint8_t PIN_ADC_CURRENT_SENSE = 32; // Lectura pin IS_L / IS_R del BTS7960

    // ==========================================
    // SALIDAS: Servomotor Empujador
    // ==========================================
    constexpr uint8_t PIN_SERVO_PUSHER      = 13;

    // ==========================================
    // SALIDAS: Motor a Pasos - Husillo Acme (Eje Z)
    // ==========================================
    constexpr uint8_t PIN_STEPPER_Z_STEP    = 16;
    constexpr uint8_t PIN_STEPPER_Z_DIR     = 17;
    constexpr uint8_t PIN_STEPPER_Z_EN      = 5;

    // ==========================================
    // SALIDAS: Motor a Pasos - Mesa Indexadora (90°)
    // ==========================================
    constexpr uint8_t PIN_STEPPER_ROT_STEP  = 18;
    constexpr uint8_t PIN_STEPPER_ROT_DIR   = 19;
    constexpr uint8_t PIN_STEPPER_ROT_EN    = 33; // Salida digital segura

    // ==========================================
    // SALIDAS: Puente H BTS7960 (Apriete de Tapa)
    // ==========================================
    constexpr uint8_t PIN_BTS_CHUCK_RPWM    = 23; // Salida PWM hardware
    constexpr uint8_t PIN_BTS_CHUCK_LPWM    = 4;  // Salida PWM hardware
    constexpr uint8_t PIN_BTS_CHUCK_EN      = 2;  // R_EN y L_EN unificados
}

```

&nbsp;

## 8\. Estructura de Directorios

Organización modular estandarizada para PlatformIO:

```
station3_capping/
├── platformio.ini
├── ARCHITECTURE.md
├── Doxyfile
├── include/
│   ├── config/
│   │   ├── hardware_pins.hpp        # Mapeo centralizado de GPIOs para ESP32
│   │   └── process_params.hpp       # Tiempos, límites de carrera, parámetros de torque
│   └── types/
│       └── station_types.hpp        # Enums de estados, códigos de error y structs de evento
├── lib/
│   ├── HAL/
│   │   ├── DigitalSensor/           # Filtro de debounce y detección de flancos
│   │   ├── DcMotorBts7960/          # Control PWM por hardware con frenado dinámico
│   │   ├── PcntEncoder/             # Controlador nativo PCNT por hardware
│   │   ├── StepperAxis/             # Generación de pasos, rampas y soft limits
│   │   └── DisplayManager/          # Driver I2C para LCD 20x4 / OLED (aislado en Core 0)
│   ├── Controllers/
│   │   ├── ConveyorController/      # Coordinador de avance de banda y encoder
│   │   └── PusherServo/             # Control angular del servo empujador
│   └── Network/
│       └── ScadaClient/             # Serializador JSON y cliente MQTT para Node-RED
└── src/
    ├── fsm/
    │   ├── capping_fsm.hpp          # Declaración de la FSM de 3 fases
    │   └── capping_fsm.cpp          # Implementación de onEntry, onDuring, onExit
    ├── station/
    │   ├── capping_station.hpp      # Clase Mediadora (orquesta sensores, actuadores y colas)
    │   └── capping_station.cpp
    └── main.cpp                     # Setup de tareas FreeRTOS y despacho dual-core

```

&nbsp;

## 9\. Roadmap de Mejoras a Futuro, Posibles Errores y Soluciones Técnicas

Esta sección consolida las optimizaciones de ingeniería mecatrónica y de software identificadas tras el diseño preliminar. Se documentan como mejoras para la fase de endurecimiento (*hardening*) posterior a la validación del prototipo funcional.

### 9.1 Control de Apriete de Tapa: Detección de Torque / Stall vs. Temporización Ciega

* **Problema Potencial:** El apriete regulado únicamente por tiempo ciego ($X\text{ s}$) no absorbe tolerancias mecánicas ni roscas desalineadas. Si la tapa aprieta antes de lo estimado, el motor continuará girando hasta barrer la rosca plástica o entrar en condición de bloqueo (*stall*), demandando picos de corriente elevados al BTS7960. Si el envase entra flojo, el tiempo puede expirar sin garantizar sellado hermético.

* **Solución a Futuro:**

  1. *Sensado de Corriente (Hardware BTS7960):* Monitorear el pin analógico `IS` mediante el ADC del ESP32 (canal `GPIO 32`). Al rebasar el umbral calibrado de corriente admisible (torque de cierre óptimo), ordenar de forma inmediata el frenado dinámico del motor.

  2. *Detección de Bloqueo por Periférico PCNT:* Leer el incremento de pulsos mediante el hardware PCNT. Si la modulación PWM está activa pero el conteo de pulsos permanece estático por más de $150\text{ ms}$, inferir tope mecánico de rosca y frenar el cabezal (`brake()`).

### 9.2 Timeouts de Fase en la FSM y Códigos de Error para Operador

* **Problema Potencial:** Si un sensor IR se descalibra, una botella se atasca en la mesa indexadora o un actuador no completa su recorrido físico, la máquina de estados quedará aguardando indefinidamente en la fase `During`, provocando un bloqueo silencioso de la estación y deteniendo la línea de manufactura completa.

* **Estrategia Implementada:**

  * Cada estado evalúa activamente su temporizador de guardia contra la tabla de tiempos límite.

  * Al expirar el tiempo de guardia, la FSM asigna el código correspondiente de la serie `100s` / `200s` al evento, detiene los actuadores y transiciona al estado seguro `ErrorStop`.

  * La cola `displayQueueHandle` despacha el error al Core 0 para su proyección instantánea en la pantalla LCD y publicación en el SCADA.

### 9.3 Protección de Carrera del Eje Z (Límites por Software / Soft Limits)

* **Problema Potencial:** Aunque el husillo vertical Acme ejecuta un *homing* exitoso contra el fin de carrera superior y calcula la carrera descendente por pasos fijos, la presencia de envases con cotas fuera de norma o cuerpos extraños sobre la base puede provocar que el husillo golpee un tope rígido, dañando el acoplamiento mecánico o deformando el eje roscado.

* **Solución a Futuro:**

  * Implementar verificación de cotas virtuales (*Soft Limits*) dentro de la clase `StepperAxis`: denegar cualquier desplazamiento que sobrepase el límite máximo de diseño (`MAX_Z_TRAVEL_STEPS`), disparando la alarma `ERR-202`.

  * Como salvaguarda física complementaria, intercalar un resorte de compensación mecánica con un microswitch de contacto en el cabezal de apriete para absorber sobrecarreras de impacto sin forzar la mecánica.

### 9.4 Parada de Emergencia (E-STOP) por Hardware y Procedimiento de Rearme Seguro

* **Problema Potencial:** Una orden de parada originada exclusivamente por software (vía SCADA o FSM) no satisface los estándares de seguridad ante cuelgues del procesador, bloqueos del bus o fallas en el firmware.

* **Solución a Futuro:**

  * **Corte Eléctrico Directo de Potencia:** La seta de emergencia física debe desconectar físicamente la línea principal de $24\text{ V}$ hacia los drivers de potencia (BTS7960 y TB6600) mediante un relé o contactor de seguridad con enclavamiento, o abriendo por hardware las líneas de habilitación general (`ENABLE`).

  * **Notificación al Microcontrolador:** Un contacto auxiliar de la seta informa al ESP32 para conmutar la FSM al estado `ErrorStop` (`ERR-901`) y proyectar el estado en pantalla.

  * **Modo de Recuperación Manual (*Manual Recovery*):** Tras restablecer la seta de seguridad, el sistema no debe ejecutar un *homing* automático ciego; si el cabezal quedó enganchado en el cuello de una botella, subir repentinamente causaría roturas mecánicas. La FSM transicionará a `ManualRecovery`, exigiendo al operario liberar el área y validar el movimiento eje por eje antes de habilitar el ciclo automático.