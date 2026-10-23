/**
 * @file stepper_axis.hpp
 * @brief Controlador HAL no bloqueante para ejes lineales paso a paso Z con ISR en Timer3 y protección de límites.
 */

#pragma once
#include <Arduino.h>

/**
 * @enum StepperSlot
 * @brief Identificador de instancia para registrar cada eje Z en la ISR compartida del Timer3.
 */
enum class StepperSlot : uint8_t
{
    FillZ = 0, ///< Eje Z del autómata de llenado (PORTC: PC0..PC2, Fin de carrera en PA2)
    CapZ  = 1  ///< Eje Z del autómata de tapado  (PORTC: PC3..PC5, Fin de carrera en PA3)
};

/**
 * @struct StepperAxisConfig
 * @brief Parámetros de pines y cinemática para inicializar un eje StepperAxis.
 */
struct StepperAxisConfig
{
    StepperSlot slot;                 ///< Ranura en la ISR del Timer3 (FillZ o CapZ)
    uint8_t     stepPin;              ///< Pin STEP en PORTC (37 o 34 en HardwarePins)
    uint8_t     dirPin;               ///< Pin DIR en PORTC (36 o 33 en HardwarePins)
    uint8_t     enPin;                ///< Pin EN en PORTC (35 o 32 en HardwarePins)
    uint8_t     limitSwitchPin;       ///< Pin de fin de carrera superior en PORTA (24 o 25 en HardwarePins)
    int32_t     maxTravelSteps;       ///< Límite máximo de carrera descendente por software [step]
    uint16_t    accelStepsPerSec2;    ///< Aceleración y desaceleración de la rampa trapezoidal [step/s^2]
    uint16_t    homeBackoffSteps;     ///< Pasos de retroceso descendente tras tocar el fin de carrera [step]
    bool        dirDownHigh;          ///< Nivel lógico en DIR para bajar hacia la botella (true = HIGH)
    bool        enableActiveLow;      ///< Polaridad del pin EN del driver TB6600 (true = LOW habilita)
    bool        limitSwitchActiveLow; ///< Polaridad del fin de carrera superior (true = LOW al activarse)
};

/**
 * @class StepperAxis
 * @brief Gestiona el perfil trapezoidal de velocidad, homing y vigilancia en ISR de un eje Z paso a paso.
 *
 * Los pulsos STEP se emiten desde la ISR del Timer3 a 20 kHz mediante escritura directa sobre PORTC.
 * En cada tick de la ISR se lee directamente el fin de carrera superior en PINA y se evalúa el soft limit
 * inferior para detener el motor en tiempo real duro sin depender del super-loop.
 */
class StepperAxis
{
public:
    static constexpr uint8_t  MAX_SLOTS           = 2;      ///< Cantidad de ejes Z gobernados por Timer3
    static constexpr uint16_t ISR_FREQ_HZ         = 20000U; ///< Frecuencia de interrupción de Timer3 COMPA [Hz]
    static constexpr uint16_t MAX_ALLOWED_SPEED   = 8000U;  ///< Velocidad máxima admisible por eje [step/s]
    static constexpr uint16_t MIN_START_SPEED     = 200U;   ///< Velocidad mínima de arranque/parada de rampa [step/s]
    static constexpr uint8_t  PROFILE_PERIOD_MS   = 20U;    ///< Periodo de actualización de la rampa trapezoidal [ms]
    static constexpr uint8_t  STEP_PULSE_WIDTH_US = 4U;     ///< Ancho de pulso STEP en alto para optoacoplador TB6600 [us]

    /**
     * @brief Construye un controlador de eje paso a paso Z.
     *
     * @param config Estructura de configuración con pines de PORTC/PORTA, aceleración y límites.
     */
    explicit StepperAxis(const StepperAxisConfig& config);

    /**
     * @brief Configura los pines GPIO, calcula las máscaras de puerto y arranca el Timer3 a 20 kHz.
     */
    void init();

    /**
     * @brief Actualiza de forma atómica el límite máximo de recorrido descendente por software (soft limit).
     *
     * Función no bloqueante. Segura para invocarse desde el super-loop.
     *
     * @param maxTravelSteps Cota máxima permitida desde home en micropasos [step] (> 0).
     */
    void setSoftLimits(int32_t maxTravelSteps);

    /**
     * @brief Inicia la secuencia no bloqueante de búsqueda de origen (homing) hacia el fin de carrera superior.
     *
     * Si el fin de carrera ya está pulsado al iniciar, pasa directamente a la fase de retroceso (homeBackoffSteps).
     *
     * @param homingSpeed Velocidad de búsqueda del fin de carrera en pasos por segundo [step/s] (rango [1..8000]).
     */
    void home(uint16_t homingSpeed);

