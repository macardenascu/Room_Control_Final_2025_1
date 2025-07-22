# 🚀 Guía de Implementación - Sistema de Control de Sala

**¡Bienvenidos al proyecto final!** Este documento les guiará paso a paso para completar el Sistema de Control de Acceso y Climatización.

## 📋 Estado Actual del Proyecto

### ✅ **Lo que YA está implementado (pueden usarlo directamente):**

#### **Hardware Configurado:**
- 🔌 **Teclado matricial 4x4** - Completamente funcional con interrupciones y debouncing
- 📺 **Pantalla OLED SSD1306** - Driver completo con funciones de escritura
- 🔄 **Timer PWM (TIM3)** - Configurado para control de ventilador en PA6
- 💾 **DMA** - Configurado para transiciones suaves del ventilador
- 📡 **USART2** - Debug/consola local funcionando
- 🚪 **GPIO PA4** - Pin para simulador de cerradura (DOOR_STATUS)

#### **Software Base:**
- 🔄 **Ring Buffer** - Librería completa para manejo de datos
- 🎛️ **Keypad Driver** - Funciones `keypad_init()` y `keypad_scan()`
- 📺 **Display Functions** - `write_to_oled()` y funciones SSD1306
- 🏗️ **State Machine Framework** - Estructura completa en `room_control.h/.c`
- ⚡ **Super Loop** - Bucle principal no-bloqueante configurado

#### **Demos Funcionando:**
- ✨ Teclado muestra teclas presionadas en pantalla
- ✨ Botón azul de la placa funciona
- ✨ Comunicación UART2 funciona
- ✨ LED heartbeat indica que el sistema está vivo

---

## 🎯 **Lo que DEBEN implementar (sus tareas):**

### 📅 **Cronograma Sugerido de Implementación**

---

## 🗓️ **ETAPA 1: Configuración de Hardware Faltante**

### **Tarea 1.1: Configurar Sensor de Temperatura (ADC)**
📍 **Archivo:** `Room_Control_Final_2025_1.ioc`

**Pasos:**
1. Abrir el archivo `.ioc` en STM32CubeMX
2. Configurar **ADC1** en un pin disponible (recomendado: **PA0** o **PA1**)
3. Configurar el canal ADC con resolución de 12 bits
4. Habilitar la interrupción ADC en NVIC
5. Regenerar código (**GENERATE CODE**)

**Resultado esperado:** Nuevo pin ADC configurado y funciones HAL generadas.

### **Tarea 1.2: Configurar ESP-01 (USART3)**
📍 **Archivo:** `Room_Control_Final_2025_1.ioc`

**Pasos:**
1. Configurar **USART3** en pines disponibles (recomendado: **PC4/PC5**)
2. Configurar baudrate a **115200**
3. Habilitar interrupción USART3 en NVIC
4. Regenerar código

**Resultado esperado:** USART3 configurado para comunicación con ESP-01.

---

## 🗓️ **ETAPA 2: Lógica de Estado Central**

### **Tarea 2.1: Implementar Máquina de Estados**
📍 **Archivo:** `Core/Src/room_control.c`

**Buscar comentarios:** `// TODO: TAREA -`

#### **2.1.1 Estado LOCKED:**
```c
case ROOM_STATE_LOCKED:
    // IMPLEMENTAR:
    // - Mostrar "SISTEMA BLOQUEADO" en display
    // - Asegurar puerta cerrada (GPIO PA4 = LOW)
    // - Transición a INPUT_PASSWORD cuando se presione tecla
```

#### **2.1.2 Estado INPUT_PASSWORD:**
```c
case ROOM_STATE_INPUT_PASSWORD:
    // IMPLEMENTAR:
    // - Mostrar asteriscos según input_index
    // - Manejar timeout (10 segundos)
    // - Verificar contraseña al completar 4 dígitos
    // - Ir a UNLOCKED si correcta, ACCESS_DENIED si incorrecta
```

#### **2.1.3 Estado UNLOCKED:**
```c
case ROOM_STATE_UNLOCKED:
    // IMPLEMENTAR:
    // - Mostrar estado completo (temperatura, ventilador)
    // - Mantener puerta abierta (GPIO PA4 = HIGH)
    // - Permitir comandos remotos
```

#### **2.1.4 Estado ACCESS_DENIED:**
```c
case ROOM_STATE_ACCESS_DENIED:
    // IMPLEMENTAR:
    // - Mostrar "ACCESO DENEGADO"
    // - Enviar alerta por ESP-01
    // - Volver a LOCKED después de 3 segundos
```

### **Tarea 2.2: Procesamiento de Teclas**
📍 **Función:** `room_control_process_key()`

**Implementar lógica para:**
- Acumular dígitos en el buffer de entrada
- Verificar contraseña contra el valor almacenado
- Manejar teclas especiales ('*' = volver a bloquear, '#' = confirmar)

### **Tarea 2.3: Activar el Sistema**
📍 **Archivo:** `Core/Src/main.c`

**Descomentar líneas:**
```c
// Línea ~176: room_control_init(&room_system);
// Línea ~193: room_control_update(&room_system);
// Línea ~202: room_control_process_key(&room_system, key);
```

