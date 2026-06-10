Aquí tienes una guía de análisis teórico-práctico completa y justificada para estructurar tu archivo markdown (`soe-tp3_03-application.md`). Este documento detalla cómo rediseñar el entorno de pruebas de la **Actividad 03** utilizando exclusivamente **Semáforos Binarios** (para señalización de eventos) y **Mutexes** (para protección de recursos compartidos).

---

# Análisis Teórico y Práctico: Sincronización Avanzada en la Actividad 03

## 1. Fundamentos Teóricos del Rediseño

En un sistema operativo de tiempo real como FreeRTOS, el intercambio de estímulos y la protección de datos deben responder a patrones de diseño específicos para garantizar el determinismo:

* **Semáforos Binarios para Señalización (Unblocking Pattern):** Un semáforo binario creado para sincronización arranca en estado "vacío" (bloqueado). La tarea receptora (`task_entry_a`) intenta tomarlo (`xSemaphoreTake`) y queda inmediatamente en estado *Blocked*. Cuando el inyector (`task_test`) genera el evento, "entrega" el semáforo (`xSemaphoreGive`), moviendo instantáneamente a la tarea objetivo al estado *Ready*.
* **Mutex para Exclusión Mutua (Resource Protection):** Las cuatro compuertas modifican una sección crítica de datos común: el aforo o población total dentro del establecimiento (`g_establishment_capacity`). Si dos compuertas se activan simultáneamente e incrementan/decrementan esta variable global sin protección, se produce una **condición de carrera** a nivel de instrucciones de ensamblador (*Read-Modify-Write*). El Mutex garantiza atomicidad y cuenta con **herencia de prioridad** para mitigar la inversión de prioridades.

---

## 2. Implementación Paso a Paso a Nivel Código

### Paso 1: Declaración y Creación de Primitivas (`app.c`)

Primero, debemos instanciar y crear los semáforos binarios individuales de activación y el mutex de aforo global.

```c
/* --- En app.c (Declaraciones globales) --- */
SemaphoreHandle_t h_sem_entry_a;
SemaphoreHandle_t h_sem_exit_a;
SemaphoreHandle_t h_sem_entry_b;
SemaphoreHandle_t h_sem_exit_b;

SemaphoreHandle_t h_mutex_capacity;
uint32_t g_establishment_capacity = 0;

void app_init(void)
{
    /* 1. Crear Mutex de exclusión mutua para el recurso compartido (inicia libre) */
    h_mutex_capacity = xSemaphoreCreateMutex();
    configASSERT(h_mutex_capacity != NULL);

    /* 2. Crear Semáforos Binarios para señalización (inician bloqueados/vacíos) */
    h_sem_entry_a = xSemaphoreCreateBinary();
    h_sem_exit_a  = xSemaphoreCreateBinary();
    h_sem_entry_b = xSemaphoreCreateBinary();
    h_sem_exit_b  = xSemaphoreCreateBinary();
    
    configASSERT(h_sem_entry_a != NULL);
    configASSERT(h_sem_exit_a  != NULL);
    configASSERT(h_sem_entry_b != NULL);
    configASSERT(h_sem_exit_b  != NULL);

    /* --- Creación de las 5 tareas (task_test, compuertas...) --- */
}

```

### Paso 2: Lógica de Inyección Sintética (`task_test.c`)

El bucle del inyector traduce el script estático del arreglo en entregas de tokens de semáforos.

```c
/* --- En task_test.c (Bucle principal) --- */
for (index = 0; index < (sizeof(e_task_test_array)/sizeof(e_task_test_t)); index++)
{
    g_task_test_cnt++;
    
    switch (e_task_test_array[index]) {
        case Entry_A:
            LOGGER_INFO("Task Test: Inyectando pulso a Entry A.");
            xSemaphoreGive(h_sem_entry_a); // Despierta de inmediato a la tarea de entrada A
            break;
            
        case Exit_A:
            LOGGER_INFO("Task Test: Inyectando pulso a Exit A.");
            xSemaphoreGive(h_sem_exit_a);
            break;
            
        /* ... Repetir simétricamente para Compuerta B ... */
        default:
            break;
    }
    
    /* Pequeño delay para permitir que la tarea desbloqueada tome la CPU */
    vTaskDelay(pdMS_TO_TICKS(50)); 
}

```

