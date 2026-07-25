/*
 *  ======== app_hooks.h ========
 *  FreeRTOS hook declarations.
 *  Task declarations are in app_tasks.h.
 */

#ifndef APP_HOOKS_H
#define APP_HOOKS_H

#include <FreeRTOS.h>
#include <task.h>

void vApplicationMallocFailedHook(void);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize);
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize);

#endif /* APP_HOOKS_H */