**Resultado esperado:** Sistema de acceso básico funcionando.

---

## 🗓️ **ETAPA 3: Control de Temperatura y Ventilador**

### **Tarea 3.1: Lectura de Temperatura**
📍 **Crear nuevo archivo:** `Core/Src/temperature_sensor.c/.h`

**Implementar funciones:**
```c
float temperature_sensor_read(void);
void temperature_sensor_init(void);
```

**Integrar en main.c:**
```c
// Descomentar líneas ~223-224:
float temperature = temperature_sensor_read();
room_control_set_temperature(&room_system, temperature);
```

### **Tarea 3.2: Control PWM del Ventilador**
📍 **Función:** `room_control_update_fan()` en `room_control.c`

**Implementar:**
```c
static void room_control_update_fan(room_control_t *room) {
    // Calcular valor PWM: 0-99 basado en current_fan_level (0-100%)
    uint32_t pwm_value = (room->current_fan_level * 99) / 100;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_value);
    
    // Iniciar PWM si no está corriendo
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
}
```

### **Tarea 3.3: Control Automático del Ventilador**
📍 **Función:** `room_control_calculate_fan_level()` en `room_control.c`

**Ya implementada - solo verificar que funcione correctamente.**

**Resultado esperado:** Ventilador responde automáticamente a cambios de temperatura.

---

## 🗓️ **ETAPA 4: Conectividad y Comandos Remotos**

### **Tarea 4.1: Parser de Comandos**
📍 **Crear nuevo archivo:** `Core/Src/command_parser.c/.h`

**Comandos a implementar:**
- `GET_TEMP` → Devolver temperatura actual
- `GET_STATUS` → Estado sistema (LOCKED/UNLOCKED, fan level)
- `SET_PASS:XXXX` → Cambiar contraseña
- `FORCE_FAN:N` → Forzar velocidad ventilador (N=0,1,2,3)

### **Tarea 4.2: Comunicación USART3**
📍 **Archivo:** `Core/Src/main.c`

**Implementar callbacks:**
```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        // Procesar comandos del ESP-01
        command_parser_process_esp01(rx_byte);
    } else if (huart->Instance == USART2) {
        // Procesar comandos de debug local
        command_parser_process_debug(rx_byte);
    }
}
```

### **Tarea 4.3: Alertas de Internet**
📍 **Función:** Estado `ACCESS_DENIED` en `room_control.c`

**Enviar comando al ESP-01:**
```c
// Ejemplo de alerta HTTP
char alert_msg[] = "POST /alert HTTP/1.1\r\nHost: mi-servidor.com\r\n\r\nAcceso denegado detectado\r\n";
HAL_UART_Transmit(&huart3, (uint8_t*)alert_msg, strlen(alert_msg), 1000);
```

**Resultado esperado:** Sistema completamente funcional con control remoto.

---

## 🎯 **Checklist de Funcionalidades**

### **Funcionalidad Básica:**
- [Si] Sistema enciende y muestra estado inicial
- [Si] Teclado permite entrada de 4 dígitos
- [Si] Contraseña correcta desbloquea el sistema
- [Si] Contraseña incorrecta muestra error
- [Si] Display muestra estado actual claramente
- [ ] Puerta (LED) responde al estado del sistema
- [Si] Super loop no usa `HAL_Delay()`

### **Funcionalidad Avanzada:**
- [Si] Sensor de temperatura funciona
- [Si] Ventilador responde automáticamente a temperatura
- [Si] 4 niveles de velocidad implementados correctamente
- [ ] Comandos remotos por USART2 funcionan
- [ ] Comandos remotos por USART3 funcionan
- [ ] Alertas de internet al acceso denegado

### **Funcionalidad Bonus (+10% extra de la nota final):**
- [ ] Transiciones suaves de ventilador con DMA
- [ ] Sincronización de hora via HTTP
- [ ] Logs a IoT (ThingSpeak/MQTT)

---

## 🎓 **Consejos para el Éxito**

1. **Implementen incrementalmente** - Una funcionalidad a la vez
2. **Prueben frecuentemente** - Compilen y prueben después de cada cambio
3. **Usen los TODOs** - Siguen la guía paso a paso en el código
4. **Documenten su progreso** - Mantengan notas para el informe final
5. **Trabajen en equipo** - Dividan tareas pero integren frecuentemente

---

## 🚨 **Entrega Final**

### **Código Fuente:**
- Repositorio Git con historial de commits
- Código comentado y bien estructurado
- README actualizado con instrucciones de uso

### **Documentación (`INFORME.md`):**
- Arquitectura de hardware con diagramas
- Explicación de patrones de diseño utilizados
- Protocolo de comandos implementados
- Análisis de optimizaciones aplicadas

### **Presentación:**
- Demo en vivo del sistema funcionando
- Explicación de arquitectura y decisiones de diseño
- 5-7 minutos de presentación

---

**¡Éxito en su proyecto! 🚀**

*Recuerden: El objetivo no es solo que funcione, sino que entiendan cómo y por qué funciona.*