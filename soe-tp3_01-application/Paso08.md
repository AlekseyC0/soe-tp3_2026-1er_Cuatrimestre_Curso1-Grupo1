

### 1. El Concepto: Productor-Consumidor (Búfer Finito)

Como leímos en el libro "The Little Book of Semaphores" que compartiste antes, este problema ocurre cuando una tarea (Productor) genera datos y los pone en un búfer, y otra tarea (Consumidor) saca esos datos y los procesa.

Para que funcione correctamente y evitar los bloqueos (deadlocks) que analizamos, necesitamos tres mecanismos de sincronización:

1. **`mutex` (Semáforo Binario / Mutex):** Para exclusión mutua. Asegura que solo una tarea a la vez acceda al búfer.
2. **`items` (Semáforo Contador):** Representa la cantidad de elementos disponibles en el búfer. El Productor lo incrementa (Signal/Give) y el Consumidor lo decrementa (Wait/Take).
3. **`spaces` (Semáforo Contador):** Representa la cantidad de espacios vacíos en el búfer. Empieza con el tamaño máximo del búfer. El Productor lo decrementa al añadir un dato, y el Consumidor lo incrementa al sacarlo.

### 2. Modificando el Código (La Implementación)

Vamos a adaptar tus tareas `task_a` (Productor) y `task_b` (Consumidor). Asumiremos que tenemos un búfer (por ejemplo, un arreglo simple) y una variable `buffer_size` definidos en `app.c`.

**En `app.c` (Inicialización):**

```c
// 1. Declarar los manejadores de los semáforos
SemaphoreHandle_t h_mutex;
SemaphoreHandle_t h_items;
SemaphoreHandle_t h_spaces;

#define BUFFER_SIZE 5 

void app_init(void) {
    // ... inicializaciones anteriores ...

    // 2. Crear los semáforos
    h_mutex = xSemaphoreCreateMutex();
    configASSERT(h_mutex != NULL);

    // Semáforo contador para los items (inicia en 0, max BUFFER_SIZE)
    h_items = xSemaphoreCreateCounting(BUFFER_SIZE, 0); 
    configASSERT(h_items != NULL);

    // Semáforo contador para los espacios libres (inicia en BUFFER_SIZE, max BUFFER_SIZE)
    h_spaces = xSemaphoreCreateCounting(BUFFER_SIZE, BUFFER_SIZE); 
    configASSERT(h_spaces != NULL);

    // ... creación de tareas ...
}

```

**En `task_a.c` (El Productor):**

```c
extern SemaphoreHandle_t h_mutex;
extern SemaphoreHandle_t h_items;
extern SemaphoreHandle_t h_spaces;

void task_a(void *parameters) {
    // ... inicialización ...
    for (;;) {
        // Generar un "evento" o dato (simulado aquí con un delay)
        vTaskDelay(pdMS_TO_TICKS(1000)); // Productor genera datos cada 1 segundo
        LOGGER_INFO("Productor: Dato generado. Intentando añadir al buffer...");

        // 1. Esperar a que haya espacio libre en el búfer
        xSemaphoreTake(h_spaces, portMAX_DELAY); 

        // 2. Tomar el mutex para acceder al búfer de forma exclusiva
        xSemaphoreTake(h_mutex, portMAX_DELAY);

        /* --- INICIO SECCIÓN CRÍTICA --- */
        // Añadir elemento al búfer (simulado)
        LOGGER_INFO("Productor: Dato añadido al buffer.");
        /* --- FIN SECCIÓN CRÍTICA --- */

        // 3. Liberar el mutex
        xSemaphoreGive(h_mutex);

        // 4. Avisar al Consumidor que hay un nuevo item
        xSemaphoreGive(h_items); 
    }
}

```

**En `task_b.c` (El Consumidor):**

```c
extern SemaphoreHandle_t h_mutex;
extern SemaphoreHandle_t h_items;
extern SemaphoreHandle_t h_spaces;

void task_b(void *parameters) {
    // ... inicialización ...
    for (;;) {
        LOGGER_INFO("Consumidor: Esperando datos...");

        // 1. Esperar a que haya al menos un item en el búfer
        xSemaphoreTake(h_items, portMAX_DELAY);

        // 2. Tomar el mutex para acceder al búfer de forma exclusiva
        xSemaphoreTake(h_mutex, portMAX_DELAY);

        /* --- INICIO SECCIÓN CRÍTICA --- */
        // Sacar elemento del búfer (simulado)
        LOGGER_INFO("Consumidor: Dato retirado del buffer.");
        /* --- FIN SECCIÓN CRÍTICA --- */

        // 3. Liberar el mutex
        xSemaphoreGive(h_mutex);

        // 4. Avisar al Productor que hay un nuevo espacio libre
        xSemaphoreGive(h_spaces);

        // Procesar el dato (simulado con un delay)
        vTaskDelay(pdMS_TO_TICKS(1500)); // El consumidor procesa los datos más lento (1.5s)
    }
}

```

### 3. Depuración y Observación (Para asentar en tu archivo `.md`)

Al compilar y depurar este código, debes prestar atención a lo siguiente en la terminal y en la vista de tareas de FreeRTOS:

* **El Orden es Crítico:** Observa que *nunca* se toma el `mutex` antes que el semáforo contador (`h_spaces` o `h_items`). Esto evita el *deadlock* descrito en el libro.
* **Comportamiento Dinámico:**
* Como el **Productor** (1s) es más rápido que el **Consumidor** (1.5s), el búfer se irá llenando poco a poco.
* Eventualmente, el búfer se llenará por completo (0 espacios libres). En este punto, observarás que el Productor intentará hacer `xSemaphoreTake(h_spaces...)` y quedará **Bloqueado (Blocked)** por el RTOS hasta que el Consumidor termine de procesar su dato actual y haga un `xSemaphoreGive(h_spaces)`.
* Esta es la magia del RTOS: el Productor no gasta ciclos de CPU haciendo comprobaciones innecesarias ("busy waiting"); simplemente se duerme hasta que las condiciones sean las correctas.



