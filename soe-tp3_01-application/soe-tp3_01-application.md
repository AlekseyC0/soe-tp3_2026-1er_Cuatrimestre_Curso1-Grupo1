
# Análisis de Código: `app_it.c`

## 1. Propósito General del Archivo
El archivo `app_it.c` (Application Interrupts / Initialization) forma parte de la capa de aplicación del proyecto. Su función principal en este contexto es proveer una rutina de inicialización a nivel de aplicación (`app_it_init`) y demostrar un mecanismo primitivo de protección de recursos compartidos a nivel de hardware (bare-metal) antes o durante la ejecución del sistema operativo.

## 2. Análisis Línea por Línea y Conceptos Clave

### Inclusiones (Includes)
```c
#include "main.h"
#include "logger.h"
#include "dwt.h"
#include "board.h"

```

* **`main.h` y `board.h**`: Contienen las definiciones de hardware generadas por STM32CubeIDE (HAL) y la configuración específica de la placa (pines, puertos, etc.).
* **`logger.h` y `dwt.h**`: Son bibliotecas de utilidad. `logger.h` suele usarse para enviar mensajes por la terminal serie (UART) con fines de depuración, y `dwt.h` (Data Watchpoint and Trace) se utiliza para conteo de ciclos de reloj y retardos precisos.

### La Función `app_it_init`

```c
void app_it_init(void)
{
	/* Init to be done */

	/* Protect shared resource */
	__asm("CPSID i");	/* disable interrupts */

	__asm("CPSIE i");	/* enable interrupts */
}

```

Esta es la sección más importante del archivo en el contexto del **Paso 6 (Sincronización)**.

* **`__asm("...");`**: Esta directiva le indica al compilador de C que inserte instrucciones de lenguaje ensamblador (Assembly) directamente en el código de máquina generado.
* **`CPSID i` (Change Processor State, Interrupt Disable)**: Esta instrucción de la arquitectura ARM Cortex-M deshabilita globalmente las interrupciones enmascarables del microcontrolador.
* **`CPSIE i` (Change Processor State, Interrupt Enable)**: Vuelve a habilitar las interrupciones.

#### ¿Por qué se hace esto? (El concepto de Sección Crítica)

El bloque de código que queda entre deshabilitar y habilitar interrupciones se conoce como **Sección Crítica**.
Si hay un *recurso compartido* (por ejemplo, una variable global, un registro de hardware o un buffer) que puede ser modificado tanto por el programa principal (o una tarea de FreeRTOS) como por una Interrupción (ISR), se corre el riesgo de que la ISR interrumpa la modificación a la mitad, corrompiendo los datos (*Condición de Carrera*). Al usar `CPSID i`, aseguramos que el microcontrolador ejecute el código intermedio de forma *atómica* (sin ser interrumpido).

## 3. Relación con el Paso 6 del TP3 y FreeRTOS

Aunque el enfoque de `CPSID/CPSIE` es efectivo y muy utilizado en sistemas "bare-metal" (sin sistema operativo), **tiene una gran desventaja en sistemas operativos en tiempo real (RTOS)**: deshabilitar todas las interrupciones "ciega" al microcontrolador. Durante ese tiempo, el sistema operativo no puede contar el tiempo (el `SysTick` se bloquea) y el sistema pierde respuesta.

En el **Paso 6** se nos pide sincronizar el acceso a recursos compartidos entre tareas (`task_gate_a`, `task_gate_b`, etc.) de control de acceso. Dado que ahora estamos usando **FreeRTOS**, el enfoque moderno y correcto **no** es apagar las interrupciones globales, sino utilizar mecanismos del RTOS como los **Mutex** (`xSemaphoreTake` / `xSemaphoreGive`).

Un Mutex protege el recurso poniendo a "dormir" (estado *Blocked*) a la tarea que intente acceder al recurso mientras esté ocupado, permitiendo que el microcontrolador y el RTOS sigan ejecutando otras tareas importantes en lugar de apagar el sistema por completo.


---
# Analisis de `freertos.c`

### Lo que nos muestra este `freertos.c`:

1. **Uso de CMSIS-RTOS:** Veo que incluyes `#include "cmsis_os.h"`. Esto significa que probablemente estás usando la capa de abstracción de ARM (CMSIS) por encima de FreeRTOS. Es un dato clave porque nos indica que quizás usemos funciones como `osMutexWait()` y `osMutexRelease()` en lugar de las nativas de FreeRTOS, o bien una mezcla si la cátedra lo permite.
2. **Funciones Hook (Ganchos):** Tienes implementados los Hooks de FreeRTOS (`vApplicationIdleHook`, `vApplicationTickHook`, y `vApplicationStackOverflowHook`). Esto es una excelente práctica. Permite llevar métricas del uso de CPU, contar los ticks y atrapar errores gravísimos (como que una tarea consuma toda su memoria RAM asignada - *Stack Overflow*).

