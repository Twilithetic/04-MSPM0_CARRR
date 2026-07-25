# MSPM0 5-Channel Gray-Scale Line Follower

基于 TI MSPM0G3507 的五路灰度循迹小车，使用 FreeRTOS 调度，硬件代理 + 影子寄存器架构。

## 硬件平台

| 项目 | 详情 |
|------|------|
| MCU | MSPM0G3507 (Cortex-M0+, 32 MHz, LQFP-64) |
| SDK | mspm0_sdk 2.10.00.04 |
| 编译器 | tiarmclang 5.1.1.LTS |
| RTOS | FreeRTOS Kernel V11.2.0 (static allocation) |
| 调试输出 | UART0, 115200 8N1 |
| 电机通信 | I2C 软件模拟 (PA12=SCL, PA13=SDA), 地址 0x26 |
| 循迹传感器 | 5 路灰度 (PA14-S1~PA17-S4, PA21-S5), 0=黑线 |
| 板载 LED | PB2=蓝色, PB3=绿色 (高电平亮) |

## 目录结构

```
├── empty.syscfg                # TI SysConfig 硬件配置文件
├── rtos/
│   └── FreeRTOSConfig.h        # FreeRTOS 配置 (tick 1kHz, preemptive, 3KB heap)
├── build_tool/
│   └── src/main.rs             # Rust 编译工具 (SysConfig → compile → link → hex)
├── src/
│   ├── main.c                  # 应用入口：初始化 + SYNC→THINK→FLUSH 主循环
│   ├── include/                # 所有头文件集中于此
│   │   ├── build_in_led.h      # 板载 LED 驱动接口
│   │   ├── line.h / line_reg.h # 循迹传感器代理 & 影子寄存器
│   │   ├── motor.h / motor_reg.h # 电机 I2C 代理 & 影子寄存器
│   │   ├── uart_debug.h        # 调试串口输出接口
│   │   ├── status_reg.h        # 系统状态影子寄存器
│   │   ├── controller.h        # PD 控制器接口
│   │   ├── task.h              # FreeRTOS 任务声明
│   │   └── app_hooks.h         # FreeRTOS 钩子声明
│   ├── driver/
│   │   ├── board/build_in_led.c # 板载 LED 驱动实现
│   │   ├── line/line.c          # 循迹传感器代理 (GPIO 读取 + 加权质心)
│   │   ├── motor/motor.c        # 电机 I2C 位带代理 (4层驱动架构)
│   │   └── uart/uart_debug.c    # 阻塞式串口调试输出
│   ├── proxy/
│   │   └── registers.c          # 全局影子寄存器实例定义 (BSS 零初始化)
│   └── software/
│       ├── controller.c         # PD 循迹控制器 (线跟踪控制算法)
│       ├── task.c               # FreeRTOS 应用任务 (LED 闪烁)
│       └── app_hooks.c          # FreeRTOS 钩子 (堆栈溢出/空闲/定时器内存)
└── .gitignore
```

## 架构：硬件代理 + 影子寄存器

```
┌──────────────────────────────────────────────────┐
│                    主循环 (main.c)                │
│                                                  │
│  [SYNC]   line.sync_from_device()    → g_line_reg │
│           motor.sync_encoder()       → g_motor_reg│
│  [THINK]  controller_calculate()   读影子寄存器    │
│  [FLUSH]  motor.flush_speed()      写 I2C 到硬件  │
│  [PRINT]  调试输出                 读影子寄存器    │
└──────────────────────────────────────────────────┘
```

### 数据流规则

- **控制器 (`controller.c`)** 只读取影子寄存器，写入 `target_speed_*` 到 `g_motor_reg`，从不直接访问硬件
- **代理 (`driver/`)** 负责硬件 I/O：`sync_*` 写入影子寄存器，`flush_*` 读取影子寄存器并写入硬件
- **主循环** 按固定顺序调用：SYNC → THINK → FLUSH → SYNC encoder(低频率) → PRINT(低频率)

## 电机驱动 (motor.c) 四层架构

```
Level 0   I2C GPIO bit-bang primitives (static)
Level 1   I2C 协议函数 (start/stop/send_byte/read_byte/ack, static)
Level 2   raw_* read/write wrappers (static)
Level 3   Public API: sync_*/flush_*/cmd_*/motor_proxy_init
```

### 电机配置