### Paso 3: Consumo y Modificación de la Sección Crítica (`task_entry_a.c`)

Cada tarea de compuerta espera pasivamente su semáforo de activación y utiliza el Mutex para alterar el aforo de forma segura.

```c
/* --- En task_entry_a.c (Estructura de la tarea) --- */
extern SemaphoreHandle_t h_sem_entry_a;
extern SemaphoreHandle_t h_mutex_capacity;
extern uint32_t g_establishment_capacity;

void task_entry_a(void *parameters)
{
    g_task_entry_a_cnt = G_TASK_ENTRY_A_INITIAL_VALUE;
    
    for (;;) {
        /* 1. Bloqueo absoluto pasivo: Espera que task_test entregue el semáforo */
        if (xSemaphoreTake(h_sem_entry_a, portMAX_DELAY) == pdTRUE) {
            
            g_task_entry_a_cnt++;
            LOGGER_INFO("Compuerta Entry A: ¡Barrera abierta por evento sintético!");

            /* 2. Proteger la sección crítica de datos globales usando el Mutex */
            if (xSemaphoreTake(h_mutex_capacity, portMAX_DELAY) == pdTRUE) {
                /* --- INICIO SECCIÓN CRÍTICA --- */
                g_establishment_capacity++;
                LOGGER_INFO("Aforo actualizado de forma atómica. Población: %lu", g_establishment_capacity);
                /* --- FIN SECCIÓN CRÍTICA --- */
                xSemaphoreGive(h_mutex_capacity);
            }

            /* Simulación del tiempo físico que tarda el auto/persona en cruzar */
            vTaskDelay(TASK_ENTRY_A_DEL_MAX); 
            LOGGER_INFO("Compuerta Entry A: Barrera cerrada de forma segura.");
        }
    }
}

```

*(Nota: Las tareas `task_exit_a.c`, `task_entry_b.c` y `task_exit_b.c` siguen exactamente esta misma estructura arquitectónica, modificando la variable `g_establishment_capacity--` dentro de sus respectivos bloques protegidos por el Mutex).*

---

## 3. Comportamiento Dinámico Esperado en la Depuración

Al correr el firmware bajo el depurador OpenOCD, el comportamiento del planificador de FreeRTOS demostrará un flujo de ejecución de la CPU perfectamente sincronizado:

1. **Estado de Reposo Inicial:** Al iniciar el *Kernel*, las 4 compuertas se ejecutan y quedan congeladas en estado **Blocked** al ejecutar la línea `xSemaphoreTake(h_sem_..., portMAX_DELAY)`. No consumen ciclos de reloj del procesador.
2. **Inyección Directa:** La tarea de pruebas (`task_test`), que incrementó dinámicamente su prioridad, toma el control del bus y emite un `xSemaphoreGive(h_sem_entry_a)`. Al instante, `task_entry_a` pasa de **Blocked** a **Ready**.
3. **Conmutación por Delay:** Cuando `task_test` se duerme por 50ms mediante `vTaskDelay`, el *Scheduler* busca la tarea de mayor prioridad lista y le entrega el microcontrolador a `task_entry_a`, que pasa a **Running**, procesa su lógica, toma el Mutex de aforo de manera exclusiva y actualiza la variable global.
4. **Prevención de Colisiones de Datos:** Si el script de prueba intentara disparar dos eventos simultáneos (ej: Entrada A y Salida B al mismo tiempo), la tarea que llegue una fracción de microsegundo tarde al bloque del Mutex quedará en espera ordenada en la lista de bloqueo del Mutex, evitando cualquier tipo de corrupción de memoria de la variable global de aforo.

Para asimilar conceptualmente cómo transicionan las tareas y cómo interactúan los semáforos contadores, binarios y mutexes en FreeRTOS sin romper el determinismo, utiliza este simulador de kernel interactivo para ejecutar el script paso a paso.