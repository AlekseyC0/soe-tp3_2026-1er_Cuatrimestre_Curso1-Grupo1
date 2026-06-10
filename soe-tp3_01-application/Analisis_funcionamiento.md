

# Resumen de Funcionamiento: Sincronización de Tareas (Paso 6)

## 1. El Problema Conceptual (Condición de Carrera)

En un sistema embebido concurrente, múltiples tareas se ejecutan aparentemente al mismo tiempo. En nuestro proyecto, tenemos tareas (`task_a` y `task_b`) que representan puertas de acceso. Ambas tareas necesitan utilizar la terminal serie (UART) mediante la función `LOGGER_INFO()` para reportar sus estados, e incrementar contadores globales.

Como el planificador (Scheduler) de FreeRTOS puede interrumpir una tarea en cualquier momento para darle la CPU a otra de igual o mayor prioridad, existe el riesgo de que **Puerta A** empiece a imprimir un mensaje y, a la mitad, el RTOS le dé el control a **Puerta B**, la cual también intentará imprimir. El resultado es la corrupción de datos y mensajes mezclados en la consola (Condición de Carrera).

## 2. Análisis y Justificación por Archivos

Para resolver este problema, el código se estructuró y modificó de la siguiente manera:

* **`app_it.c` (El enfoque Bare-Metal - Primitivo):**
Este archivo demuestra cómo se protege un recurso compartido sin un sistema operativo, usando lenguaje ensamblador (`CPSID i` y `CPSIE i`) para apagar y prender las interrupciones globales.
* *Justificación de descarte:* En FreeRTOS, apagar interrupciones globales para operaciones lentas (como imprimir por UART o esperar que pase una persona) es una mala práctica, ya que detiene el `SysTick` del sistema operativo, "cegando" al microcontrolador.


* **`app.c` (Orquestador y Creación de Recursos):**
Es el corazón de la configuración.
* *Funcionamiento:* Antes de crear las tareas, se instancia y crea un `SemaphoreHandle_t h_mutex` mediante `xSemaphoreCreateMutex()`.
* *Justificación:* El Mutex ("llave" de exclusión mutua) debe existir en la memoria antes de que cualquier tarea intente usarlo. Luego, las tareas A y B se crean con la **misma prioridad** (`tskIDLE_PRIORITY + 1`), lo que significa que competirán en igualdad de condiciones por el uso de la CPU.


* **`task_a.c` y `task_b.c` (Las Máquinas de Estado Controladas):**
Aquí reside la lógica concurrente.
* *Funcionamiento:* Se implementó un bloque condicional `if (xSemaphoreTake(h_mutex, portMAX_DELAY) == pdTRUE)`.
* *Justificación:* Cuando `task_a` llega a esta línea, pide el Mutex. Si está libre, entra a su **Sección Crítica**. Dentro de ella, imprime el estado "ABRIENDO", espera un tiempo con `vTaskDelay` (simulando que la puerta está abierta para que pase la persona) y luego imprime "CERRANDO". Al terminar, **debe** ejecutar `xSemaphoreGive(h_mutex)`.
* Si mientras `task_a` está en su sección crítica, el RTOS le da CPU a `task_b`, esta última intentará hacer el `xSemaphoreTake`. Como la "llave" la tiene A, **`task_b` pasará automáticamente al estado `Blocked**` (consumiendo 0% de CPU) hasta que A libere el Mutex.


* **`freertos.c` (Monitoreo del Sistema):**
* *Funcionamiento:* Implementa las funciones *Hook* (ganchos) de FreeRTOS (`vApplicationIdleHook`, `vApplicationTickHook`, `vApplicationStackOverflowHook`).
* *Justificación:* Sirven para llevar métricas del sistema. Si por algún motivo nuestras tareas se bloquean mutuamente (Deadlock) o consumen demasiada memoria RAM, el Stack Overflow Hook atrapará el error (`configASSERT(0)`) y detendrá el procesador de forma segura para evitar un colapso catastrófico.



---

## 3. Guía de Debugging: ¿Qué debería pasar y qué observar?

Al conectar tu placa (ej. Nucleo-F103RB) y lanzar la sesión de Debug (con STM32CubeIDE o similar), esto es lo que debes validar para comprobar que el Paso 6 está correctamente implementado:

### A. Observación en la Terminal Serie (UART)

1. **Sin Mutex (Lo incorrecto):** Verías algo como `Puerta A: ABRIE Puerta B: ABRIENDO... NDO...`. Los textos se superponen.
2. **Con Mutex (Lo correcto - Tu objetivo):** Verás bloques de texto atómicos.
```text
Puerta A: Solicitando acceso...
Puerta A: ABRIENDO...
Puerta A: CERRANDO.
==> Task A - Wait: 2500mS
Puerta B: Solicitando acceso...
Puerta B: ABRIENDO...

```


*Ningún mensaje de la Puerta B interrumpirá el ciclo de la Puerta A hasta que esta diga "Wait" (que es cuando hace el `xSemaphoreGive`).*

### B. Observación en el Depurador (Breakpoints y FreeRTOS View)

1. **Vista de Tareas (Task List):** Pon un breakpoint (punto de interrupción) justo en la línea `vTaskDelay(pdMS_TO_TICKS(1000));` dentro del `if` del Mutex en `task_a.c`.
* Cuando el código se detenga allí, abre la vista de **FreeRTOS Task List** de tu IDE.
* Deberías ver que `Task A` está en estado **Running** (o el estado actual del debugger).
* Deberías ver que `Task B` está en estado **Blocked**, y el motivo del bloqueo será que está esperando el objeto `h_mutex`.


2. **Paso a Paso (Step Over):**
* Si avanzas línea por línea pasando el `xSemaphoreGive(h_mutex)` en `task_a`, y observas la vista de tareas nuevamente, verás que mágicamente `Task B` pasa del estado **Blocked** al estado **Ready** (Lista para ejecutarse), porque el RTOS ya le avisó que el recurso se liberó.



### Conclusión del Debugging

Si logras observar este comportamiento (mensajes limpios en consola y tareas transicionando ordenadamente entre estado Bloqueado y Listo), habrás demostrado exitosamente el dominio sobre la concurrencia, la protección de memoria y la prevención de condiciones de carrera utilizando FreeRTOS.