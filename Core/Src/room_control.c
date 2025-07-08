#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
#include <stdio.h>

// Default password
static const char DEFAULT_PASSWORD[] = "7777";

// Temperature thresholds for automatic fan control
static const float TEMP_THRESHOLD_LOW = 25.0f;
static const float TEMP_THRESHOLD_MED = 28.0f;  
static const float TEMP_THRESHOLD_HIGH = 31.0f;

// Timeouts in milliseconds
static const uint32_t INPUT_TIMEOUT_MS = 20000;  // 20 seconds
static const uint32_t ACCESS_DENIED_TIMEOUT_MS = 5000;  // 3 seconds

// Private function prototypes
static void room_control_change_state(room_control_t *room, room_state_t new_state);
static void room_control_update_display(room_control_t *room);
static void room_control_update_door(room_control_t *room);
static void room_control_update_fan(room_control_t *room);
static fan_level_t room_control_calculate_fan_level(float temperature);
static void room_control_clear_input(room_control_t *room);

void room_control_init(room_control_t *room) {
    // Initialize room control structure
    room->current_state = ROOM_STATE_LOCKED;
    strcpy(room->password, DEFAULT_PASSWORD);
    room_control_clear_input(room);
    room->last_input_time = 0;
    room->state_enter_time = HAL_GetTick();
    
    // Initialize door control
    room->door_locked = true;
    
    // Initialize temperature and fan
    room->current_temperature = 22.0f;  // Default room temperature
    room->current_fan_level = FAN_LEVEL_OFF;
    room->manual_fan_override = false;
    
    // Display
    room->display_update_needed = true;
    
    // TODO: TAREA - Initialize hardware (door lock, fan PWM, etc.)
    // Ejemplo: HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
}

void room_control_update(room_control_t *room) {
    uint32_t current_time = HAL_GetTick();
    
    // State machine
    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            // TODO: TAREA - Implementar lógica del estado LOCKED
            // - Mostrar mensaje "SISTEMA BLOQUEADO" en display
            // - Asegurar que la puerta esté cerrada
            // - Transición a INPUT_PASSWORD cuando se presione una tecla
            break;
            
        case ROOM_STATE_INPUT_PASSWORD:
            // TODO: TAREA - Implementar lógica de entrada de contraseña
            // - Mostrar asteriscos en pantalla (**)
            // - Manejar timeout (volver a LOCKED después de 10 segundos sin input)
            // - Verificar contraseña cuando se ingresen 4 dígitos
            
            // Example timeout logic:
            if (current_time - room->last_input_time > INPUT_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;
            
        case ROOM_STATE_UNLOCKED:
            // TODO: TAREA - Implementar lógica del estado UNLOCKED  
            // - Mostrar "ACCESO CONCEDIDO" y temperatura
            // - Mantener puerta abierta
            // - Permitir comandos de control manual
            break;
            
        case ROOM_STATE_ACCESS_DENIED:
            // TODO: TAREA - Implementar lógica de acceso denegado
            // - Mostrar "ACCESO DENEGADO" durante 3 segundos
            // - Enviar alerta a internet via ESP-01 (nuevo requerimiento)
            // - Volver automáticamente a LOCKED
            
            if (current_time - room->state_enter_time > ACCESS_DENIED_TIMEOUT_MS) {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;
            
        case ROOM_STATE_EMERGENCY:
            // TODO: TAREA - Implementar lógica de emergencia (opcional)
            break;
    }
    
    // Update subsystems
    room_control_update_door(room);
    room_control_update_fan(room);
    
    if (room->display_update_needed) {
        room_control_update_display(room);
        room->display_update_needed = false;
    }
}

void room_control_process_key(room_control_t *room, char key) {
    room->last_input_time = HAL_GetTick();

    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            if (key == '#') {
                room_control_clear_input(room);
                room_control_change_state(room, ROOM_STATE_INPUT_PASSWORD);
            }
            break;
            
        case ROOM_STATE_INPUT_PASSWORD:
            if (key == '#') {
            room->input_buffer[room->input_index] = '\0'; // Finaliza el string
            if (strcmp(room->input_buffer, room->password) == 0) {
             room_control_change_state(room, ROOM_STATE_UNLOCKED);
            } else {
            room_control_change_state(room, ROOM_STATE_ACCESS_DENIED); 
            }
                room_control_clear_input(room); // Limpia el buffer después de validación
                } else if (room->input_index < PASSWORD_LENGTH) {
                room->input_buffer[room->input_index++] = key;
            }
            break;

            
        case ROOM_STATE_UNLOCKED:
            if (key == '*') {
                room_control_change_state(room, ROOM_STATE_LOCKED);
            }
            break;

        default:
            break;
    }

    room->display_update_needed = true;
}

