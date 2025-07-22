// dht11.h
#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#include "main.h"
#include <stdbool.h> // Para usar 'bool', 'true', 'false'

// Asegúrate de que coincidan con tu hardware
#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_PIN_5

/**
 * @brief Inicializa el driver del DHT11.
 * @param htim Puntero al handle del timer configurado a 1MHz (1 tick = 1µs).
 */
void DHT11_Init(TIM_HandleTypeDef *htim);

/**
 * @brief Inicia una nueva secuencia de lectura de datos del sensor.
 *        Esta función no es bloqueante.
 * @return true si la lectura pudo iniciarse (el sensor estaba libre), false si no.
 */
bool DHT11_StartReading(void);

/**
 * @brief Procesa la máquina de estados de la comunicación con el DHT11.
 *        Debe ser llamada continuamente en el bucle principal.
 */
void DHT11_Process(void);

/**
 * @brief Comprueba si hay una nueva lectura de datos válida disponible.
 * @return true si hay nuevos datos, false si no.
 */
bool DHT11_IsDataReady(void);

/**
 * @brief Obtiene el último valor de temperatura y humedad leídos.
 * @param temperature Puntero donde se almacenará la temperatura.
 * @param humidity Puntero donde se almacenará la humedad.
 * @return true si los datos se copiaron con éxito (porque había datos nuevos), false si no.
 *         Al llamar a esta función, se resetea el flag de 'datos listos'.
 */
bool DHT11_GetNewData(float* temperature, float* humidity);

#endif /* INC_DHT11_H_ */