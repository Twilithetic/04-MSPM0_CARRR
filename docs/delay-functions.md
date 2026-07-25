# MSPM0 项目中的延迟 / 延时函数

> FreeRTOS + DriverLib 环境下的所有延迟方式速查

---

## 一、阻塞延迟（CPU 空转）

### `delay_cycles(uint32_t cycles)`

TI DriverLib 提供的**总线循环延迟**，CPU 空转等待指定周期数。

```c
#include "ti_msp_dl_config.h"   // 或 <ti/devices/msp/m0p/dl_m0p_utils.h>

delay_cycles(n);   // 阻塞 n 个 CPU 时钟周期
```

**换算：** MSPM0G3507 默认 32MHz

| 时长 | 周期数 | 宏定义（项目中） |
|------|--------|-----------------|
| 1μs | 32 | — |
| 1ms | 32,000 | `DELAY_1MS_CYCLES` |
| 100ms | 3,200,000 | `DELAY_100MS_CYCLES` |

```c
// 实际用法（motor.c）
#define DELAY_1MS_CYCLES   (32000U)
#define DELAY_100MS_CYCLES (3200000U)

delay_cycles(DELAY_1MS_CYCLES);     // 卡 1ms
delay_cycles(DELAY_100MS_CYCLES);   // 卡 100ms
```

| 特性 | 说明 |
|------|------|
| 精度 | ✅ 微秒级，非常精确 |
| CPU 利用率 | ❌ 100% 空转 |
| 调度器 | ❌ 不让出 CPU，其他 FreeRTOS 任务饿死 |
| 适用场景 | I2C bit-bang、等硬件稳定等极短时序 |
| **不要在任务里用！** | 会卡住整个系统 |

---

## 二、FreeRTOS 异步延迟（让出 CPU）⭐ 推荐

### 时钟配置

```c
// rtos/FreeRTOSConfig.h
#define configTICK_RATE_HZ  1000    // tick 频率 = 1000Hz
                                    // 1 tick = 1ms
```

### `pdMS_TO_TICKS(ms)` — 毫秒转 tick

```c
// rtos/FreeRTOS/include/projdefs.h
pdMS_TO_TICKS(500)   // → 500 ticks = 500ms
pdMS_TO_TICKS(10)    // → 10  ticks = 10ms
pdMS_TO_TICKS(1)     // → 1   tick  = 1ms  (最小值)
```

---

### 1. `vTaskDelay()` — 相对延时

**"从现在起，等 X 毫秒"**

```c
#include <FreeRTOS.h>
#include <task.h>

void vTaskDelay(const TickType_t xTicksToDelay);
```

```c
// 示例：每 500ms 翻转一次 LED
void vBlueTask(void *pvParameters) {
    for (;;) {
        blue_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));  // 休眠 500ms，让出 CPU
    }
}
```

| 特性 | 说明 |
|------|------|
| 语义 | 从**当前时刻**起等 X ms |
| 周期精度 | ⚠️ 会漂移！任务执行时间越长，实际周期越长 |
| 最小延时 | 1 tick = 1ms |
| 传 0 | 不延时，仅触发一次任务切换 |
| 适用场景 | 简单定时（LED 闪烁）、偶尔等待 |

**周期漂移示意：**
```
期望周期: 100ms
实际:     |--任务(5ms)--|--vTaskDelay(100ms)--|--任务(5ms)--|--vTaskDelay(100ms)--|
周期 = 105ms（多了 5ms），长期会漂移！
```

---

### 2. `vTaskDelayUntil()` — 绝对周期延时 ⭐ 控制循环专用

**"从上一次唤醒起，每隔 X 毫秒执行一次"**

```c
#include <FreeRTOS.h>
#include <task.h>

BaseType_t vTaskDelayUntil(TickType_t *pxPreviousWakeTime,
                           const TickType_t xTimeIncrement);
```

```c
// 示例：精确 10ms 控制循环
void vControlTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();  // 记录起始时刻

    for (;;) {
        // ↓ 业务逻辑 ↓
        sync_from_device(&g_line_proxy, &g_line_reg);
        controller_calculate(&g_line_reg, &g_motor_reg, &g_status_reg);
        flush_speed_to_device(&g_motor_proxy, &g_motor_reg);
        // ↑ 业务逻辑 ↑

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
        // 保证：不管业务逻辑跑了多久，两次进入循环的间隔始终是 10ms
    }
}
```

| 特性 | 说明 |
|------|------|
| 语义 | 距**上一次**唤醒等满 X ms |
| 周期精度 | ✅ 绝对周期，不漂移 |
| 返回值 | `pdTRUE` = 正常延时，`pdFALSE` = 超时了（业务逻辑跑太久） |
| 适用场景 | **控制循环**、定时采样、PID 控制 |
| 最小延时 | 1 tick = 1ms |

**不漂移示意：**
```
期望周期: 10ms
实际:     |--任务(3ms)--|--等 7ms--|--任务(5ms)--|--等 5ms--|--任务(4ms)--|--等 6ms--|
周期 = 严格 10ms ✅ 永不漂移！
```

---

## 三、速查口诀

```
delay_cycles()  →  I2C 时序专用，其他地方别用
vTaskDelay()    →  等一等再干活（会漂移）
vTaskDelayUntil() → 定时干活，周期精确（控制循环首选）
```

---

## 四、实际用法速查表

| 场景 | 用什么 | 示例代码 |
|------|--------|---------|
| I2C bit-bang 时序 | `delay_cycles(160)` | 见 `motor.c` 第 56 行 |
| 电机配置后等待稳定 | `delay_cycles(DELAY_100MS_CYCLES)` | 见 `motor.c` 第 468 行 |
| LED 每 500ms 闪烁 | `vTaskDelay(pdMS_TO_TICKS(500))` | 见 `task.c` 第 18 行 |
| 10ms 循线控制 | `vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10))` | 见上方示例 |
| 100ms 传感器采样 | `vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(100))` | — |
| 1 秒心跳打印 | `vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000))` | — |

---

## 五、底细：不想用 FreeRTOS 时

如果没开 FreeRTOS，唯一可用的阻塞延迟就是：

```c
// SysTick 定时器（ARM Cortex-M 自带）
// DriverLib 封装
DL_SYSCTL_delay(ms);    // 阻塞毫秒延时（DriverLib 未确认有此 API，需查手册）

// 或自己用 SysTick 实现
void delay_ms(uint32_t ms) {
    // 手动配置 SysTick…
}
```

但项目已用 FreeRTOS，SysTick 被 RTOS 接管，直接用 `vTaskDelay` 即可。

---

*2026-07-25 整理，随项目演进更新*
