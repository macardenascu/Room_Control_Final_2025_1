#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
#include <stdio.h>

// Extern handles for hardware
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;

// Hardware mapping
#define FAN_PWM_TIMER           htim3
#define FAN_PWM_CHANNEL         TIM_CHANNEL_2
#define DOOR_LOCK_GPIO_Port     DOOR_STATUS_GPIO_Port
#define DOOR_LOCK_Pin           DOOR_STATUS_Pin

// System constants
static const char DEFAULT_PASSWORD[] = "0000";

// Temperature thresholds for automatic fan control
static const float TEMP_THRESHOLD_LOW = 25.0f;
static const float TEMP_THRESHOLD_MED = 28.0f;
static const float TEMP_THRESHOLD_HIGH = 31.0f;

// Timeouts in milliseconds
static const uint32_t INPUT_TIMEOUT_MS = 20000;  // 20 seconds
static const uint32_t ACCESS_DENIED_TIMEOUT_MS = 5000;  // 5 seconds

// Private function prototypes
static void room_control_change_state(room_control_t *room, room_state_t new_state);
static void room_control_update_display(room_control_t *room);
static void room_control_update_door(room_control_t *room);
static void room_control_update_fan_pwm(room_control_t *room);
static fan_level_t room_control_calculate_fan_level(float temperature);
static void room_control_clear_input(room_control_t *room);