    /**
     * @brief Configura el desplazamiento hacia una posición absoluta objetivo dentro de [0, maxTravelSteps].
     *
     * Calcula el perfil trapezoidal sin bloquear la CPU. Los pulsos se emiten desde la ISR del Timer3.
     *
     * @param targetSteps Posición absoluta de destino desde home expresada en micropasos [step].
     * @param maxSpeed Velocidad límite de crucero en pasos por segundo [step/s] (máx. 8000).
     * @return true si la consigna está dentro de los límites por software y el eje está en home; false si se rechazó.
     */
    bool moveToTarget(int32_t targetSteps, uint16_t maxSpeed);

    /**
     * @brief Ordena el ascenso continuo hacia el fin de carrera superior (usado en RaisingNozzle y RaisingHead).
     *
     * Función no bloqueante. La ISR detendrá automáticamente el eje al detectar el fin de carrera en PINA.
     *
     * @param maxSpeed Velocidad de ascenso en pasos por segundo [step/s] (máx. 8000).
     */
    void moveTowardHomeSwitch(uint16_t maxSpeed);

    /**
     * @brief Ejecuta un desplazamiento ascendente relativo seguro (jog) para el modo ManualRecovery.
     *
     * Función no bloqueante. La ISR detiene el movimiento de inmediato si se alcanza el fin de carrera superior.
     *
     * @param steps Cantidad de micropasos a subir [step].
     * @param speed Velocidad de desplazamiento manual en pasos por segundo [step/s] (máx. 8000).
     */
    void jogUp(uint16_t steps, uint16_t speed);

    /**
     * @brief Detiene de forma atómica e inmediata la emisión de pulsos del eje manteniendo el par de retención.
     *
     * Función no bloqueante. Segura para invocarse desde el super-loop.
     */
    void stop();

    /**
     * @brief Habilita o deshabilita la etapa de potencia del driver paso a paso (línea EN).
     *
     * @param enable true para energizar las bobinas; false para liberar el eje (usado en ErrorStop).
     */
    void setDriverEnabled(bool enable);

    /**
     * @brief Recalcula la velocidad instantánea de la rampa trapezoidal y supervisa las fases de homing.
     *
     * Función no bloqueante. Debe invocarse periódicamente desde la tarea de perfiles de movimiento (cada 20 ms).
     */
    void updateMotionProfile();

    /**
     * @brief Consulta si el actuador se encuentra actualmente ejecutando una trayectoria o secuencia de homing.
     *
     * Segura para invocarse desde el super-loop.
     *
     * @return true si el eje está en movimiento; false si alcanzó la cota de reposo.
     */
    bool isBusy() const;

    /**
     * @brief Verifica si el eje alcanzó satisfactoriamente la última cota comandada por moveToTarget().
     * @return true si el movimiento concluyó en targetSteps sin disparar límites; false en caso contrario.
     */
    bool isTargetReached() const;

    /**
     * @brief Evalúa si el eje completó la secuencia de homing y su referencia cero es válida.
     * @return true si el eje está referenciado en home; false si requiere ejecutar home().
     */
    bool isHome() const;

    /**
     * @brief Indica si la ISR detuvo el eje por intentar rebasar maxTravelSteps (ERR-202 / ERR-212).
     * @return true si se disparó la protección de soft limit en la ISR; false en operación normal.
     */
    bool hasSoftLimitTripped() const;

    /**
     * @brief Restablece de forma atómica la bandera de fallo por soft limit tras un rearme.
     */
    void clearSoftLimitFault();

    /**
     * @brief Copia de forma atómica la posición actual del eje medida desde home.
     *
     * Segura para invocarse desde el super-loop mientras la ISR está activa.
     *
     * @return Posición actual expresada en micropasos [step] (0 = tope superior, positivo = hacia abajo).
     */
    int32_t getCurrentSteps() const;

    /**
     * @brief Despachador estático invocado exclusivamente desde ISR(TIMER3_COMPA_vect) a 20 kHz.
     *
     * No debe invocarse desde el super-loop.
     */
    static void handleTimer3Isr();

private:
    uint8_t handleTimerIsrTick();

    enum class HomingPhase : uint8_t
    {
        Unhomed,
        SeekingSwitch,
        HitSwitch,
        BackingOff,
        Homed
    };

    static void initTimer3Once();
    void setDirectionDown(bool down);
    bool isLimitSwitchTriggeredIsr() const;

    static StepperAxis* instances_[MAX_SLOTS];
    static bool         timer3Initialized_;

    StepperAxisConfig     config_;
    uint8_t               stepMaskPortC_;
    uint8_t               limitMaskPortA_;
    volatile int32_t      currentSteps_;
    volatile int32_t      targetSteps_;
    volatile int32_t      maxTravelSteps_;
    volatile uint16_t     currentSpeedStepsPerSec_;
    uint16_t              cruiseSpeedStepsPerSec_;
    uint16_t              phaseAccumulator_;
    volatile bool         moving_;
    volatile bool         dirDown_;
    volatile bool         ignoreSoftLimit_;
    volatile bool         softLimitTripped_;
    volatile HomingPhase  homingPhase_;
};
