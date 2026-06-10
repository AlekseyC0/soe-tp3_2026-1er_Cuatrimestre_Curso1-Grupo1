/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"
#include "cmsis_os.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"

/********************** macros and definitions *******************************/
#define G_TASK_B_CNT_INI	0ul

#define TASK_B_DEL_ZERO		(pdMS_TO_TICKS(0ul))
#define TASK_B_DEL_MAX		(pdMS_TO_TICKS(2500ul))

/********************** internal data declaration ****************************/
extern SemaphoreHandle_t h_room_empty;
/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_b_wait_250mS			= "   ==> Task    B - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_b_cnt;

/********************** external functions definition ************************/
/* Task thread */
void task_b(void *parameters)
{
	/*  Declare & Initialize Task Function variables */
	g_task_b_cnt = G_TASK_B_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
    {
		/* Update Task Counter */
		g_task_b_cnt++;
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
    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_b_wait_250mS);
		vTaskDelay(TASK_B_DEL_MAX);
	}
}

/********************** end of file ******************************************/