void room_control_set_temperature(room_control_t *room, float temperature) {
    room->current_temperature = temperature;
    
    // Update fan level automatically if not in manual override
    if (!room->manual_fan_override) {
        fan_level_t new_level = room_control_calculate_fan_level(temperature);
        if (new_level != room->current_fan_level) {
            room->current_fan_level = new_level;
            room->display_update_needed = true;
        }
    }
}

void room_control_force_fan_level(room_control_t *room, fan_level_t level) {
    room->manual_fan_override = true;
    room->current_fan_level = level;
    room->display_update_needed = true;
}

void room_control_change_password(room_control_t *room, const char *new_password) {
    if (strlen(new_password) == PASSWORD_LENGTH) {
        strcpy(room->password, new_password);
    }
}

// Status getters
room_state_t room_control_get_state(room_control_t *room) {
    return room->current_state;
}

bool room_control_is_door_locked(room_control_t *room) {
    return room->door_locked;
}

fan_level_t room_control_get_fan_level(room_control_t *room) {
    return room->current_fan_level;
}

float room_control_get_temperature(room_control_t *room) {
    return room->current_temperature;
}

// Private functions
static void room_control_change_state(room_control_t *room, room_state_t new_state) {
    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->display_update_needed = true;
    
    // State entry actions
    switch (new_state) {
        case ROOM_STATE_LOCKED:
            room->door_locked = true;
            room_control_clear_input(room);
            break;
            
        case ROOM_STATE_UNLOCKED:
            room->door_locked = false;
            room->manual_fan_override = false;  // Reset manual override
            break;
            
        case ROOM_STATE_ACCESS_DENIED:
            room_control_clear_input(room);
            break;
            
        default:
            break;
    }
}

static void room_control_update_display(room_control_t *room) {
    char display_buffer[32];
    ssd1306_Fill(Black);

    switch (room->current_state) {
        case ROOM_STATE_LOCKED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("SISTEMA", Font_7x10, White);
            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString("BLOQUEADO", Font_7x10, White);
            break;

        case ROOM_STATE_INPUT_PASSWORD:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("INGRESE CLAVE:", Font_7x10, White);

            char masked_input[PASSWORD_LENGTH + 1] = {0};
            for (uint8_t i = 0; i < room->input_index; i++) {
            masked_input[i] = '*';
            }
            masked_input[room->input_index] = '\0';

            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString(masked_input, Font_7x10, White);


            break;

        case ROOM_STATE_UNLOCKED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("ACCESO CONCEDIDO", Font_7x10, White);

            snprintf(display_buffer, sizeof(display_buffer), "Temp: %.1fC", room->current_temperature);
            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString(display_buffer, Font_7x10, White);

            snprintf(display_buffer, sizeof(display_buffer), "Fan: %d%%", room->current_fan_level);
            ssd1306_SetCursor(10, 40);
            ssd1306_WriteString(display_buffer, Font_7x10, White);
            break;

        case ROOM_STATE_ACCESS_DENIED:
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("ACCESO", Font_7x10, White);
            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString("DENEGADO", Font_7x10, White);
            break;

        default:
            break;
    }

    ssd1306_UpdateScreen(); 
}


static void room_control_update_door(room_control_t *room) {
    // TODO: TAREA - Implementar control físico de la puerta
    // Ejemplo usando el pin DOOR_STATUS:
    if (room->door_locked) {
        // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET);
    } else {
        // HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_SET);
    }
}

static void room_control_update_fan(room_control_t *room) {
    // TODO: TAREA - Implementar control PWM del ventilador
    // Calcular valor PWM basado en current_fan_level
    // Ejemplo:
    // uint32_t pwm_value = (room->current_fan_level * 99) / 100;  // 0-99 para period=99
    // __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value);
}

static fan_level_t room_control_calculate_fan_level(float temperature) {
    // TODO: TAREA - Implementar lógica de niveles de ventilador
    if (temperature < TEMP_THRESHOLD_LOW) {
        return FAN_LEVEL_OFF;
    } else if (temperature < TEMP_THRESHOLD_MED) {
        return FAN_LEVEL_LOW;
    } else if (temperature < TEMP_THRESHOLD_HIGH) {
        return FAN_LEVEL_MED;
    } else {
        return FAN_LEVEL_HIGH;
    }
}

static void room_control_clear_input(room_control_t *room) {
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    room->input_index = 0;
}