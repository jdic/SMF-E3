/**
 * @file bottle_gate.hpp
 * @brief Controlador HAL no bloqueante para el par de servomotores (brazo de tope y brazo de sujeción) de un autómata.
 */

#pragma once
#include <Arduino.h>
#include <Servo.h>

/**
 * @struct BottleGateConfig
 * @brief Parámetros físicos y temporales para configurar una instancia de BottleGate.
 */
struct BottleGateConfig
{
    uint8_t  stopPin;      ///< Pin GPIO del servomotor de tope (HardwarePins)
    uint8_t  clampPin;     ///< Pin GPIO del servomotor de sujeción (HardwarePins)
    uint8_t  stopUpDeg;    ///< Ángulo de reposo (arriba) del brazo de tope en grados [0..180]
    uint8_t  stopDownDeg;  ///< Ángulo de bloqueo (abajo) del brazo de tope en grados [0..180]
    uint8_t  clampUpDeg;   ///< Ángulo de reposo (arriba) del brazo de sujeción en grados [0..180]
    uint8_t  clampDownDeg; ///< Ángulo de retención (abajo) del brazo de sujeción en grados [0..180]
    uint16_t armMoveMs;    ///< Tiempo estimado de recorrido mecánico de un brazo en milisegundos [ms]
    uint16_t releaseGapMs; ///< Espera escalonada entre subir sujeción y subir tope en milisegundos [ms]
};

/**
 * @class BottleGate
 * @brief Coordina de forma no bloqueante los dos brazos con servomotor (tope y sujeción) de una estación.
 *
 * Opera sobre servomotores sin retroalimentación de posición mediante temporizaciones con millis().
 * En la liberación (releaseBottle), levanta primero el brazo de sujeción y, tras releaseGapMs,
 * levanta el brazo de tope para que la botella salga suavemente empujada por la banda en movimiento.
 */
class BottleGate
{
public:
    /**
     * @brief Construye el controlador de compuerta de botellas con su configuración de pines, ángulos y tiempos.
     *
     * @param config Estructura con pines GPIO, ángulos admisibles [0..180 grados] y tiempos de maniobra [ms].
     */
    explicit BottleGate(const BottleGateConfig& config);

    /**
     * @brief Precarga los ángulos de reposo, adjunta ambos servomotores al Timer5 y posiciona los brazos arriba.
     */
    void init();

    /**
     * @brief Comanda el descenso del brazo de tope para frenar el avance de la botella entrante.
     *
     * Función no bloqueante; inicia el temporizador de asentamiento de armMoveMs.
     */
    void lowerStopArm();

    /**
     * @brief Comanda el descenso del brazo de sujeción para inmovilizar la botella contra el brazo de tope.
     *
     * Función no bloqueante; inicia el temporizador de asentamiento de armMoveMs.
     */
    void lowerClampArm();

    /**
     * @brief Inicia la secuencia escalonada de liberación: sube sujeción de inmediato y agenda subir el tope.
     *
     * Función no bloqueante; requiere llamadas periódicas a update() para ejecutar la segunda etapa tras releaseGapMs.
     */
    void releaseBottle();

    /**
     * @brief Levanta simultáneamente ambos brazos (sujeción y tope) hacia sus ángulos de reposo.
     *
     * Función no bloqueante. Utilizada en Boot, entrada a Idle y comando táctil ReleaseArms.
     */
    void raiseArmsImmediate();

    /**
     * @brief Gestiona la transición escalonada de releaseBottle() y el temporizador de asentamiento mecánico.
     *
     * Función no bloqueante. Debe invocarse desde la tarea de control del super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Verifica si la última maniobra ordenada a los servomotores ya cumplió su tiempo físico de recorrido.
     * @return true si no existen etapas pendientes y transcurrió armMoveMs desde el último comando; false si sigue en tránsito.
     */
    bool isSettled() const;

private:
    BottleGateConfig config_;
    Servo            stopServo_;
    Servo            clampServo_;
    uint32_t         actionStartMs_;
    bool             pendingStopArmUp_;
    bool             settled_;
};
