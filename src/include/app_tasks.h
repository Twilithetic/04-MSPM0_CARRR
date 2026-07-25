/*
 *  ======== app_tasks.h ========
 *  Application task declarations.
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <FreeRTOS.h>
#include <task.h>

void vBlueTask(void *pvParameters);
void vGreenTask(void *pvParameters);

#endif /* APP_TASKS_H */
