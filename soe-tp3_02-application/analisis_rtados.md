# Paso 06: Observación del Comportamiento - Readers-Writers

Se implementó el patrón de sincronización "Lectores-Escritores" utilizando la técnica de **Lightswitch** (Interruptor de luz) para proteger el acceso a una sección crítica simulada.

**Observaciones de la Depuración (UART Log):**
1. **Exclusión Mutua Categórica:** Se comprobó exitosamente que cuando el Escritor (Task B) ingresa a la sección crítica (`Acceso exclusivo`), el Lector (Task A) queda bloqueado esperando que la habitación se libere. Nunca se observó superposición entre lectura y escritura.
2. **Lógica Lightswitch:** El primer lector en llegar bloquea el recurso para los escritores (`xSemaphoreTake(h_room_empty)`), y el último en salir lo libera.
3. **Análisis de Rendimiento y Riesgo de Inanición (Starvation):** Debido a que los lectores tienen un retardo mucho menor (250ms) que los escritores (2500ms), la CPU es monopolizada casi enteramente por las tareas de lectura. En este escenario con un solo lector, el escritor logra "colarse" durante la breve pausa del lector. Sin embargo, se deduce que en un escenario de alta concurrencia (múltiples lectores solapados), el contador de lectores activos nunca llegaría a cero, provocando la **inanición total** del escritor.

**Conclusión:**
La implementación base del *Lightswitch* prioriza fuertemente a los lectores, garantizando la integridad de los datos pero sacrificando la predecibilidad del tiempo de respuesta para las actualizaciones de datos (escrituras).