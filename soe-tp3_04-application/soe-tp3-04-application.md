# TP3 - Actividad 04: Esclusa de Seguridad (Security Airlock)

## Descripción General
La **Actividad 4** se centra en la resolución de otro problema clásico de concurrencia: **Security Airlock** (Esclusa de Seguridad). El objetivo es diseñar un sistema de control de acceso para una habitación tipo esclusa con cuatro puertas (A, B, C y D), que garantice que **sólo pueda haber una puerta abierta a la vez** y que el paso sea para una sola persona por vez.

## Tareas Implementadas en FreeRTOS
1. **`task_gate_a`, `task_gate_b`, `task_gate_c`, `task_gate_d`**: Cada tarea representa el controlador de una de las cuatro puertas. Controlan el ciclo de apertura (`Open`) y cierre (`Close`).
2. **`task_test`**: Simula los eventos físicos como la pulsación de los botones de apertura y los sensores de cierre de las puertas enviando estímulos (`OPEN_REQUEST_X`, `DOOR_CLOSED_X`).

## Mecanismos de Sincronización

* **Mutex Global (Exclusión Mutua de la Esclusa):**
    * El núcleo del problema es que las puertas no pueden abrirse simultáneamente. Para resolverlo, se utiliza un único **Mutex global** (`mutex_airlock`). Cuando una puerta recibe una petición de apertura, primero debe adquirir este Mutex. Si otra puerta ya está abierta (y por tanto tiene el Mutex), la nueva petición quedará bloqueada en estado `Blocked` hasta que la puerta activa se cierre y libere el recurso.
* **Semáforos Binarios (Peticiones y Cierres):**
    * Cada puerta cuenta con dos semáforos binarios propios: uno para la solicitud de apertura (`sem_open_req`) y otro para la confirmación de cierre (`sem_door_closed`).
    * La `task_test` libera el `sem_open_req` para iniciar el ciclo de una puerta. Una vez abierta, la tarea de la puerta espera (`Take`) por el `sem_door_closed`. Cuando `task_test` simula que la persona cruzó y la puerta se cerró físicamente, emite el cierre, lo que permite a la tarea liberar por fin el Mutex global.

## Flujo Lógico (Ejemplo Puerta A)
1. `task_gate_a` espera petición -> `xSemaphoreTake(sem_open_req_a)`
2. Esperar disponibilidad de esclusa -> `xSemaphoreTake(mutex_airlock)`
3. Abrir Puerta A (`LOGGER_INFO("Puerta A: Abierta")`)
4. Persona cruza, esperar cierre físico -> `xSemaphoreTake(sem_door_closed_a)`
5. Confirmar cierre -> (`LOGGER_INFO("Puerta A: Cerrada")`)
6. Liberar esclusa para las demás -> `xSemaphoreGive(mutex_airlock)`

