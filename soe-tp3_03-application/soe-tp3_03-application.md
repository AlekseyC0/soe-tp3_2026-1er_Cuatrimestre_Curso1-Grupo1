

# Análisis de Código: TP3 - Actividad 03 (Entorno de Pruebas Concurrente)

## 1. Propósito Arquitectónico del Sistema

En esta actividad se pasa de un modelo reactivo basado puramente en estímulos físicos externos (como pulsadores) a un entorno de **Pruebas Automatizadas y Determinísticas** dentro del RTOS.

El sistema está compuesto por cinco tareas concurrentes cooperativas: cuatro de ellas representan los puntos de control físicos de un establecimiento (`task_entry_a`, `task_exit_a`, `task_entry_b`, `task_exit_b`) y la quinta es una tarea de control de calidad de software/inyección de fallas denominada `task_test`. Esta última actúa como un **inyector de estímulos sintéticos** para verificar la robustez de los mecanismos de sincronización (Mutex/Semáforos) sin necesidad de hardware real.

---

## 2. Análisis Detallado de los Archivos Fuentes

### A. Archivo `task_test.c` (Inyector de Eventos Clave)

Este archivo es el núcleo metodológico de la Actividad 03. Permite simular secuencias de eventos complejas a velocidades controladas.

```c
task_test_priority = uxTaskPriorityGet(NULL) + 2ul;
vTaskPrioritySet(NULL, task_test_priority);

```

* **Análisis:** Al iniciar, la tarea de prueba consulta su propia prioridad con `uxTaskPriorityGet(NULL)` y se autoincrementa en dos niveles usando `vTaskPrioritySet()`.
* **Justificación:** Al convertirse temporalmente en la tarea de **mayor prioridad del sistema**, el planificador de FreeRTOS le garantiza el monopolio absoluto de la CPU al momento de inyectar un evento. Esto evita que los retrasos de las otras tareas corrompan el determinismo de la secuencia de prueba.

```c
for (index = 0; index < (sizeof(e_task_test_array)/sizeof(e_task_test_t)); index++) {
    g_task_test_cnt++;
    switch (e_task_test_array[index]) {
        case Entry_A:
            // Despertar o notificar a task_entry_a
            break;
        case Exit_A:
            // Despertar o notificar a task_exit_a
            break;
        ...
    }
}

```

* **Análisis:** Recorre un arreglo estático indexado (`e_task_test_array`) que almacena la traza o el "script" de la simulación. El operador `sizeof` calcula automáticamente el límite del bucle dinámicamente según la cantidad de eventos definidos.
* **Justificación:** Cada elemento del arreglo representa una acción del mundo real (ej. *Llega un auto/persona al Ingreso A*). El bloque `switch-case` traduce este identificador lógico en una señal de sincronización concreta de FreeRTOS (como una notificación directa entre tareas `xTaskNotifyGive()` o la liberación de un semáforo binario) hacia la tarea correspondiente.

---

### B. Archivos de Control de Flujo (`task_entry_a.c`, `task_exit_a.c`, etc.)

Estos cuatro archivos poseen estructuras simétricas optimizadas para el bloqueo pasivo.

```c
#define TASK_ENTRY_A_DEL_MAX   (pdMS_TO_TICKS(2500ul))

```

* **Análisis:** Define el tiempo de guarda de los actuadores físicos (el tiempo que permanece abierta la barrera o puerta) transformando milisegundos nativos a *Ticks* del procesador de forma portable.

```c
for (;;) {
    // 1. Bloqueo pasivo esperando el estímulo de task_test
    // [Mecanismo de sincronización como semáforo o notificación]
    
    g_task_entry_a_cnt++;
    LOGGER_INFO("Task Entry A detectó evento inyectado.");
    
    // 2. Simulación de la apertura del actuador
    vTaskDelay(TASK_ENTRY_A_DEL_MAX);
}

```

* **Análisis:** Las tareas no corren en bucles de espera activa (*busy waiting*). Al inicio del bucle infinito, se suspenden inmediatamente en una primitiva de sincronización.
* **Justificación:** Al quedar en estado **Blocked**, FreeRTOS remueve estas tareas de la lista de ejecución, permitiendo que la CPU quede libre para que el inyector de eventos (`task_test`) compute el siguiente paso del script de prueba.

---

### C. Archivo `app.c` (Configuración e Instanciación Total)

Este archivo centraliza la asignación de recursos en memoria (Heap) del microcontrolador.

```c
TaskHandle_t h_task_entry_a;
TaskHandle_t h_task_exit_a;
...
TaskHandle_t h_task_test;

```

* **Análisis:** Define los punteros de control estructural (*Handles*) de cada hilo. Son indispensables para que `task_test` pueda enviar señales de control directas a cada tarea de forma unívoca.

```c
xTaskCreate(task_test, "Task Test", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1ul), &h_task_test);

```

* **Análisis:** Instancia las 5 tareas con la misma prioridad base inicial (`Priority 1`).
* **Justificación:** Esto demuestra el control dinámico de prioridades: todas arrancan compartiendo la CPU equitativamente, pero el entorno de prueba toma el control de los tiempos modificando su prioridad sobre la marcha dentro de su propia función.

---

## 3. Comportamiento en el Debugger (Qué observar al ejecutarlo)

Cuando lances la sesión de depuración en OpenOCD y monitorices la terminal, verás una simulación automatizada perfecta. Dado que no dependes de la velocidad de un dedo humano presionando un botón, el orden de los logs será estrictamente determinístico y repetible en cada reinicio.

Para comprender cómo interactúa este inyector de eventos automático con las colas y las prioridades del sistema operativo sin congelar el procesador, he diseñado la siguiente herramienta de simulación interactiva. Te permitirá visualizar el flujo exacto de inyección de eventos sintéticos y cómo cambian los estados de las tareas.