void room_control_init(room_control_t *room) {
    // Initialize room control structure
    memset(room, 0, sizeof(room_control_t)); // Clear the whole structure first
    strcpy(room->password, DEFAULT_PASSWORD);
    room->current_state = ROOM_STATE_LOCKED;
    room->state_enter_time = HAL_GetTick();
    
    // Initialize door control
    room->door_locked = true;
    
    // Initialize temperature and fan
    room->current_temperature = 22.0f;  // Default room temperature
    room->current_fan_level = FAN_LEVEL_OFF;
    room->manual_fan_override = false;
    
    // Display
    room->display_update_needed = true;
    
    // Initialize hardware
    HAL_GPIO_WritePin(DOOR_LOCK_GPIO_Port, DOOR_LOCK_Pin, GPIO_PIN_RESET); // RESET = Bloqueado
    HAL_TIM_PWM_Start(&FAN_PWM_TIMER, FAN_PWM_CHANNEL);
    room_control_update_fan_pwm(room); // Establecer PWM inicial a 0%
}
// --- Actualiza el estado del sistema y maneja la lógica de entrada de contraseña ---
/// @param room Puntero al sistema de control de habitación
/// @note Esta función se llama periódicamente para actualizar el estado del sistema
void room_control_update(room_control_t *room) {
    uint32_t current_time = HAL_GetTick();
    
    // State machine logic
    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            // No se necesita acción continua aquí. Las transiciones son por eventos (teclas).
            break;
            
        case ROOM_STATE_INPUT_PASSWORD:
            // Timeout para la entrada de contraseña. Si el usuario no hace nada, se bloquea.
            if (current_time - room->last_input_time > INPUT_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;
            
        case ROOM_STATE_UNLOCKED:
            // La transición a LOCKED se maneja por evento de tecla ('*').
            // Se podría añadir un auto-bloqueo por tiempo aquí si se deseara.
            break;
            
        case ROOM_STATE_ACCESS_DENIED:
            // Muestra "ACCESO DENEGADO" y vuelve a LOCKED después de un tiempo.
            if (current_time - room->state_enter_time > ACCESS_DENIED_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;
            
        case ROOM_STATE_EMERGENCY:
            // Lógica de emergencia (opcional).
            break;
    }
    
    // Update physical subsystems if needed
    // room_control_update_door(room); // Se llama solo al cambiar de estado para eficiencia
    // room_control_update_fan_pwm(room); // Se llama cuando cambia el nivel del ventilador
    
    if (room->display_update_needed) {
        room_control_update_display(room);
        room->display_update_needed = false;
    }
}
// --- Procesa una tecla del teclado y actualiza el estado del sistema ---
/// @param room Puntero al sistema de control de habitación
/// @param key El carácter de la tecla presionada
/// @note Esta función maneja la lógica de entrada de contraseña y transiciones de estado
void room_control_process_key(room_control_t *room, char key) {
    room->last_input_time = HAL_GetTick();

    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            // Para entrar al modo de ingreso de clave, se presiona cualquier dígito.
            if (key >= '0' && key <= '9') {
                room_control_change_state(room, ROOM_STATE_INPUT_PASSWORD);
                // Procesa la primera tecla inmediatamente
                room_control_process_key(room, key);
            }
            break;
            
        case ROOM_STATE_INPUT_PASSWORD:
            if (key >= '0' && key <= '9' && room->input_index < PASSWORD_LENGTH) {
                room->input_buffer[room->input_index++] = key;
                room->display_update_needed = true;

                // Validar automáticamente al alcanzar la longitud de la contraseña
                if (room->input_index == PASSWORD_LENGTH) {
                    room->input_buffer[room->input_index] = '\0';
                    if (strcmp(room->input_buffer, room->password) == 0) {
                        room_control_change_state(room, ROOM_STATE_UNLOCKED);
                    } else {
                        room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                    }
                }
            } else if (key == '#') { // Cancelar y volver a bloquear
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;
            
        case ROOM_STATE_UNLOCKED:
            if (key == '*') { // Bloquear el sistema
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            // Aquí se podrían añadir más comandos para el modo desbloqueado
            break;

        default:
            break;
    }
}
// --- CORRECCIÓN CRÍTICA: Establece la temperatura actual y actualiza el ventilador ---
/// @brief Establece la temperatura actual y actualiza el nivel del ventilador
/// @param room Puntero al sistema de control de habitación
/// @param temperature La temperatura a establecer

void room_control_set_temperature(room_control_t *room, float temperature) {
    // Usar histéresis para evitar cambios constantes si la temperatura fluctúa poco
    if (temperature > room->current_temperature + 0.5f || temperature < room->current_temperature - 0.5f) {
        room->current_temperature = temperature;
        
        if (!room->manual_fan_override) {
            fan_level_t new_level = room_control_calculate_fan_level(temperature);
            if (new_level != room->current_fan_level) {
                room->current_fan_level = new_level;
                room_control_update_fan_pwm(room);
            }
        }
        // Solo actualizar el display si el sistema está desbloqueado
        if (room->current_state == ROOM_STATE_UNLOCKED) {
            room->display_update_needed = true;
        }
    }
}
// --- CORRECCIÓN CRÍTICA: Fuerza un nivel de ventilador específico ---
/// @brief Fuerza un nivel de ventilador específico, ignorando la temperatura
/// @param room Puntero al sistema de control de habitación
/// @param level El nivel de ventilador a establecer
void room_control_force_fan_level(room_control_t *room, fan_level_t level) {
    if (room->current_state == ROOM_STATE_UNLOCKED) {
        room->manual_fan_override = true;
        if (level != room->current_fan_level) {
            room->current_fan_level = level;
            // *** CORRECCIÓN CRÍTICA ***
            room_control_update_fan_pwm(room); // Se corrigió la llamada a la función
            room->display_update_needed = true;
        }
    }
}

void room_control_change_password(room_control_t *room, const char *new_password) {
    if (strlen(new_password) == PASSWORD_LENGTH) {
        strcpy(room->password, new_password);
    }
}

// --- Getters ---
room_state_t room_control_get_state(room_control_t *room) { return room->current_state; }
bool room_control_is_door_locked(room_control_t *room) { return room->door_locked; }
fan_level_t room_control_get_fan_level(room_control_t *room) { return room->current_fan_level; }
float room_control_get_temperature(room_control_t *room) { return room->current_temperature; }

// --- Private functions ---
/// @brief Cambia el estado del sistema y actualiza el display
/// @param room Puntero al sistema de control de habitación
/// @param new_state El nuevo estado al que se cambiará
static void room_control_change_state(room_control_t *room, room_state_t new_state) {
    if (room->current_state == new_state) return; // Evitar re-entrar al mismo estado

    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->display_update_needed = true;
    
    // Acciones al entrar a un nuevo estado
    switch (new_state) {
        case ROOM_STATE_LOCKED:
            room->door_locked = true;
            room_control_clear_input(room);
            room->manual_fan_override = false; // El control del ventilador vuelve a ser automático
            // Recalcular nivel del ventilador por si la temperatura cambió mientras estaba desbloqueado
            room->current_fan_level = room_control_calculate_fan_level(room->current_temperature);
            room_control_update_fan_pwm(room);
            break;
            
        case ROOM_STATE_UNLOCKED:
            room->door_locked = false;
            break;
            
        case ROOM_STATE_INPUT_PASSWORD:
            room_control_clear_input(room);
            room->last_input_time = HAL_GetTick(); // Iniciar temporizador de timeout
            break;
            
        case ROOM_STATE_ACCESS_DENIED:
            room_control_clear_input(room);
            // Aquí se podría enviar una alerta por UART al ESP-01
            // HAL_UART_Transmit(&huart2, (uint8_t*)"ALERT:FAIL_LOGIN\r\n", 18, 100);
            break;
            
        default:
            break;
    }
    
    room_control_update_door(room); // Actualizar estado físico de la puerta
}
// --- Actualiza el display OLED con el estado actual del sistema ---
/// @param room Puntero al sistema de control de habitación
/// @note Esta función se llama al cambiar de estado o cuando se necesita actualizar el display
///       para reflejar el estado actual del sistema.
static void room_control_update_display(room_control_t *room) {
    char display_buffer[32];
    ssd1306_Fill(Black);

    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            ssd1306_SetCursor(25, 10);
            ssd1306_WriteString("SISTEMA", Font_7x10, White);
            ssd1306_SetCursor(15, 30);
            ssd1306_WriteString("BLOQUEADO", Font_7x10, White);
            break;

        case ROOM_STATE_INPUT_PASSWORD:
            ssd1306_SetCursor(5, 5);
            ssd1306_WriteString("INGRESE CLAVE:", Font_7x10, White);
            
            char masked_input[PASSWORD_LENGTH + 1] = {0};
            for (uint8_t i = 0; i < room->input_index; i++) {
                masked_input[i] = '*';
            }
            
            ssd1306_SetCursor(40, 25);
            ssd1306_WriteString(masked_input, Font_11x18, White);
            break;

        case ROOM_STATE_UNLOCKED:
            ssd1306_SetCursor(5, 5);
            ssd1306_WriteString("ACCESO PERMITIDO", Font_7x10, White);

            // *** MEJORA: Mostrar temperatura con un decimal ***
            snprintf(display_buffer, sizeof(display_buffer), "Temp: %.1f C", room->current_temperature);
            ssd1306_SetCursor(5, 22);
            ssd1306_WriteString(display_buffer, Font_7x10, White);
           
            const char* fan_mode = room->manual_fan_override ? "MAN" : "AUTO";
            // *** MEJORA: Mostrar nivel del ventilador como porcentaje ***
            snprintf(display_buffer, sizeof(display_buffer), "Fan(%s): %d%%", fan_mode, (int)room->current_fan_level);
            ssd1306_SetCursor(5, 38);
            ssd1306_WriteString(display_buffer, Font_7x10, White);
            break;

        case ROOM_STATE_ACCESS_DENIED:
            ssd1306_SetCursor(25, 10);
            ssd1306_WriteString("ACCESO", Font_7x10, White);
            ssd1306_SetCursor(15, 30);
            ssd1306_WriteString("DENEGADO", Font_7x10, White);
            break;

        default:
            break;
    }

    ssd1306_UpdateScreen(); 
}
// --- CORRECCIÓN CRÍTICA: Actualiza el estado de la puerta ---
/// @brief Actualiza el estado físico de la puerta según el estado actual   
/// @param room Puntero al sistema de control de habitación
/// @note Esta función se llama al cambiar de estado para reflejar el bloqueo/desbloqueo
///       de la puerta en el hardware el LED conectado a PA4.
static void room_control_update_door(room_control_t *room) {
    // *** CORRECCIÓN: Lógica de control de puerta activada ***
    if (room->door_locked) {
        HAL_GPIO_WritePin(DOOR_LOCK_GPIO_Port, DOOR_LOCK_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(DOOR_LOCK_GPIO_Port, DOOR_LOCK_Pin, GPIO_PIN_SET);
    }
}
/// @brief Actualiza el PWM del ventilador basado en el nivel actual
/// @param room Puntero al sistema de control de habitación
/// @note Esta función se llama cada vez que cambia el nivel del ventilador
///       o al iniciar el sistema. Asegura que el PWM del ventilador
static void room_control_update_fan_pwm(room_control_t *room) {
    // El periodo de TIM3 se configuró a 100, así que el nivel de fan (0-100) mapea directamente.
    uint32_t pwm_pulse = (uint32_t)room->current_fan_level;
    __HAL_TIM_SET_COMPARE(&FAN_PWM_TIMER, FAN_PWM_CHANNEL, pwm_pulse);
}

/// @brief Calcula el nivel del ventilador basado en la temperatura
/// @param temperature La temperatura actual
/// @return El nivel del ventilador correspondiente
static fan_level_t room_control_calculate_fan_level(float temperature) {
    if (temperature < TEMP_THRESHOLD_LOW)       return FAN_LEVEL_OFF;
    else if (temperature < TEMP_THRESHOLD_MED)  return FAN_LEVEL_LOW;
    else if (temperature < TEMP_THRESHOLD_HIGH) return FAN_LEVEL_MED;
    else                                        return FAN_LEVEL_HIGH;
}
/// @brief Limpia el buffer de entrada y resetea el índice
/// @param room Puntero al sistema de control de habitación     
/// @note Esta función se usa para reiniciar la entrada del usuario
///       cuando se cambia de estado o se cancela la entrada.
static void room_control_clear_input(room_control_t *room) {
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    room->input_index = 0;
}