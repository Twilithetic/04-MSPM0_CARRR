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
void vI2CScanTask(void *pvParameters);
void vImuPollTask(void *pvParameters);
void vLoggerTask(void *pvParameters);
void vMotorInitTask(void *pvParameters);
void vMotorSyncTask(void *pvParameters);
void vCarCtrlTask(void *pvParameters);
void vStatsTask(void *pvParameters);

#endif /* APP_TASKS_H */