### Lo que falta para completar el Paso 6:

En este fragmento de código **no están definidas las tareas** (`task_gate_a`, `task_gate_b`, etc.). Como veo que en la cabecera tienes `#include "app.h"`, es casi seguro que la lógica principal de tu aplicación y la definición de estas tareas se encuentran en el archivo **`app.c`** (o quizás más abajo en este mismo archivo si el texto se cortó al pegar).

Para poder aplicar el **Mutex** y agregar el control de la puerta (Open/Close) , necesito ver el contenido de esas tareas. Suele verse algo parecido a esto:

```c
void task_gate_a(void *argument) {
  /* Bucle infinito de la tarea */
  for(;;) {
      // Lógica de monitoreo
      // ...
  }
}

```

---


# Análisis y Modificación: `app.c` (Paso 6 - Mutex Init)
```markdown
## 1. Identificando el Problema
En el archivo `app.c` actual se están creando e inicializando dos tareas (`Task A` y `Task B`) que se ejecutarán de forma concurrente. Ambas tareas seguramente intentarán enviar mensajes por la terminal UART usando `LOGGER_INFO()` o modificar contadores globales. Si ambas lo hacen exactamente al mismo tiempo, los mensajes se entrelazarán en la pantalla o los contadores se corromperán.

Para evitar esto, FreeRTOS provee el **Mutex** (Mutual Exclusion), que funciona como un "testigo" o llave única. Quien tiene el Mutex puede usar el recurso, y los demás deben esperar.

## 2. Implementación de las Modificaciones

Vamos a dividir los cambios en dos partes dentro del mismo archivo `app.c`:

### Modificación 1: Declarar el Handle del Mutex
Busca la sección de declaraciones externas (alrededor de la línea 66). Justo debajo del comentario que habla de declarar un `SemaphoreHandle_t`, debes instanciar la variable que guardará la referencia a nuestro Mutex.

**Código a agregar:**
```c
/* Declare a variable of type SemaphoreHandle_t (binary or counting) or mutex.
 * This is used to reference the semaphore that is used to synchronize a thread
 * with other thread or to ensure mutual exclusive access to...*/
SemaphoreHandle_t h_mutex; /* <-- AGREGAR ESTA LÍNEA */

```

### Modificación 2: Crear el Mutex en `app_init`

El Mutex debe existir en la memoria antes de que las tareas intenten usarlo. Por lo tanto, debemos crearlo dentro de la función `app_init(void)`, justo antes de crear las tareas con `xTaskCreate()`.

Busca la sección en `app_init` donde dice `/* Before a queue or semaphore... */` y agrega la llamada a la API de FreeRTOS para crear el Mutex, seguida de un `configASSERT` para asegurar que la memoria se asignó correctamente.

**Código a agregar:**

```c
    /* Before a queue or semaphore (binary or counting) or mutex is used it must 
     * be explicitly created.
	 * Check the queue or semaphore (binary or counting) or mutex was created
     * successfully.
     * Add queue or semaphore (binary or counting) or mutex to registry. */
     
    h_mutex = xSemaphoreCreateMutex();    /* <-- AGREGAR ESTA LÍNEA */
    configASSERT(h_mutex != NULL);        /* <-- AGREGAR ESTA LÍNEA (Verifica creación exitosa) */

	/* Add threads, ... */
    BaseType_t ret;

```
--- 

# Analisis de `task_a.c`

¡Perfecto! Analicemos el archivo `task_a.c` paso a paso para entender su funcionamiento y determinar cómo aplicar el Mutex, tal como lo requiere el Paso 6 del TP3.

### Análisis del Archivo `task_a.c`

Este archivo contiene la definición de una de las tareas del sistema (`task_a`). Su estructura es muy representativa de cómo se escriben las tareas en FreeRTOS.

**1. Inclusiones y Definiciones Iniciales**

```c
#include "main.h"
#include "cmsis_os.h"
#include "logger.h"
#include "dwt.h"
#include "board.h"
#include "app.h"

#define G_TASK_A_CNT_INI	0ul
#define TASK_A_DEL_ZERO		(pdMS_TO_TICKS(0ul))
#define TASK_A_DEL_MAX		(pdMS_TO_TICKS(2500ul))

