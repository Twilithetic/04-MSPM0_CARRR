# 🐛 Bug 记录：DMA UART 静默失效 — FreeRTOS 堆耗尽

> **日期**: 2026-07-26  
> **平台**: MSPM0G3507 + FreeRTOS + TI DriverLib  
> **关联 Commit**: 可工作基线 `b061dcb` → 当前 `b061dcb` + 未提交改动  
> **严重程度**: 🔴 高（UART 完全无输出，无任何错误提示）  
> **状态**: ✅ 已修复

---

## 一、现象

DMA UART（XDS110 CDC backchannel, UART0: PA10 TX / PA11 RX, 115200-8N1）**完全发不出任何数据**。

- LED 闪烁任务（BlueLED / GreenLED）仍然正常运行
- 串口终端收不到任何字符，连启动问候消息都没有
- 没有任何编译错误或运行时 hard fault
- 回到 commit `b061dcb` 时 DMA UART 正常

## 二、环境信息

| 项目 | 值 |
|------|-----|
| MCU | MSPM0G3507 (32KB SRAM, 128KB Flash) |
| RTOS | FreeRTOS (heap_4) |
| 编译器 | TI ARM Clang |
| UART | UART0 DMA TX (Channel 0), RX polling |
| 关键配置 | `configTOTAL_HEAP_SIZE = 3 × 1024`<br>`configMINIMAL_STACK_SIZE = 128` 字 (512 字节)<br>`StackType_t = uint32_t` |
| SysConfig | empty.syscfg（含 UART DMA + I2C 配置） |

## 三、排查过程

### 3.1 方法：三路并行排查

为了避免人为偏见，采用**三个独立 subagent 从不同角度同时分析**的策略：

```
角度1：uart_debug.c 删除 → "不可能，死代码无关"
角度2：FreeRTOS 堆栈/调度 → "堆耗尽!!!"
角度3：syscfg 硬件配置   → "无关，反而修复了旧bug"
       └── 结论收敛：角度2 是根因 ✅
```

### 3.2 逐步分析

#### Step 1 — 确认改动范围

```bash
$ git diff HEAD --stat

 build_tool/src/main.rs                  |  29 ++
 empty.syscfg                            |  11 +-
 src/driver/board/board_drivers_config.c |  62 ++++
 src/driver/chip/I2C_test.c              |  90 ++++++
 src/driver/chip/ti_drivers_i2c_config.c | 133 +++++++++
 src/driver/chip/ti_drivers_i2c_config.h |  55 ++++
 src/driver/line/line.c                  |  61 ----       ← 删除
 src/driver/motor/motor.c                | 504 -------      ← 删除
 src/driver/uart/uart_debug.c            |  54 ----       ← 删除
 src/include/app_tasks.h                 |   1 +           ← +vImuTask 声明
 src/include/i2c_scanner_reg.h           |  34 +++
 src/main.c                              |  10 +           ← +vImuTask 创建
 src/proxy/registers.c                   |  72 ++++-
 src/software/task.c                     |  44 ++-
 14 files changed, 526 insertions(+), 634 deletions(-)
```

#### Step 2 — 排除干扰项

| 怀疑项 | 分析 | 结论 |
|--------|------|------|
| `uart_debug.c` 被删除 | 阻塞式 UART 死代码，从未被调用；不操作 DMA 寄存器 | ❌ 无关 |
| `motor.c` / `line.c` 被删除 | 只有 Flash 代码被删，RAM 中的 register 仍在；不改变堆布局 | ❌ 无关 |
| `empty.syscfg` 修改 | SysConfig 生成代码反而**修复了**之前缺失的 DMA 初始化 | ❌ 无关（反而利好） |
| 新增 I2C 驱动代码 | 代码未被调用（`i2c_test_init()` 已注释）；I2C0 使用 PA0/PA1，不占用 DMA CH0 | ❌ 无关 |

#### Step 3 — 锁定根因

**关键发现**: `main.c` 新增了 `vImuTask` 的创建——**在 vLoggerTask 之前**，且使用了 `configMINIMAL_STACK_SIZE * 3` 的栈大小。

## 四、根因分析

### 4.1 堆消耗计算

```
configTOTAL_HEAP_SIZE    = 3 × 1024 = 3072 字节
configMINIMAL_STACK_SIZE = 128 字 × 4 字节 = 512 字节
TCB                     ≈ 140 字节
Semaphore               ≈ 80 字节
```

| # | 任务 | 栈 (字节) | TCB | 合计 |
|---|------|-----------|-----|------|
| - | Idle (自动创建) | 512 | 140 | 652 |
| - | Timer Daemon (自动) | 512 | 140 | 652 |
| 1 | BlueLED | 512 | 140 | 652 |
| 2 | GreenLED | 512 | 140 | 652 |
| 3 | **vImuTask (新增!)** | **512 × 3 = 1536** | **140** | **1676** |
| 4 | **vLoggerTask** | 512 × 2 = 1024 | 140 | 1164 |
| - | `g_tx_done_sem` | — | — | ~80 |

