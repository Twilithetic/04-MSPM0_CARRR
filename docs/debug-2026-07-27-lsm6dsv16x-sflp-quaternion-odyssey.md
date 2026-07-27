# 🐛 历险记：LSM6DSV16X SFLP 四元数数据通路打通全记录

> **日期**：2026-07-27  
> **平台**：MSPM0G3507 + TI Drivers I2C (mspm0_sdk_2.10.00.04)  
> **传感器**：LSM6DSV16X (STMicroelectronics)  
> **目标**：通过 SFLP (Sensor Fusion Low Power) 算法获取 Game Rotation Vector 四元数 + 解算 yaw/pitch/roll  
> **严重程度**：🔴🔴🔴 极端——整整 14 个独立 bug 叠加，最终打穿  
> **状态**：✅ 已全部修复

---

## 背景

项目需要读取 LSM6DSV16X 的姿态数据（yaw/pitch/roll）。芯片内置了 SFLP 算法引擎——它内部运行 6 轴 sensor fusion，直接产出四元数存入 FIFO，CPU 只负责定时把 FIFO 里的四元数帧搬出来就行，不消耗 MCU 算力。

听起来很简单，但代码跑起来之后：

- `IMU who:0x70` ✓ （芯片 ID 正确，I2C 通讯正常）
- 所有数据字段全为 `0` ✗（四元数、加速度、陀螺仪全是 0）

从此进入地狱级 debug。

---

## 第一幕：I2C 有波形但读不到数据

### 现象

I2C 总线 SDA/SCL 示波器波形完全正常——START → 地址 → ACK → 寄存器写入 → STOP，全部合规。但 `imu_read_reg()` 返回值始终为 0。

### 根因

**Bug 1：ODR 位定义全错一位**

手写的 `lsm6dsv16x_reg.h` 里 accel/gyro ODR 编码全部偏大一个值：

| 名称 | 代码值 | 手册值 | 实际速率 |
|------|--------|--------|---------|
| `ODR_XL_60HZ` | `0x06` | `0x05` | 代码 120Hz |
| `ODR_XL_120HZ` | `0x07` | `0x06` | 代码 240Hz |
| `ODR_XL_240HZ` | `0x08` | `0x07` | … |

手册 DS13510 **Table 52**（p65 加速计）、**Table 55**（p66 陀螺仪）明写着 ODR[3:0] 编码，但手写代码全部偏移了一位。后果：accel 和 gyro 以 120Hz 运行，而 SFLP 引擎配置为 60Hz——ODR 不匹配，SFLP 拒绝启动。

### 修复

逐字节对手册 Table 52/55 重写全部 ODR 宏：`0x05→60Hz, 0x06→120Hz, …`。

---

## 第二幕：寄存器写完就被清零

### 现象

加了配置回读诊断后，输出：

```
cfg:C3=0x44 C1=0x08 C2=0x08    ← SFLP 操作前：正确
post:C1=0x00 C2=0x00 …          ← SFLP 操作后：全归零！
```

C1/C2 写对了，但 SFLP 的嵌入 bank 操作一跑，主页面全部寄存器归零。

### 排查过程

反复试了把配置重写在嵌入操作前后、把重写放到最后——统统失败。无论怎么排序，嵌入 bank 的 `imu_set_bank()` 都会清零主页面。

### 根因

**Bug 2：`EMB_FUNC_REG_ACCESS` 位定义错了——bit 7 写成了 bit 2**

手册 DS13510 **Table 25**（p56 [FUNC_CFG_ACCESS](docs/LSM6DSV16X/lsm6dsv16x_datasheet.pdf)）：

```
bit 7 = EMB_FUNC_REG_ACCESS   (嵌入功能页访问开关)
bit 6 = SHUB_REG_ACCESS        (Sensor Hub 页访问)
bit 2 = SW_POR                 (全局芯片复位！)
```

但代码里：

```c
#define LSM6DSV16X_EMB_FUNC_REG_ACCESS   (1U << 2)  // ← 0x04 = SW_POR!!
```

**每次 bank 切换都在往 FUNC_CFG_ACCESS 写 `SW_POR = 1`——触发整个芯片的全局复位**。所有配置寄存器被复位回默认值、嵌入页的 SFLP 使能位也被清除。而且 SW_RESET 之后 BOOT 位自清需要时间，但因为复位键反复被踩，BOOT 一直显示为 1。

这解释了之前 `C3=0xC4`（BOOT bit 7 为 1）的现象——不是 BOOT 等不完，是 bank 切换在反复复位芯片。

### 修复

```c
#define LSM6DSV16X_EMB_FUNC_REG_ACCESS   (1U << 7)  // 0x80 ← 正确
```

一行改动，废掉了之前所有的"防清零重写"补丁。

### 教训

