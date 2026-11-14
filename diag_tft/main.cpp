/**
 * @file main.cpp
 * @brief Diagnóstico aislado del bus paralelo TFT (no forma parte del firmware de producción).
 *
 * Compilar/subir con: pio run -e tft_diag -t upload
 *
 * 1) Lee el ID del controlador 50 veces y cuenta cuántas veces cada bit del ID viene en 1.
 *    - ID siempre idéntico  -> línea fija (cable pegado/cortado).
 *    - ID variable          -> ruido o contacto intermitente.
 * 2) Inicializa con 0x9341 y pinta colores sólidos para validar las ESCRITURAS por el arnés.
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

static MCUFRIEND_kbv tft;

static void printBinary16(uint16_t v)
{
    for (int8_t b = 15; b >= 0; --b)
    {
        Serial.print((v >> b) & 1U);
        if ((b % 4) == 0 && b != 0)
        {
            Serial.print('_');
        }
    }
}

/**
 * @brief Escáner de cortos entre líneas de datos D0..D7 (pines Mega 8,9,2,3,4,5,6,7).
 *
 * Maneja una línea a LOW con las demás como INPUT_PULLUP; cualquier otra que lea LOW
 * está puenteada (corto o baja resistencia) con la línea manejada.
 */
static void scanDataBusShorts()
{
    static const uint8_t DATA_PINS[8] = {8, 9, 2, 3, 4, 5, 6, 7};
    bool anyShort = false;

    Serial.println(F("--- Escaner de cortos en D0..D7 ---"));
    for (uint8_t d = 0; d < 8; ++d)
    {
        for (uint8_t i = 0; i < 8; ++i)
        {
            pinMode(DATA_PINS[i], INPUT_PULLUP);
        }
        pinMode(DATA_PINS[d], OUTPUT);
        digitalWrite(DATA_PINS[d], LOW);
        delay(5);

        for (uint8_t i = 0; i < 8; ++i)
        {
            if ((i != d) && (digitalRead(DATA_PINS[i]) == LOW))
            {
                anyShort = true;
                Serial.print(F("CORTO: D"));
                Serial.print(d);
                Serial.print(F(" <-> D"));
                Serial.println(i);
            }
        }
    }
    for (uint8_t i = 0; i < 8; ++i)
    {
        pinMode(DATA_PINS[i], INPUT);
    }
    Serial.println(anyShort ? F("RESULTADO: HAY CORTOS") : F("RESULTADO: sin cortos entre D0..D7"));
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println(F("=== TFT DIAG (arnes) ==="));
    scanDataBusShorts();

    constexpr uint8_t SAMPLES = 50;
    uint8_t bitCount[16] = {0};
    uint16_t lastId      = 0;
    uint8_t  distinct    = 0;
    uint16_t seen[8]     = {0};

    for (uint8_t i = 0; i < SAMPLES; ++i)
    {
        const uint16_t id = tft.readID();
        lastId            = id;

        for (uint8_t b = 0; b < 16; ++b)
        {
            if ((id >> b) & 1U)
            {
                bitCount[b]++;
            }
        }

        bool known = false;
        for (uint8_t k = 0; k < distinct; ++k)
        {
            if (seen[k] == id)
            {
                known = true;
            }
        }
        if (!known && distinct < 8)
        {
            seen[distinct++] = id;
        }
        delay(20);
    }

    Serial.print(F("Ultimo ID: 0x"));
    Serial.println(lastId, HEX);
    Serial.print(F("IDs distintos vistos: "));
    Serial.println(distinct);
    for (uint8_t k = 0; k < distinct; ++k)
    {
        Serial.print(F("  0x"));
        Serial.print(seen[k], HEX);
        Serial.print(F("  "));
        printBinary16(seen[k]);
        Serial.println();
    }

    Serial.println(F("Bit: veces en 1 (de 50)"));
    for (int8_t b = 15; b >= 0; --b)
    {
        Serial.print(F("  bit"));
        Serial.print(b);
        Serial.print(F(": "));
        Serial.println(bitCount[b]);
    }

    Serial.println(F("Inicializando con 0x9341 y pintando colores..."));
    tft.begin(0x9341);
    tft.setRotation(1);
}

void loop()
{
    static const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    static uint8_t        idx      = 0;

    tft.fillScreen(colors[idx]);
    Serial.print(F("Color idx "));
    Serial.println(idx);
    idx = static_cast<uint8_t>((idx + 1U) % (sizeof(colors) / sizeof(colors[0])));
    delay(1500);
}
