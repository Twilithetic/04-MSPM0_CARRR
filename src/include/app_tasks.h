/*
 *  ======== app_tasks.h ========
 *  Application task declarations.
 *
 *  Files:
 *    task_indicator.c  — LED heartbeat + logger telemetry output
 *    task_init.c       — one-shot init (I2C scan, motor config)
 *    task_telemetry.c  — sensor telemetry (IMU sync, motor sync, stats)
 *    task_control.c    — car speed / steering control
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <FreeRTOS.h>
#include <task.h>

void vBlueTask(void *pvParameters);
void vGreenTask(void *pvParameters);
void vLoggerTask(void *pvParameters);
void vI2CScanTask(void *pvParameters);
void vMotorInitTask(void *pvParameters);
void vLSM6DSV16XSyncTask(void *pvParameters);
void vMotorSyncTask(void *pvParameters);
void vCarCtrlTask(void *pvParameters);
void vStatsTask(void *pvParameters);

#endif /* APP_TASKS_H */
