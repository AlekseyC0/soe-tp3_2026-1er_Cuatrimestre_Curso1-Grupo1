


# Paso 06: Implementación del Problema Lectores-Escritores

## 1. Análisis Conceptual del Problema

El problema clásico de **Lectores-Escritores (Readers-Writers)** se presenta cuando múltiples tareas concurrentes necesitan acceder a una misma estructura de datos (por ejemplo, una base de datos de usuarios o un registro histórico de accesos).

Las reglas de acceso (restricciones de sincronización) son asimétricas:

1. **Múltiples Lectores Permitidos:** Si la estructura de datos no se está modificando, cualquier cantidad de tareas "Lectoras" pueden entrar y leer simultáneamente. La lectura concurrente es segura.
2. **Escritor Exclusivo:** Si una tarea "Escritora" necesita modificar los datos, debe hacerlo sola. No puede haber otros escritores escribiendo al mismo tiempo, y **no puede haber lectores leyendo** mientras se escribe, para evitar que lean datos corruptos o a medio actualizar.

### El Patrón "Lightswitch" (Interruptor de Luz)

El texto describe un patrón de diseño muy útil llamado *Lightswitch*. La analogía es perfecta:

* La primera persona que entra a una habitación oscura enciende la luz.
* Las siguientes personas que entran simplemente disfrutan de la luz (no tienen que tocar el interruptor).
* La última persona en salir apaga la luz.

Aplicado a nuestro problema:

* El **primer Lector** que entra "bloquea" la habitación para los Escritores.
* El **último Lector** que sale "desbloquea" la habitación, permitiendo que entren los Escritores.

## 2. Modificaciones en el Código (Implementación FreeRTOS)

Para implementar esto en tu proyecto (usando `task_a` como Lector y `task_b` como Escritor, o viceversa), necesitaremos modificar los archivos `app.c`, `task_a.c` y `task_b.c`.

### A. Modificaciones en `app.c` (Inicialización)

Debemos crear los recursos compartidos:

* `mutex_readers`: Un mutex para proteger el contador de lectores.
* `room_empty`: Un semáforo binario (actúa como mutex) que representa si la habitación (la sección crítica) está vacía o no.
* `g_readers_count`: Una variable global para llevar la cuenta de cuántos lectores hay.

```c
// En app.c (Sección de declaraciones globales)
SemaphoreHandle_t h_mutex_readers;
SemaphoreHandle_t h_room_empty;
uint32_t g_readers_count = 0;

void app_init(void) {
    // ... código previo ...

    // Crear el Mutex para proteger el contador de lectores
    h_mutex_readers = xSemaphoreCreateMutex();
    configASSERT(h_mutex_readers != NULL);

    // Crear el Semáforo/Mutex para indicar si la habitación está vacía
    h_room_empty = xSemaphoreCreateMutex();
    configASSERT(h_room_empty != NULL);

    // ... creación de tareas task_a y task_b ...
}

```

### B. Modificaciones en `task_a.c` (El Lector)

Esta tarea implementará la lógica del *Lightswitch*.

```c
// En task_a.c
extern SemaphoreHandle_t h_mutex_readers;
extern SemaphoreHandle_t h_room_empty;
extern uint32_t g_readers_count;

void task_a(void *parameters) {
    for (;;) {
        LOGGER_INFO("Lector A: Intentando leer...");

        // --- Patrón Lightswitch: Entrar a la habitación ---
        xSemaphoreTake(h_mutex_readers, portMAX_DELAY); // Proteger el contador
        g_readers_count++;
        if (g_readers_count == 1) {
            // Soy el primer lector. Bloqueo la habitación para los escritores.
            xSemaphoreTake(h_room_empty, portMAX_DELAY); 
        }
        xSemaphoreGive(h_mutex_readers); // Suelto el contador

        // --- INICIO SECCIÓN CRÍTICA DE LECTURA ---
        LOGGER_INFO("Lector A: Leyendo datos (Lectores activos: %lu)...", g_readers_count);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Simula tiempo de lectura
        // --- FIN SECCIÓN CRÍTICA DE LECTURA ---

        // --- Patrón Lightswitch: Salir de la habitación ---
        xSemaphoreTake(h_mutex_readers, portMAX_DELAY); // Proteger el contador
        g_readers_count--;
        if (g_readers_count == 0) {
            // Soy el último lector. Desbloqueo la habitación para los escritores.
            xSemaphoreGive(h_room_empty); 
        }
        xSemaphoreGive(h_mutex_readers); // Suelto el contador

        LOGGER_INFO("Lector A: Lectura terminada.");
        vTaskDelay(TASK_A_DEL_MAX); // Retardo antes de la próxima lectura
    }
}

```

### C. Modificaciones en `task_b.c` (El Escritor)

El escritor tiene un trabajo mucho más sencillo: solo debe esperar a que la habitación esté completamente vacía (sin lectores ni otros escritores).

```c
// En task_b.c
extern SemaphoreHandle_t h_room_empty;

void task_b(void *parameters) {
    for (;;) {
        LOGGER_INFO("Escritor B: Intentando escribir...");

        // Intentar bloquear la habitación entera
        xSemaphoreTake(h_room_empty, portMAX_DELAY); 

        // --- INICIO SECCIÓN CRÍTICA DE ESCRITURA ---
        LOGGER_INFO("Escritor B: ¡Escribiendo datos! Acceso exclusivo.");
        vTaskDelay(pdMS_TO_TICKS(1500)); // Simula tiempo de escritura
        // --- FIN SECCIÓN CRÍTICA DE ESCRITURA ---

        // Liberar la habitación
        xSemaphoreGive(h_room_empty); 

        LOGGER_INFO("Escritor B: Escritura terminada.");
        vTaskDelay(TASK_B_DEL_MAX); // Retardo antes de la próxima escritura
    }
}

```

## 3. Depuración y Observación (El Problema del "Starvation")

Cuando compiles y depures esto, debes asentar en tu archivo `.md` lo siguiente:

**Comportamiento Esperado:**

* Si solo hay lectores (ej. clonas `task_a` varias veces), verás que todos entran a la sección crítica a la vez. El log mostrará "Leyendo datos" simultáneamente desde varias tareas.
* Cuando `task_b` (Escritor) intenta acceder, se quedará bloqueado en `xSemaphoreTake(h_room_empty...)` si hay algún lector adentro.
* Cuando el **último** lector sale, el escritor entra y tiene acceso exclusivo.

**Observación Crítica (El defecto de esta solución base):**
Como menciona la sección *4.2.3 Starvation* del texto, si la frecuencia de llegada de los lectores es muy alta, siempre habrá al menos un lector en la habitación (`g_readers_count > 0`). Si esto pasa, la variable `h_room_empty` **nunca se liberará**.
Por lo tanto, el Escritor (`task_b`) se quedará esperando eternamente en un estado conocido como **"Inanición" (Starvation)**. Nunca logrará escribir.

> *Nota para tu informe:* Esta solución prioriza a los lectores y es susceptible a la inanición de los escritores. Para evitarlo (como pide el *Puzzle* del texto), se necesitaría añadir un "torniquete" (Turnstile) que bloquee el ingreso de *nuevos* lectores tan pronto como un escritor anuncie que quiere entrar.