```

* **Inclusiones:** Importa las librerías necesarias, incluyendo las definiciones de FreeRTOS (`cmsis_os.h`), la herramienta de log (`logger.h`), y las cabeceras de la aplicación (`app.h`).
* **Macros de Retardo:** Define constantes para los retardos usando la macro `pdMS_TO_TICKS`. Esta macro es fundamental porque convierte un tiempo en milisegundos a la cantidad equivalente de *ticks* del sistema operativo (basado en la configuración de FreeRTOS).
* `TASK_A_DEL_MAX` define un retardo de 2500 ms (2.5 segundos).



**2. Variables y Cadenas Constantes**

```c
const char *p_task_a_wait_2500mS = "   ==> Task A       - Wait:   2500mS";
uint32_t g_task_a_cnt;

```

* **`p_task_a_wait_2500mS`:** Una cadena de texto constante que se usará para imprimir en el log.
* **`g_task_a_cnt`:** Una variable global que funciona como un contador para llevar un registro de cuántas veces se ha ejecutado el ciclo principal de la tarea.

**3. La Función de la Tarea (`task_a`)**

```c
void task_a(void *parameters)
{
	/* Declare & Initialize Task Function variables */
	g_task_a_cnt = G_TASK_A_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Update Task Counter */
		g_task_a_cnt++;

    	/* We want this task to execute every 2500 milliseconds. */
		LOGGER_INFO(p_task_a_wait_2500mS);
		vTaskDelay(TASK_A_DEL_MAX);
	}
}

```

Esta es la estructura clásica de una tarea en FreeRTOS:

* **Firma de la función:** Recibe un puntero genérico `void *parameters`. Esto permite pasar argumentos a la tarea al momento de crearla (aunque aquí no se usa, por eso el `NULL` en `xTaskCreate` en `app.c`).
* **Inicialización (Fuera del Bucle):** Antes de entrar al bucle infinito, la tarea inicializa su contador y emite un mensaje de inicio usando `LOGGER_INFO`, mostrando su nombre y el conteo de ticks actual.
* **El Bucle Infinito (`for(;;)`):** Esta es el alma de la tarea. Un sistema embebido (por lo general) no termina su ejecución; se ejecuta continuamente.
* Incrementa su contador interno (`g_task_a_cnt`).
* Imprime el mensaje de que va a esperar usando `LOGGER_INFO`.
* **La Función Clave: `vTaskDelay(TASK_A_DEL_MAX)**`: Esta función no bloquea el microcontrolador (no es un ciclo *while* vacío). Le dice al planificador de FreeRTOS: "Ponme en estado *Blocked* (Bloqueado) durante 2500 milisegundos. Durante este tiempo, dale la CPU a otras tareas que estén listas para ejecutarse". Cuando pasan los 2500 ms, la tarea vuelve al estado *Ready* (Lista).



---

### Aplicando el Mutex y Control de Puertas (Paso 6)

Para cumplir con el Paso 6, debemos adaptar esta tarea (imaginando que ahora es `task_gate_a`) para proteger los recursos compartidos y controlar la puerta.

El recurso compartido más evidente aquí es la función `LOGGER_INFO()`. Si `task_a` (o `task_gate_a`) y `task_b` intentan imprimir al mismo tiempo, los caracteres se mezclarán en la terminal (esto es un clásico problema de concurrencia).

**¿Qué debemos modificar?**

1. **Declarar el Mutex Externo:** Necesitamos acceso al Mutex que creamos en `app.c`.
2. **Tomar el Mutex:** Antes de usar el recurso compartido (el logger o las variables globales compartidas), la tarea debe intentar adquirir (tomar) el Mutex.
3. **Lógica de la Puerta:** Agregar la simulación o el código de control para abrir y cerrar la puerta.
4. **Liberar el Mutex:** Una vez que termina de usar el recurso, debe liberar (dar) el Mutex para que otras tareas puedan usarlo.

Aquí te muestro cómo quedaría la tarea modificada (asumiendo que usamos la API nativa de FreeRTOS, que es más común en este contexto educativo):

```c
// Al inicio del archivo (sección de variables externas)
extern SemaphoreHandle_t h_mutex; // Declarar el handle del mutex creado en app.c

