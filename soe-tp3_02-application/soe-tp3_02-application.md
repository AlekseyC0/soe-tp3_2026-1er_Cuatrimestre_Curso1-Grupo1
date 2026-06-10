¡Perfecto! Avanzamos al TP3 - Actividad 02, donde el objetivo es pasar de una simple concurrencia a la **comunicación de tareas** utilizando FreeRTOS. Esto es clave, ya que en un sistema real las tareas rara vez están aisladas; normalmente necesitan pasarse datos entre sí de forma segura.

A continuación te presento un análisis detallado, archivo por archivo y línea por línea, estructurado en formato Markdown (listo para tu archivo `.md`).

---

# Análisis de Código: TP3 - Actividad 02 (Comunicación entre Tareas)

## 1. Análisis de `app_it.c`

```c
#include "main.h"
#include "logger.h"
#include "dwt.h"
#include "board.h"

void app_it_init(void)
{
	/* Init to be done */

	/* Protect shared resource */
	__asm("CPSID i");	/* disable interrupts */

	__asm("CPSIE i");	/* enable interrupts */
}

```

* **Propósito:** Este archivo sigue mostrando un enfoque primitivo (bare-metal) para inicializar interrupciones a nivel de aplicación.
* **Líneas clave:** El uso de ensamblador (`__asm("CPSID i")` y `__asm("CPSIE i")`) para deshabilitar y habilitar interrupciones globalmente.
* **Contexto en FreeRTOS:** Aunque este archivo está presente, en un entorno con RTOS tratamos de evitar apagar las interrupciones globales prolongadamente. En cambio, usaremos colas (Queues) para comunicar tareas y proteger los datos en tránsito.

## 2. Análisis de `freertos.c`

```c
#include "main.h"
#include "cmsis_os.h"
// ... otros includes ...

void vApplicationIdleHook(void)
{
	g_task_idle_cnt++;
}

void vApplicationTickHook(void)
{
	g_app_tick_cnt++;
}

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
    taskENTER_CRITICAL();
    configASSERT( 0 );   /* hang the execution for debugging purposes */
    taskEXIT_CRITICAL();
    g_app_stack_overflow_cnt++;
}

```

* **Propósito:** Contiene los *Hooks* (ganchos o callbacks) del sistema operativo.
* **`vApplicationIdleHook`:** Se ejecuta cuando ninguna otra tarea tiene trabajo que hacer. Aquí simplemente incrementa un contador (`g_task_idle_cnt`), útil para perfilar el uso de la CPU.
* **`vApplicationTickHook`:** Se ejecuta con cada interrupción del *SysTick* del RTOS (típicamente cada 1 ms). Actualiza el contador `g_app_tick_cnt`.
* **`vApplicationStackOverflowHook`:** Es una función crítica de seguridad. Si el RTOS detecta que una tarea (como `task_a` o `task_b`) ha consumido toda la memoria RAM asignada a su pila (*stack*), el sistema entra en una sección crítica y detiene la ejecución con `configASSERT(0)`. Esto facilita la depuración de problemas severos de memoria.

## 3. Análisis de `app.c` (El Orquestador)

Este archivo configura la aplicación y levanta los recursos del RTOS antes de que el *scheduler* empiece a correr.

```c
// ... (Includes y definiciones de contadores) ...

/* Declare a variable of type QueueHandle_t. This is used to reference queues*/
// ... (Nota: Aquí en el futuro deberíamos declarar la Cola) ...

TaskHandle_t h_task_a;
TaskHandle_t h_task_b;

void app_init(void)
{
	// 1. Inicialización de contadores
	g_app_cnt = G_APP_CNT_INI;
    // ...

    // 2. Creación de la Tarea A (El Productor)
    ret = xTaskCreate(task_a, "Task A", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1ul), &h_task_a);
    configASSERT(pdPASS == ret);

    // 3. Creación de la Tarea B (El Consumidor)
    ret = xTaskCreate(task_b, "Task B", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1ul), &h_task_b);
    configASSERT(pdPASS == ret);

    // ... (Init de interrupciones y contadores de ciclo) ...
}

```

* **Propósito:** Inicializa las variables globales y crea los hilos de ejecución (`task_a` y `task_b`).
* **Observación sobre Queues:** Hay un comentario explícito: `/* Declare a variable of type QueueHandle_t... */`. Esto nos da la pista de que en esta actividad deberemos instanciar una cola aquí usando `xQueueCreate()`, para que las tareas puedan comunicarse de forma segura.

## 4. Análisis de `task_a.c` (Productor)

```c
#include "app.h" // ...
#define TASK_A_DEL_MAX		(pdMS_TO_TICKS(250ul)) // Retardo de 250 ms

const char *p_task_a_wait_250mS	= "   ==> Task    A - Wait:   250mS";
uint32_t g_task_a_cnt;

void task_a(void *parameters)
{
	g_task_a_cnt = G_TASK_A_CNT_INI;
	LOGGER_INFO("  %s is running...", pcTaskGetName(NULL));

	for (;;)
	{
		g_task_a_cnt++;
		LOGGER_INFO(p_task_a_wait_250mS);
		vTaskDelay(TASK_A_DEL_MAX);
	}
}

```

* **Propósito Actual:** Es una tarea muy rápida. Imprime un mensaje por la consola y luego se bloquea (duerme) durante 250 milisegundos (`vTaskDelay`).
* **Contexto de la Actividad 2:** En un esquema de comunicación, esta tarea probablemente actuará como el **Productor**. En lugar de solo dormir, generará un dato (ej. leer un estado simulado) y lo enviará a una cola (Queue) utilizando una API como `xQueueSend()`.

## 5. Análisis de `task_b.c` (Consumidor)

```c
#include "app.h" // ...
#define TASK_B_DEL_MAX		(pdMS_TO_TICKS(2500ul)) // Retardo de 2500 ms (2.5 segundos)

const char *p_task_b_wait_2500mS = "   ==> Task    B - Wait:   2500mS";
uint32_t g_task_b_cnt;

void task_b(void *parameters)
{
	g_task_b_cnt = G_TASK_B_CNT_INI;
	LOGGER_INFO("  %s is running...", pcTaskGetName(NULL));

	for (;;)
    {
		g_task_b_cnt++;
		LOGGER_INFO(p_task_b_wait_2500mS);
		vTaskDelay(TASK_B_DEL_MAX);
	}
}

```

* **Propósito Actual:** Esta tarea es 10 veces más lenta que la Tarea A. Se bloquea durante 2500 milisegundos.
* **Contexto de la Actividad 2:** Aquí hay un claro escenario de *cuello de botella* si intentamos que se comuniquen sin colas. Si `task_a` produce datos cada 250ms y `task_b` los consume cada 2500ms, los datos se perderán o el sistema fallará.
* **El Rol de la Cola:** La solución a este desfase de velocidades es implementar una **Cola (Queue)** de FreeRTOS. La cola actuará como un *buffer*. `task_a` dejará sus mensajes allí rápidamente, y `task_b` irá retirándolos (`xQueueReceive()`) a su propio ritmo lento y seguro.

---

### Resumen Conceptual de la Actividad 2

Los archivos proporcionados establecen la base estructural. Sin embargo, para lograr la **Comunicación de Tareas**, el siguiente paso lógico (que te pedirá la guía) será modificar `app.c` para crear un objeto `QueueHandle_t`, modificar `task_a` para que inserte datos en esa cola, y modificar `task_b` para que espere y extraiga esos datos. Todo esto gestionado por el RTOS de forma *thread-safe* (segura para hilos).