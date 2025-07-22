// dht11.c (VERSIÓN CORREGIDA Y ROBUSTA)
#include "dht11.h"
#include <string.h>

//--- Umbrales de tiempo en microsegundos (µs) ---
#define START_PULLDOWN_MS            20    // 20ms de pulso de inicio
#define RESPONSE_TIMEOUT_US          100   // Timeout para la respuesta inicial del sensor
#define BIT_READ_TIMEOUT_US          120   // Timeout máximo para leer un pulso
#define BIT_HIGH_PULSE_THRESHOLD_US  45    // Umbral para decidir entre '0' y '1'. (Un '0' dura ~28us, un '1' ~70us)

//--- Máquina de Estados Simplificada ---
typedef enum {
    DHT11_STATE_IDLE,
    DHT11_STATE_START_PULLDOWN,
    DHT11_STATE_START_PULLUP,
    DHT11_STATE_WAIT_RESPONSE_LOW,
    DHT11_STATE_WAIT_RESPONSE_HIGH,
    DHT11_STATE_READ_BITS,          // <-- CAMBIO CLAVE: Un solo estado para leer todos los bits
    DHT11_STATE_COMPLETE,
    DHT11_STATE_ERROR
} DHT11_State_t;

//--- Variables estáticas del módulo ---
static TIM_HandleTypeDef* dht_timer;
static DHT11_State_t current_state = DHT11_STATE_IDLE;

static uint32_t last_event_time_ms = 0;
static uint16_t last_event_time_us = 0;

static float last_temperature = 0.0f;
static float last_humidity = 0.0f;
static bool data_ready_flag = false;

// --- Funciones auxiliares de Pin ---
static void DHT11_Set_Pin_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_Set_Pin_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// Función para reiniciar el estado en caso de error o finalización
static void reset_to_idle(void) {
    current_state = DHT11_STATE_IDLE;
    // Dejar el pin en modo input con pull-up es más seguro que output-high
    // para no interferir si otro dispositivo comparte la línea (aunque aquí no sea el caso).
    DHT11_Set_Pin_Input(); 
}

// --- Funciones Públicas ---
void DHT11_Init(TIM_HandleTypeDef *htim) {
    dht_timer = htim;
    HAL_TIM_Base_Start(dht_timer);
    reset_to_idle();
}

bool DHT11_StartReading(void) {
    if (current_state != DHT11_STATE_IDLE) {
        return false;
    }

    DHT11_Set_Pin_Output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    last_event_time_ms = HAL_GetTick();
    current_state = DHT11_STATE_START_PULLDOWN;
    data_ready_flag = false;
    return true;
}

bool DHT11_IsDataReady(void) { return data_ready_flag; }

bool DHT11_GetNewData(float* temperature, float* humidity) {
    if (data_ready_flag) {
        *temperature = last_temperature;
        *humidity = last_humidity;
        data_ready_flag = false; // La bandera se resetea al leer los datos
        return true;
    }
    return false;
}

// *** CAMBIO CLAVE: Función de lectura de bits bloqueante ***
// Esta función se llama una sola vez y lee los 40 bits de golpe.
// Es mucho más fiable por ser bloqueante, pero suficientemente rápida (<5ms).
static bool read_data_bits(uint8_t* data_out) {
    memset(data_out, 0, 5);

    for (int i = 0; i < 40; i++) {
        uint16_t start_time;
        uint16_t duration;

        // 1. Esperar a que el pin baje (inicio del pulso de sync de 50us)
        start_time = __HAL_TIM_GET_COUNTER(dht_timer);
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
            duration = __HAL_TIM_GET_COUNTER(dht_timer) - start_time;
            if (duration > BIT_READ_TIMEOUT_US) return false; // Timeout
        }

        // 2. Esperar a que el pin suba (fin del pulso de sync, inicio del pulso de datos)
        start_time = __HAL_TIM_GET_COUNTER(dht_timer);
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
            duration = __HAL_TIM_GET_COUNTER(dht_timer) - start_time;
            if (duration > BIT_READ_TIMEOUT_US) return false; // Timeout
        }

        // 3. Medir la duración del pulso alto (el dato en sí)
        start_time = __HAL_TIM_GET_COUNTER(dht_timer);
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
            duration = __HAL_TIM_GET_COUNTER(dht_timer) - start_time;
            if (duration > BIT_READ_TIMEOUT_US) return false; // Timeout
        }

        // Clasificar el bit y guardarlo
        data_out[i / 8] <<= 1;
        if (duration > BIT_HIGH_PULSE_THRESHOLD_US) {
            data_out[i / 8] |= 1;
        }
    }
    return true;
}


void DHT11_Process(void) {
    uint16_t current_time_us = __HAL_TIM_GET_COUNTER(dht_timer);
    uint16_t elapsed_us = current_time_us - last_event_time_us;

    switch (current_state) {
        case DHT11_STATE_IDLE:
            break;

        case DHT11_STATE_START_PULLDOWN:
            if (HAL_GetTick() - last_event_time_ms >= START_PULLDOWN_MS) {
                DHT11_Set_Pin_Input(); // Soltar el pin
                last_event_time_us = __HAL_TIM_GET_COUNTER(dht_timer);
                current_state = DHT11_STATE_WAIT_RESPONSE_LOW;
            }
            break;

        case DHT11_STATE_WAIT_RESPONSE_LOW:
            if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
                last_event_time_us = __HAL_TIM_GET_COUNTER(dht_timer);
                current_state = DHT11_STATE_WAIT_RESPONSE_HIGH;
            } else if (elapsed_us > RESPONSE_TIMEOUT_US) {
                reset_to_idle();
            }
            break;

        case DHT11_STATE_WAIT_RESPONSE_HIGH:
            if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
                // Respuesta del sensor recibida, ahora leemos todos los bits
                current_state = DHT11_STATE_READ_BITS;
            } else if (elapsed_us > RESPONSE_TIMEOUT_US) {
                reset_to_idle();
            }
            break;

        case DHT11_STATE_READ_BITS:
        {
            uint8_t data_bytes[5];
            if (read_data_bits(data_bytes)) {
                // Verificación del Checksum
                uint8_t sum = data_bytes[0] + data_bytes[1] + data_bytes[2] + data_bytes[3];
                if (sum == data_bytes[4]) {
                    // Datos válidos
                    last_humidity    = (float)data_bytes[0] + ((float)data_bytes[1] * 0.1f);
                    last_temperature = (float)data_bytes[2] + ((float)data_bytes[3] * 0.1f);
                    data_ready_flag = true;
                }
            }
            // Haya funcionado o no, la lectura ha terminado. Volvemos a idle.
            reset_to_idle();
        }
            break;

        // Los estados COMPLETE y ERROR ya no son necesarios, se manejan dentro de READ_BITS
        default:
            reset_to_idle();
            break;
    }
}