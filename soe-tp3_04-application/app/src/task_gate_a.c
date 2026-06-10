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
#define G_TASK_GATE_A_CNT_INI	0ul

#define TASK_GATE_A_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_GATE_A_DEL_MAX		(pdMS_TO_TICKS(2500ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_gate_a_wait_2500mS		= "   ==> Task Gate A  - Wait:   2500mS";

/********************** external data declaration *****************************/
uint32_t g_task_gate_a_cnt;

/********************** external functions definition ************************/
/* Task thread */

void task_gate_a(void *parameters) {
	/*  Declare & Initialize Task Function variables */
	g_task_gate_a_cnt = G_TASK_GATE_A_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

    // Inicialización ...
    for (;;) {
        // 1. Esperar petición de apertura
        xSemaphoreTake(sem_open_req_a, portMAX_DELAY);

        // 2. Tomar el control de la esclusa entera (Mutex)
        xSemaphoreTake(mutex_airlock, portMAX_DELAY);

        // 3. Abrir Puerta A (Open)
        LOGGER_INFO("Puerta A: Abierta");

        // 4. Esperar a que la persona cruce y cierre la puerta
        xSemaphoreTake(sem_door_closed_a, portMAX_DELAY);

        // 5. Cerrar Puerta A (Close)
        LOGGER_INFO("Puerta A: Cerrada");

        // 6. Liberar la esclusa para la otra puerta
        xSemaphoreGive(mutex_airlock);

    }
}

/*void task_gate_a(void *parameters)
{
	g_task_gate_a_cnt = G_TASK_GATE_A_CNT_INI;

	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	for (;;)
	{
		g_task_gate_a_cnt++;

		LOGGER_INFO(p_task_gate_a_wait_2500mS);
		vTaskDelay(TASK_GATE_A_DEL_MAX);
	}
}*/

/********************** end of file ******************************************/
