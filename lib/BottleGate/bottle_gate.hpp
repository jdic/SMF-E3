/**
 * @file bottle_gate.hpp
 * @brief Controlador HAL no bloqueante para el par de servomotores (brazo de tope y brazo de sujecion) de un automata.
 */

#pragma once
#include <Arduino.h>
#include <Servo.h>

/**
 * @brief Parametros de configuracion fisica y temporal para una instancia de BottleGate.
 */
struct BottleGateConfig
{
    uint8_t  stopPin;      // Pin GPIO del servomotor de tope (HardwarePins)
    uint8_t  clampPin;     // Pin GPIO del servomotor de sujecion (HardwarePins)
    uint8_t  stopUpDeg;    // Angulo de reposo (arriba) del brazo de tope en grados [0..180]
    uint8_t  stopDownDeg;  // Angulo de bloqueo (abajo) del brazo de tope en grados [0..180]
    uint8_t  clampUpDeg;   // Angulo de reposo (arriba) del brazo de sujecion en grados [0..180]
    uint8_t  clampDownDeg; // Angulo de retencion (abajo) del brazo de sujecion en grados [0..180]
    uint16_t armMoveMs;    // Tiempo estimado de recorrido fisico de un brazo en milisegundos [ms]
    uint16_t releaseGapMs; // Espera escalonada entre subir sujecion y subir tope en milisegundos [ms]
};

/**
 * @class BottleGate
 * @brief Coordina de forma no bloqueante los dos brazos con servomotor (tope y sujecion) de una estacion.
 *
 * Opera sobre servomotores sin retroalimentacion de posicion mediante temporizaciones con millis().
 * En la liberacion (releaseBottle), levanta primero el brazo de sujecion y, tras releaseGapMs,
 * levanta el brazo de tope para que la botella salga suavemente empujada por la banda.
 */
class BottleGate
{
public:
    /**
     * @brief Construye el controlador de compuerta de botellas con su configuracion de pines, angulos y tiempos.
     *
     * @param config Estructura con pines, angulos [grados] y tiempos de maniobra [ms].
     */
    explicit BottleGate(const BottleGateConfig& config);

    /**
     * @brief Adjunta ambos servomotores al Timer5 y posiciona los dos brazos arriba (reposo).
     */
    void init();

    /**
     * @brief Baja el brazo de tope para detener el avance de la botella entrante.
     *
     * Funcion no bloqueante; inicia el temporizador de asentamiento de armMoveMs.
     */
    void lowerStopArm();

    /**
     * @brief Baja el brazo de sujecion para inmovilizar la botella contra el brazo de tope.
     *
     * Funcion no bloqueante; inicia el temporizador de asentamiento de armMoveMs.
     */
    void lowerClampArm();

    /**
     * @brief Inicia la secuencia escalonada de liberacion: sube sujecion de inmediato y programa subir el tope.
     *
     * Funcion no bloqueante; requiere llamadas periodicas a update() para completar la segunda etapa.
     */
    void releaseBottle();

    /**
     * @brief Sube inmediatamente ambos brazos (sujecion y tope) al mismo tiempo.
     *
     * Utilizado en la inicializacion (Boot), entrada a Idle y comando manual ReleaseArms.
     */
    void raiseArmsImmediate();

    /**
     * @brief Gestiona la transicion escalonada de releaseBottle() y el estado de asentamiento temporal.
     *
     * Debe invocarse periodicamente desde la tarea de control del super-loop (cada 1 ms).
     */
    void update();

    /**
     * @brief Indica si el ultimo movimiento ordenado a los servomotores ya cumplio su tiempo fisico de recorrido.
     * @return true si no hay etapas pendientes y transcurrio armMoveMs desde el ultimo comando; false en caso contrario.
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