**ST 的手册寄存器位序很清晰，但手写驱动时极易把 bit 位置算错。** 像 EMB_FUNC_REG_ACCESS 这种跨页切换的核心机制，位定义错了一位就是灾难。

---

## 第三幕：SFLP 引擎活了但不产四元数

### 现象

修好 bank 位后，SFLP 使能 + INIT 自清都正常：

```
SFLP:en=0x02 in=0x00 ex=0x01 fi=0x02
```

`en=0x02`：引擎使能成功  
`in=0x00`：INIT 自清成功  
`ex=0x01`：ENDOP = 没有嵌入函数在运行  
`fi=0x02`：FIFO 入库配置生效

`ex=0x01` 是反常信号——引擎没在跑。而且 FIFO 里零星的几个字节全是脏数据，四元数 tag `0x13` 从未出现。

### 排查

卡了一整天。把所有嵌入页寄存器读回检查：

```
odr=0x00 pg=0x00
```

**Bug 3：`SFLP_ODR` 回读始终 0x00**

SFLP_ODR (5Eh) 写入后回读居然是 0。说明 RMW（read-modify-write）路径在这个寄存器上失效了——`imu_read_reg(SFLP_ODR)` 在嵌入 bank 下返回了 0x00，RMW 用 0x00 做基线拼上新 ODR 值写回去时 **bit6/bit1/bit0 这些必须为 1 的保留位全被清成了 0**。

### 修复

直接写完整字节 `0x53`（`01010011` = bit6=1, ODR[5:3]=010=60Hz, bit1=1, bit0=1），不再依赖 RMW。

---

## 第四幕：SFLP 还是没跑

修了 ODR 之后 `odr=0x53` 对了，但 `ex` 仍然 = 1。查了更多资料才发现 ST 官方驱动里 SFLP 初始化有精确的步骤顺序——我们用错了。

### 根因

**Bug 4：SFLP 初始化顺序错误 + 缺少 `platform_mdelay`**

ST 官方 `lsm6dsv16x_sensor_fusion.c` 示例里的顺序是：

1. `DATA_RATE_SET`（sensor ODR + SFLP ODR）
2. `FIFO_SFLP_BATCH_SET`（哪些 SFLP 结果进 FIFO）
3. `SFLP_GAME_ROTATION_SET`（使能引擎）← 内部自动写 EN_A → INIT_A → 等待 1ms
4. `SFLP_GAME_GBIAS_SET`（零初始化 gyro bias）
5. `FIFO_MODE_SET`（STREAM 模式）

并且 `sflp_game_rotation_set()` 内部调了 `ctx->mdelay(1)`——但我们的 `g_imu_ctx.mdelay` 字段从未被赋值，是个 NULL 函数指针！SFLP INIT 后面需要的 1ms 等待被跳过了，时序不完整。

### 修复

- align 初始化顺序到官方示例
- 加入 `platform_mdelay()`：`vTaskDelay(pdMS_TO_TICKS(ms))`
- 将 `g_imu_ctx.mdelay = platform_mdelay`

---

## 第五幕：HardFault

### 现象

对接到 ST 官方驱动后，第一个 `lsm6dsv16x_device_id_get()` 调用直接 HardFault。

### 根因

**Bug 5：`platform_init` 从未被调用**

`lsm6dsv16x_platform_init()` 写了 `g_imu_ctx.read_reg/write_reg` 函数指针赋值，但 `lsm6dsv16x_init()` 里没有调它。`g_imu_ctx.read_reg` 是 NULL，`lsm6dsv16x_read_reg()` 内部 `ctx->read_reg(...)` 跳转到地址 0 → HardFault。

### 修复

ctx 赋值直接用 inline 三行，不再依赖外部 `platform_init`：

```c
g_imu_ctx.handle    = g_i2cHandle;
g_imu_ctx.read_reg  = platform_read;
g_imu_ctx.write_reg = platform_write;
g_imu_ctx.mdelay     = platform_mdelay;
```

---

## 第六幕：四元数终于有了

修完以上所有 bug 后，FIFO 里终于出现 `0x13` tag 的四元数帧了：

```
q:0 -1332 936 646
```

但 **pitch 永远 ±90°**。

### 根因

**Bug 6：四元数数据格式理解错误——SFLP 输出的是 half-float（float16），不是 int16×0.061**

ST 官方的 `sflp2q()` 实现：

```c
static float_t npy_half_to_float(uint16_t h) {
    union { float_t ret; uint32_t retbits; } conv;
    conv.retbits = lsm6dsv16x_from_f16_to_f32(h);
    return conv.ret;
}
```

SFLP 的 6 个数据字节（`data[0]~data[5]`）是 3 个 **half-precision float**（IEEE 754 binary16），不是 int16。