// Dentro del bucle infinito de la tarea:
	for (;;)
	{
        // 1. Intentar tomar el Mutex
        // Esperamos infinitamente (portMAX_DELAY) hasta que el mutex esté disponible
		if (xSemaphoreTake(h_mutex, portMAX_DELAY) == pdTRUE)
        {
            /* --- INICIO DE SECCIÓN CRÍTICA --- */
            
            // 2. Usar los recursos compartidos de forma segura
            g_task_a_cnt++;
            LOGGER_INFO("Puerta A: Solicitando acceso...");
            
            // 3. Simular control de puerta (Open)
            LOGGER_INFO("Puerta A: ABRIENDO...");
            // Aquí iría el código para accionar el actuador de la puerta (ej. HAL_GPIO_WritePin)
            
            // Simulamos el tiempo que tarda la persona en pasar (usamos un retardo NO bloqueante)
            vTaskDelay(pdMS_TO_TICKS(1000)); // Retardo para que pase la persona
            
            // 4. Simular control de puerta (Close)
            LOGGER_INFO("Puerta A: CERRANDO.");
            // Aquí iría el código para cerrar la puerta
            
            LOGGER_INFO(p_task_a_wait_2500mS);
            
            /* --- FIN DE SECCIÓN CRÍTICA --- */
            
            // 5. Liberar el Mutex para que otras tareas (ej. Task B) puedan usar el logger
            xSemaphoreGive(h_mutex);
        }

        // 6. Retardo principal de la tarea
		vTaskDelay(TASK_A_DEL_MAX);
	}

```

**Explicación de las funciones:**

* `xSemaphoreTake(h_mutex, portMAX_DELAY)`: Esta función intenta adquirir el mutex. El segundo parámetro (`portMAX_DELAY`) indica que si el mutex está ocupado, la tarea se bloqueará indefinidamente hasta que se libere. Cuando retorna `pdTRUE`, significa que la tarea tiene "la llave" y puede continuar de forma segura.
* `xSemaphoreGive(h_mutex)`: Devuelve la llave. Es **crítico** no olvidar esta llamada, de lo contrario, si una tarea toma el mutex y no lo libera (o hay una falla dentro de la sección crítica), las demás tareas se quedarán esperando para siempre (esto se conoce como *Deadlock* o Interbloqueo).

¿Qué te parece esta modificación? Este es el núcleo conceptual del **Paso 6** en lo que respecta a la sincronización de tareas de control.

---

# Analsis de `task_b.c`

## 1. Análisis del Código Original
El código actual de `task_b.c` define un hilo de ejecución independiente con la misma prioridad y tiempo de ciclo (2500 ms) que `task_a.c`. 
Al ejecutarse concurrentemente con `task_a`, ambas tareas intentan acceder a la función `LOGGER_INFO()` simultáneamente. En un sistema RTOS, la función `LOGGER_INFO` (que usualmente maneja el periférico UART) es un **recurso compartido crítico**.

## 2. Aplicación del Patrón de Exclusión Mutua (Mutex)
Para cumplir con el Paso 6, debemos implementar el control de la puerta (Open/Close) y proteger las impresiones utilizando la "llave" que forjamos en `app.c` (`h_mutex`).

Es **fundamental** que `task_b` utilice exactamente la misma variable `h_mutex` que usa `task_a`. De esta forma, si la Puerta A está imprimiendo o modificando un contador global, la Puerta B se bloqueará (esperará) hasta que la Puerta A termine.

### Código Modificado para `task_b.c`:

```c
// 1. Declarar el Mutex global (creado en app.c)
extern SemaphoreHandle_t h_mutex;

// 2. Dentro de la función task_b, modificar el bucle infinito:
	for (;;)
	{
		// Intentamos adquirir el Mutex, esperando el tiempo necesario
		if (xSemaphoreTake(h_mutex, portMAX_DELAY) == pdTRUE)
		{
			/* --- INICIO DE SECCIÓN CRÍTICA --- */
			g_task_b_cnt++;
			
			LOGGER_INFO("Puerta B: Solicitando acceso...");
			
			// Simulación de control de puerta (Paso 6)
			LOGGER_INFO("Puerta B: ABRIENDO...");
			// [Aquí iría la acción de hardware: ej. HAL_GPIO_WritePin(...)]
			
			// Simulamos el paso de la persona (retardo no bloqueante)
			vTaskDelay(pdMS_TO_TICKS(1000));
			
			LOGGER_INFO("Puerta B: CERRANDO.");
			// [Aquí iría la acción de hardware para cerrar]
			
			LOGGER_INFO(p_task_b_wait_2500mS);
			
			/* --- FIN DE SECCIÓN CRÍTICA --- */
			
			// ¡MUY IMPORTANTE! Liberar el Mutex para no causar un Deadlock
			xSemaphoreGive(h_mutex);
		}

		/* Retardo para volver a ejecutar el ciclo de la Puerta B */
		vTaskDelay(TASK_B_DEL_MAX);
	}

```

## 3. Conclusión Conceptual

Con esta implementación en ambas tareas, el planificador de FreeRTOS garantiza que, aunque las tareas Puerta A y Puerta B coincidan en el tiempo queriendo acceder al hardware (UART), el RTOS gestionará una fila de espera ordenada. Esto previene la corrupción de datos (*race conditions*) y demuestra un diseño robusto de firmware concurrente.