| 参数 | 值 |
|------|-----|
| 编码器类型 | TT encoder (3) |
| 编码器线数 | 13 |
| 减速比 | 45:1 |
| 轮径 | 68 mm |
| 死区 | 1250 |
| 驱动轮 | M2(左), M4(右) |
| 转向符号 | M2=-1, M4=-1 |

## PD 控制器 (controller.c)

线跟踪控制算法，可调整参数：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `LINE_LEFT_BASE_SPEED` | 110 | 左轮基准速度 |
| `LINE_RIGHT_BASE_SPEED` | 110 | 右轮基准速度 |
| `LINE_MAX_SPEED` | 180 | 最大速度 |
| `LINE_CORRECTION_MAX` | 50 | 最大修正量 |
| `LINE_KP_NUM / DEN` | 1/20 | 比例项系数 |
| `LINE_KD_NUM / DEN` | 1/45 | 微分项系数 |
| `LINE_TURN_SIGN` | -1 | 转向方向 |
| `LINE_SEARCH_SPEED` | 100 | 线丢失时搜索速度 |

### 停车条件

- **线丢失** (`active_count == 0`)：5 路传感器全白 → 根据 `LINE_SEARCH_WHEN_LOST` 决定搜索还是停车
- **全黑** (`active_count >= 4`)：十字路口或终点线 → 立即停车

## FreeRTOS 配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `configCPU_CLOCK_HZ` | 32,000,000 | 系统时钟 32 MHz |
| `configTICK_RATE_HZ` | 1000 | Tick 频率 1 kHz |
| `configUSE_PREEMPTION` | 1 | 抢占式调度 |
| `configUSE_TIME_SLICING` | 0 | 关闭时间片轮转 |
| `configTOTAL_HEAP_SIZE` | 3 KB | 动态堆大小 |
| `configMINIMAL_STACK_SIZE` | 128 words | 最小任务栈 |
| `configMAX_PRIORITIES` | 10 | 优先级数 |
| `configUSE_TICKLESS_IDLE` | 1 | 低功耗 tickless 模式 |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 | 栈溢出检测 |

### FreeRTOS 钩子

- **静态内存分配**：空闲任务和定时器任务使用静态分配 (`configSUPPORT_STATIC_ALLOCATION=1`)
- **堆栈溢出**：检测到溢出时关中断死循环
- **malloc 失败**：关中断死循环

### 应用任务

| 任务 | 功能 | 周期 |
|------|------|------|
| `vBlueTask` | 蓝色 LED (PB2) 闪烁 | 500 ms |
| `vGreenTask` | 绿色 LED (PB3) 闪烁，偏移 250 ms | 500 ms |

## 构建系统

使用 Rust 编写的构建工具 (`build_tool/`)，依次执行：

1. **SysConfig** — 根据 `empty.syscfg` 生成 `ti_msp_dl_config.c` 和设备链接器文件
2. **Compile** — `tiarmclang` 编译所有 `.c` 文件，优化级别 `-O2`
3. **Link** — 生成 `empty.out` (ELF) 和 `empty.map`
4. **Hex** — `tiarmhex` 生成 `empty.hex` (Intel HEX)

### 运行构建

```powershell
cd build_tool
cargo run
```

构建输出在 `Debug/` 目录下。

## 硬件引脚分配

| 功能 | 端口 | 引脚 | 说明 |
|------|------|------|------|
| 灰度 S1 | PA14 | PIN_14 | 五路灰度传感器通道1 |
| 灰度 S2 | PA15 | PIN_15 | 通道2 |
| 灰度 S3 | PA16 | PIN_16 | 通道3 |
| 灰度 S4 | PA17 | PIN_17 | 通道4 |
| 灰度 S5 | PA21 | PIN_21 | 通道5 |
| I2C SDA | PA13 | PIN_13 | 电机 I2C 数据线 (bit-bang) |
| I2C SCL | PA12 | PIN_12 | 电机 I2C 时钟线 (bit-bang) |
| UART TX | PA10 | PIN_10 | 调试串口发送 |
| UART RX | PA11 | PIN_11 | 调试串口接收 |
| 蓝 LED | PB2 | PIN_2 | 板载蓝色 LED |
| 绿 LED | PB3 | PIN_3 | 板载绿色 LED |
| 按键 | PA18 | PIN_18 | 用户按键 (下拉) |