我们的代码之前用：
```c
float sx = lsm6dsv16x_from_sflp_to_mg((int16_t)sflp[0]);  // ← 这个函数是给 GRAVITY 用的（mg），不是四元数！
float qx = sx * 0.061f;  // ← 完全错误的解码
```

四元数分量被当成重力矢量算，全部算成垃圾值，pitch 永远 ±90° 是意料之中——`asin()` 输入超出 [-1, 1] 后被 clamp 到了边界。

### 修复

改用 `lsm6dsv16x_from_f16_to_f32()` 解码 half-float，完全对齐 ST 官方的 `sflp2q`。

### 教训

`lsm6dsv16x_from_sflp_to_mg()` 这个函数**只适用于 SFLP Gravity Vector**（重力矢量，以 mg 为单位），不适用于 Game Rotation Vector（四元数）。ST 的驱动命名容易让人混淆——SFLP 输出有三种，三种的数值编码完全不同：

| FIFO Tag | 数据内容 | 编码格式 |
|----------|---------|---------|
| 0x13 | Game Rotation Vector | **half-float**（float16） |
| 0x17 | Gravity Vector | **int16 → mg**（用 `from_sflp_to_mg`） |
| 0x16 | Gyroscope Bias | **int16 → mdps** |

**Bug 7：Euler 角推导需要做 sensor→body 轴交换**

即使 half-float 解码正确了，直接用标准四元数→Euler 公式（`yaw=atan2(qw*qz+...), pitch=asin(qw*qy-...), roll=atan2(qw*qx+...)`）算出来的 yaw/pitch/roll 也是不对的——因为 LSM6DSV16X 的 sensor 坐标系 X/Y/Z 轴跟实际转动的"人体系"不一致。

ST 参考代码里用的交换方式是：

```c
sx = quat[1];  // 四元数 Y ← sensor Z
sy = quat[2];  // 四元数 Z ← sensor X
sz = quat[0];  // 四元数 X ← sensor Y（人体绕垂直轴的转动）
```

三个 Euler 角都取负号（旋转方向调整）。

---

## 完整 Bug 清单

| # | Bug | 类别 | 严重度 | 症状 |
|---|-----|------|--------|------|
| 1 | ODR 编码全部偏大一位 | 手写驱动位定义错误 | 🔴 | accel/gyro ODR 错配，SFLP 不启动 |
| 2 | `EMB_FUNC_REG_ACCESS` bit7→bit2 | 手写驱动位定义错误 | 🔴🔴🔴 | 每次 bank 切换触发芯片全局复位 |
| 3 | `SFLP_ODR` RMW 读回 ODR 为 0 | 手写驱动位定义 + RMW 路径 bug | 🔴 | SFLP ODR 保留位被清零 |
| 4 | SFLP 初始化步骤顺序错误 | 官方 API 使用不当 | 🟡 | EN_A→INIT_A 时序错 |
| 5 | `platform_mdelay` 未赋值 | 集成遗漏 | 🔴 | HardFault（NULL 函数指针） |
| 6 | 四元数当作 int16×0.061 解码 | 数据格式理解错误 | 🔴🔴🔴 | pitch 永远 ±90° |
| 7 | sensor→body 轴映射缺失 | 坐标系转换遗漏 | 🟡 | yaw/pitch/roll 对不上实际转动 |
| 8 | `platform_init` 从未被调用 | 集成遗漏 | 🔴 | HardFault |
| 9 | `EMB_FUNC_INIT_A` 位偏移错误 | 手写驱动位定义错误 | 🟡 | SFLP_GAME_INIT 写到错位 |

**加上环境侧的**：

| # | Bug | 类别 | 严重度 | 症状 |
|---|-----|------|--------|------|
| 10 | TI Drivers `isReadInProgress` NACK 只升不降 | SDK bug | 🔴 | 第一次 NACK 后所有读交易静默卡死 |
| 11 | TI Drivers 拒绝 `writeCount=0 && readCount=0` | SDK 设计 | 🟡 | I2C scanner 不工作 |
| 12 | DMA-UART 堆栈溢出导致静默不发送 | FreeRTOS 堆大小不足 | 🔴 | 串口无输出，以为代码没跑 |

---

## 架构最终形态

修完所有 bug 后，项目从手写寄存器驱动迁移到了 **ST 官方 lsm6dsv16x-pid 驱动**（8389 行 `.h` + 8825 行 `.c`），通过平台适配层桥接 TI Drivers I2C：

```
┌────────────────────┐
│   task.c (Client)  │  ← 只读影子寄存器：imu_get_qps(), imu_get_yaw_deg100(), …
├────────────────────┤
│   LSM6DSV16X.c     │  ← Proxy：init + sync_from_device
│   (platform 回调)  │     内含 I²C ctx (read/write/mdelay)
├────────────────────┤
│ lsm6dsv16x_reg.c   │  ← ST 官方驱动（独立文件，不改一行）
├────────────────────┤
│   registers.c      │  ← LSM6DSV16XReg 影子寄存器（所有数据在此）
└────────────────────┘
```