```
总计 = 652 + 652 + 652 + 652 + 1676 + 1164 + 80 = 5528 字节
堆上限 = 3072 字节  ← 差了 2456 字节！
```

### 4.2 创建时序：多米诺骨牌

```c
// main.c 中各 xTaskCreate 的调用顺序：

xTaskCreate(vBlueTask,   ...);   // 堆剩余: 3072 - 652  = 2420 ✅
xTaskCreate(vGreenTask,  ...);   // 堆剩余: 2420 - 652  = 1768 ✅
xTaskCreate(vImuTask,    ...);   // 堆剩余: 1768 - 1676 = 92   ⚠️ 仅剩 92 字节!
xTaskCreate(vLoggerTask, ...);   // 需要 1164 字节，堆只剩 92
                                  // → 返回 pdFALSE，静默失败 💀
```

**vLoggerTask 根本没有被创建！** 这个任务里包含了所有 `uart_send_async()` 调用。没有任务，自然没有任何 DMA UART 输出。

更糟的是：之后 `vTaskStartScheduler()` 还会尝试创建 Idle + Timer daemon（1304 字节），堆只剩 92，调度器启动也会失败。

### 4.3 为什么无声无息？

`main.c` 中没有检查 `xTaskCreate()` 的返回值：

```c
xTaskCreate(vLoggerTask, "Logger", configMINIMAL_STACK_SIZE * 2,
            NULL, 1, NULL);
// ↑ 返回 pdFALSE 但被忽略了！没有 assert，没有日志，没有 hard fault
```

## 五、修复

### ✅ 采用方案：增大 FreeRTOS 堆

```diff
// FreeRTOSConfig.h 第 326 行

-#define configTOTAL_HEAP_SIZE    ((size_t)(3 * 1024))
+#define configTOTAL_HEAP_SIZE    ((size_t)(8 * 1024))
```

MSPM0G3507 有 32KB SRAM，8KB 的 FreeRTOS 堆完全在可接受范围内。

### 🔄 备选方案

| 方案 | 操作 | 适用场景 |
|------|------|---------|
| **减小 vImuTask 栈** | `configMINIMAL_STACK_SIZE * 1` | 当前 vImuTask 只调用 `vTaskSuspend()` |
| **加断言检查** | `configASSERT(xTaskCreate(...) == pdPASS)` | 防止未来再次静默失败 |
| **使用静态分配** | `xTaskCreateStatic()` + 静态栈数组 | 堆用尽时也不影响关键任务 |

---

## 六、经验教训

### 🎯 黄金法则

1. **永远检查 `xTaskCreate()` 返回值**
   ```c
   // ❌ 不好
   xTaskCreate(task, name, stack, NULL, prio, NULL);

   // ✅ 好
   BaseType_t ret = xTaskCreate(task, name, stack, NULL, prio, NULL);
   configASSERT(ret == pdPASS);
   ```

2. **堆大小要留余量**
   - 计算公式：`堆 ≥ Σ(栈+TCB) + 信号量 + 队列 + 余量(≥30%)`
   - 本例中"刚好"的设计在加一个任务后就崩塌了

3. **多角度排查更有效**
   - 三个 subagent 独立分析，避免了"先入为主"的偏见
   - 角度之间会自然形成相互验证

4. **理解"静默失败"的危险性**
   - 没有 hard fault、没有编译错误、没有明显症状
   - 唯一线索是"某个功能不工作了"
   - 防御性编程是应对这类问题的最好手段

### 📋 排查此类 Bug 的建议清单

- [ ] 用 `git diff` / `git bisect` 确定改动范围
- [ ] 排除明显不相关的改动（死代码、注释、格式）
- [ ] 检查 FreeRTOS 堆使用情况（`xPortGetFreeHeapSize()`）
- [ ] 检查所有 `xTaskCreate` / `xQueueCreate` / `xSemaphoreCreate*` 返回值
- [ ] 计算各任务栈需求，与 `configTOTAL_HEAP_SIZE` 对比
- [ ] 启用 `configASSERT` 和栈溢出检测（`configCHECK_FOR_STACK_OVERFLOW`）

---

## 七、修复验证

增大 `configTOTAL_HEAP_SIZE` 至 `8*1024` 后：

- ✅ DMA UART 启动问候消息正常发送
- ✅ 定时日志输出正常
- ✅ LED 闪烁任务不受影响
- ✅ 所有 5 个任务（4 用户 + Idle + Timer）正常运行

---

> *"一个 1536 字节的任务栈，撑爆了 3072 字节的堆天花板。"*  
> *— 来自这次 Bug 的墓志铭*

---

## 📎 关联文件

| 文件 | 角色 |
|------|------|
| `rtos/FreeRTOSConfig.h:326` | `configTOTAL_HEAP_SIZE` 定义（修复点） |
| `src/main.c:42-45` | `vImuTask` + `vLoggerTask` 创建（触发点） |
| `src/driver/board/XDS110_cdc.c:57-66` | `uart_init()` + DMA semaphore 创建 |
| `src/software/task.c:52-93` | `vLoggerTask` — DMA UART 发送者 |
| `docs/debug-2025-07-26-dma-uart-silent-failure.md` | 本文档 |
