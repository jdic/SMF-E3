/**
 * @file stepper_axis.hpp
 * @brief Controlador HAL no bloqueante para ejes lineales paso a paso Z con ISR en Timer3 y proteccion de limites.
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Identificador de instancia para registrar cada eje Z en la ISR compartida del Timer3.
 */
enum class StepperSlot : uint8_t
{
    FillZ = 0, // Eje Z del automata de llenado (PORTC: PC0..PC2, Fin de carrera en PA2)
    CapZ  = 1  // Eje Z del automata de tapado  (PORTC: PC3..PC5, Fin de carrera en PA3)
};

/**
 * @brief Parametros de pines y cinematica para inicializar un eje StepperAxis.
 */
struct StepperAxisConfig
{
    StepperSlot slot;                // Ranura en la ISR del Timer3 (FillZ o CapZ)
    uint8_t     stepPin;             // Pin STEP en PORTC (37 o 34 en HardwarePins)
    uint8_t     dirPin;              // Pin DIR en PORTC (36 o 33 en HardwarePins)
    uint8_t     enPin;               // Pin EN en PORTC (35 o 32 en HardwarePins)
    uint8_t     limitSwitchPin;      // Pin de fin de carrera superior en PORTA (24 o 25 en HardwarePins)
    int32_t     maxTravelSteps;      // Limite maximo de carrera descendente por software [step]
    uint16_t    accelStepsPerSec2;   // Aceleracion / desaceleracion de la rampa trapezoidal [step/s^2]
    uint16_t    homeBackoffSteps;    // Pasos de retroceso descendente tras tocar el fin de carrera [step]
    bool        dirDownHigh;         // Nivel logico en DIR para bajar hacia la botella (true = HIGH)
    bool        enableActiveLow;     // Polaridad del pin EN del driver TB6600 (true = LOW habilita)
    bool        limitSwitchActiveLow;// Polaridad del fin de carrera superior (true = LOW al activarse)
};

/**
 * @class StepperAxis
 * @brief Gestiona el perfil trapezoidal de velocidad, homing y vigilancia en ISR de un eje Z paso a paso.
 *
 * Los pulsos STEP se emiten desde la ISR del Timer3 a 20 kHz mediante escritura directa sobre PORTC.
 * En cada tick de la ISR se lee directamente el fin de carrera superior en PINA y se evalua el soft limit
 * inferior para detener el motor en tiempo real duro sin depender del super-loop.
 */
class StepperAxis
{
public:
    static constexpr uint8_t  MAX_SLOTS              = 2;
    static constexpr uint16_t ISR_FREQ_HZ            = 20000U;
    static constexpr uint16_t MAX_ALLOWED_SPEED      = 8000U;
    static constexpr uint16_t MIN_START_SPEED        = 200U;
    static constexpr uint8_t  PROFILE_PERIOD_MS      = 20U;
    static constexpr uint8_t  STEP_PULSE_WIDTH_US    = 4U;

    /**
     * @brief Construye un controlador de eje paso a paso Z.
     *
     * @param config Estructura de configuracion con pines de PORTC/PORTA, aceleracion y limites.
     */
    explicit StepperAxis(const StepperAxisConfig& config);

    /**
     * @brief Configura los pines GPIO, calcula las mascaras de puerto y arranca el Timer3 a 20 kHz.
     */
    void init();

    /**
     * @brief Actualiza el limite maximo de recorrido descendente por software (soft limit).
     *
     * @param maxTravelSteps Cota maxima permitida desde home en micropasos [step] (> 0).
     */
    void setSoftLimits(int32_t maxTravelSteps);

    /**
     * @brief Inicia la secuencia no bloqueante de busqueda de origen (homing) hacia el fin de carrera superior.
     *
     * @param homingSpeed Velocidad de busqueda del fin de carrera en pasos por segundo [step/s].
     */
    void home(uint16_t homingSpeed);

    /**
     * @brief Ordena un desplazamiento absoluto hacia una cota objetivo dentro de [0, maxTravelSteps].
     *
     * Calcula el sentido de giro y arranca la rampa trapezoidal sin bloquear la CPU.
     *
     * @param targetSteps Posicion absoluta de destino desde home en micropasos [step].
     * @param maxSpeed Velocidad maxima de crucero en pasos por segundo [step/s] (max. 8000).
     * @return true si la consigna es valida; false si rebasa los limites por software o el eje no tiene home.
     */
    bool moveToTarget(int32_t targetSteps, uint16_t maxSpeed);

    /**
     * @brief Ordena el ascenso continuo hacia el fin de carrera superior (usado en RaisingNozzle / RaisingHead).
     *
     * La ISR detendra automaticamente el eje al detectar la activacion del fin de carrera en PINA.
     *
     * @param maxSpeed Velocidad de ascenso en pasos por segundo [step/s] (max. 8000).
     */
    void moveTowardHomeSwitch(uint16_t maxSpeed);

    /**
     * @brief Ejecuta un desplazamiento ascendente relativo seguro (jog) para el modo ManualRecovery.
     *
     * @param steps Cantidad de pasos a subir [step]. Se detiene antes si alcanza el fin de carrera superior.
     * @param speed Velocidad de jog en pasos por segundo [step/s].
     */
    void jogUp(uint16_t steps, uint16_t speed);

    /**
     * @brief Detiene inmediatamente la emision de pulsos del eje sin desenergizar las bobinas.
     *
     * Seguro para invocarse desde el super-loop.
     */
    void stop();

    /**
     * @brief Habilita o deshabilita la etapa de potencia del driver paso a paso (pin EN).
     *
     * @param enable true para energizar el motor; false para liberar el par (usado en ErrorStop).
     */
    void setDriverEnabled(bool enable);

    /**
     * @brief Recalcula la velocidad instantanea de la rampa trapezoidal y supervisa las fases de homing.
     *
     * Debe invocarse periodicamente desde la tarea de perfiles de movimiento (cada 20 ms).
     */
    void updateMotionProfile();

    /**
     * @brief Consulta si el eje se encuentra ejecutando un movimiento o secuencia de homing.
     * @return true si el eje esta en movimiento; false si esta detenido.
     */
    bool isBusy() const;

    /**
     * @brief Indica si el eje alcanzo satisfactoriamente la ultima cota comandada por moveToTarget().
     * @return true si el movimiento termino en targetSteps sin fallos; false en caso contrario.
     */
    bool isTargetReached() const;

    /**
     * @brief Indica si el eje completo la secuencia de homing y su origen cero es valido.
     * @return true si el eje esta referenciado en home; false si requiere homing.
     */
    bool isHome() const;

    /**
     * @brief Consulta si la ISR detuvo el eje por rebasar el limite maximo descendente (ERR-202 / ERR-212).
     * @return true si se disparo la proteccion de soft limit; false en operacion normal.
     */
    bool hasSoftLimitTripped() const;

    /**
     * @brief Limpia la bandera de fallo por soft limit tras un rearme.
     */
    void clearSoftLimitFault();

    /**
     * @brief Obtiene de forma atomica la posicion actual del eje desde home.
     * @return Posicion actual en micropasos [step] (0 = tope superior, positivo = hacia abajo).
     */
    int32_t getCurrentSteps() const;

    /**
     * @brief Despachador estatico invocado exclusivamente desde ISR(TIMER3_COMPA_vect) a 20 kHz.
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