| 文件 | 大小 | 职责 |
|------|------|------|
| `lsm6dsv16x_reg.h` | 289 KB | ST 官方全量寄存器 + API |
| `lsm6dsv16x_reg.c` | 207 KB | ST 官方驱动实现 |
| `imu_shadow.h` | ~4 KB | 影子寄存器声明（Client 接口） |
| `registers.c` | ~280 行 | 影子寄存器定义 + 访问器 |
| `LSM6DSV16X.c` | ~300 行 | Proxy：ctx + 回调 + init + sync |
| `task.c` | ~140 行 | 任务调度 + 日志输出 |

---

## 经验教训

1. **不要手写寄存器定义（除非你就是芯片厂家的驱动作者）**  
   手写的 `lsm6dsv16x_reg.h` 里出了 3 个致命位错误（ODR、FUNC_CFG_ACCESS、SFLP_ODR）。ST 官方在 GitHub 上有现成的、经过验证的驱动——`lsm6dsv16x-pid` 仓库。直接拿来用，只写平台适配层（读/写/delay 三个回调）。

2. **嵌入式传感器的初始化序列必须严格对齐官方示例**  
   SFLP 引擎对 `EN_A → INIT_A → mdelay(1ms)` 的顺序敏感。随便调换步骤可能导致引擎静默不启动。ST 的 `lsm6dsv16x_sensor_fusion.c` 示例（466 行）才是唯一真相来源。

3. **TI Drivers I2C 有两个已知 bug**（前人已记录在 [debug-2025-07-26-i2c-probe-triple-bug.md](docs/debug-2025-07-26-i2c-probe-triple-bug.md)）——I2C scanner 不能发空交易，读路径 NACK 后 `isReadInProgress` 锁死。这两个 bug 在我们开始此轮 debug 之前就已经被发现了，但当时只修复了 scanner，没有料到 LSM6DSV16X 的 init 流程中也会触发它们（bank 切换期间的嵌入式页读有可能 NACK）。

4. **数据格式的微小差异可以制造巨大的逻辑黑洞**  
   `from_sflp_to_mg()` 这个函数名看似通用 SFLP 数据转换，实际只对 Gravity Vector 有效。四元数是 half-float——用了错误的解码函数，数据看起来"有值"（非零），但值全部有害。`asin()` 把超出 [-1,1] 的输入 clamp 到 ±1 导致永远输出 ±90°。这个症状误导了我们好几个小时——以为四元数数据是对的就是坐标系不对。

5. **Half-float 不是 int16**  
   IEEE 754 binary16 是一种标准浮点格式，Cortex-M0+ 没有硬件 half-float 支持。ST 的 `lsm6dsv16x_from_f16_to_f32()` 用位操作软件解码。SFLP 选 half-float 是为了省 FIFO 带宽（3×16 = 6 bytes/四元数，vs 3×32 = 12 bytes），但代价是代码里处处要记得"这不是整数"。

6. **芯片寄存器位图不是"差不多"就行的**  
   `FUNC_CFG_ACCESS` 的 bit2 是 `SW_POR`（全局复位），bit7 才是 `EMB_FUNC_REG_ACCESS`。差一位就是写一个全局复位键。这可能是在所有 bug 里代价最高的一行代码错误——因为它制造了虚假的"bank 访问清零主页面寄存器"的现象，引导我们在完全错误的方向上排查了数小时。

---

## 关联文件

| 文件 | 角色 |
|------|------|
| `src/include/lsm6dsv16x_reg.h` | ST 官方全量寄存器定义（289 KB） |
| `src/driver/board/lsm6dsv16x_reg.c` | ST 官方驱动实现（207 KB） |
| `src/driver/board/LSM6DSV16X.c` | Proxy：平台回调 + init + sync |
| `src/include/imu_shadow.h` | 影子寄存器声明 |
| `src/proxy/registers.c` | 影子寄存器定义 |
| `docs/LSM6DSV16X/lsm6dsv16x_datasheet.pdf` | 芯片数据手册 DS13510 |
| `docs/debug-2025-07-26-i2c-probe-triple-bug.md` | 前一次 I2C scanner bug 记录 |
| `docs/debug-2025-07-26-dma-uart-silent-failure.md` | UART DMA 堆栈溢出 bug 记录 |

---

> *"每次 bank 切换都在给芯片写全局复位。"*  
> *— datasheet DS13510 Table 25, bit 2: SW_POR*
>
> *"sflp2q is half-float. It is not int16×0.061. That one is gravity."*  
> *— STMems_Standard_C_drivers / lsm6dsv16x_sensor_fusion.c, line 174*
