# Hardware Proxy + Shadow Register 架构指南规范

> 版本：v1.0 | 适用范围：本项目所有嵌入式外设驱动开发 | 最后更新：2026-07-20

---

## 一、模式总览

### 1.1 模式名称

**Hardware Proxy Pattern with Shadow Registers（硬件代理 + 影子寄存器模式）**

### 1.2 核心思想

```
┌─────────────────────────────────────────────────────┐
│  Application / Tasks（Client 层）                      │
│  → 只读写本地 Shadow Register，不碰任何总线/外设       │
├─────────────────────────────────────────────────────┤
│  Hardware Proxy（代理层）                              │
│  ┌─────────────────────────────────────────────┐     │
│  │  Shadow Registers（影子寄存器）                 │     │
│  │  - 原子类型字段，中断/task 安全                │     │
│  │  - 是远端硬件状态的"本地真理"                  │     │
│  └──────────────────┬──────────────────────────┘     │
│  Sync/Flush 操作接口                               │
│  ├─ sync_*():  从总线读 → 更新影子寄存器（无返回值） │
│  └─ flush_*(): 从影子寄存器读 → 写总线（不接受参数） │
├─────────────────────────────────────────────────────┤
│  Hardware（真实硬件）                                 │
│  CAN / SPI / I2C / UART / GPIO ...                   │
└─────────────────────────────────────────────────────┘
```

**一句话概括**：Proxy 是墙，Shadow Register 是黑板——Client 只看黑板，Proxy 负责把墙外的真实世界同步到黑板上。

### 1.3 为什么用这个模式

| 问题 | 本模式的解决方案 |
|------|-----------------|
| 每次读取都要等总线回应（延迟高） | 影子寄存器缓存最新值，读取零延迟 |
| 多个 task 并发读写寄存器 | 原子类型天然线程安全 |
| 上层代码和总线协议紧耦合 | Proxy 封装全部协议，Client 无感知 |
| 状态分散在 driver 内部 | 所有状态集中在 software 模块，全局可见 |

### 1.4 和其他模式的区分

| 模式 | Query（读） | Command（写） | 和本模式的区别 |
|------|------------|--------------|---------------|
| **CQS / CQRS** | 返回数据，无副作用 | 有副作用，不返回值 | CQS 的 Query 必须返回值 |
| **Observer** | 被动接收通知更新 | 不涉及 | Observer 不含主动同步 |
| **Shadow Register (纯硬件)** | 硬件自动复制 | 硬件自动复制 | 纯硬件影子寄存器由芯片自动管理 |
| **★本模式** | **无返回值**，写入影子寄存器 | **不接受数据参数**，从影子读 | 主动同步 + 主动刷新 |

---

## 二、模块组织规范

### 2.1 文件结构

```
src/
├── driver/                          # 代理层（与硬件通信）
│   └── board/
│       ├── jc2804.rs                # Proxy 实现：CAN 协议、Sync/Flush 操作
│       └── tja1050.rs               # 通信接口封装（CAN 总线）
├── proxy/                           # 影子寄存器层（本地状态缓存）
│   ├── register.rs                  # 遥测/数据影子寄存器
│   ├── status.rs                    # 状态影子寄存器
│   └── setting.rs                   # 设置/配置影子寄存器
├── software/                        # Client 层（消费者）
│   └── tasks.rs                     # 异步任务
└── main.rs                          # 启动入口：Peripherals::init → spawn tasks
```

### 2.2 铁律

> **`driver/` 绝对不 import `software/` 的逻辑层，只 import `proxy/` 的影子寄存器 struct 类型。**
>
> **`software/` 绝对不 import `driver/` 的总线操作函数，只 import Proxy 提供的 trait 和 `proxy/` 的影子寄存器。**

跨层只通过 **trait + 影子寄存器引用** 通信。

---

## 三、影子寄存器规范（proxy/register.rs | status.rs | setting.rs）

### 3.1 类型选择

| 数据类型 | 使用原子类型 | 说明 |
|---------|------------|------|
| 布尔标志 | `AtomicBool` | 如 closed_loop、idle、ready |
| 小整数状态 | `AtomicU8` | 如 control_mode |
| 16位数值 | `AtomicU16` | 如电压(mV)、电流(mA)、温度(0.1°C) |
| 32位有符号数值 | `AtomicI32` | 如速度(mrad/s)、位置(mrad) |
| 32位标志位图 | `AtomicU32` | 如错误码位图 |
| 字符串/数组 | `static mut` + 临界区 | 如 CDC_MSG，仅在单个 task 写入时可用 unsafe |

### 3.2 结构体定义模板

```rust
/// JC2804 遥测影子寄存器（原子访问，中断/task 安全）
pub struct Jc2804TelemetryReg {
    /// 电源电压 (mV) — sync_from_device 更新
    pub voltage: AtomicU16,
    /// 实时位置 (mrad) — sync_from_device 更新
    pub position: AtomicI32,
    /// 目标位置 (mrad) — flush_to_device 读取
    pub target_position: AtomicI32,
}

impl Jc2804TelemetryReg {
    /// 常量构造器（从 0 初始化）
    pub const fn new() -> Self {
        Self {
            voltage: AtomicU16::new(0),
            position: AtomicI32::new(0),
            target_position: AtomicI32::new(0),
        }
    }
}
```

### 3.3 全局静态实例

```rust
/// 每个物理设备 → 一个全局静态影子寄存器实例
pub static JC2804_D1: Jc2804TelemetryReg = Jc2804TelemetryReg::new();
pub static JC2804_D2: Jc2804TelemetryReg = Jc2804TelemetryReg::new();
```

### 3.4 字段注释规则

每个字段必须标注：
1. **物理单位**（如 mV、mA、mrad/s）
2. **数据流方向**（`sync_from_device` 或 `flush_to_device`）
3. **精度信息**（如 `raw*0.1°C`）

### 3.5 Ordering 选择

| 场景 | 推荐 Ordering |
|------|--------------|
| 遥测更新（写） | `Relaxed` — 只关心最终一致性 |
| 标志位设置（如 can_ready） | `Release` / `Acquire` — 有 happens-before 关系 |
| 通用读写 | `Relaxed` — 本项目均为非关键同步路径 |

---

## 四、Proxy 层规范（driver/board/jc2804.rs）

### 4.1 Proxy struct 最小化原则

```rust
/// Proxy struct 只存寻址信息，不存状态
pub struct Jc2804 {
    pub id: u8,           // CAN 设备地址
    // 不放状态！不放缓存！
}
```

### 4.2 Trait 划分原则

按 **语义领域** 拆分 trait，每个 trait 只做一件事：

| Trait | 职责 | 示例方法 |
|-------|------|---------|
| `XxxRegister` | sync_from_device：读总线 → 写影子寄存器 | `sync_voltage_from_device()`, `sync_position_from_device()` |
| `XxxStatus` | 状态同步 + 设备命令 | `sync_error_from_device()`, `flush_closed_loop_to_device()` |
| `XxxSettings` | 配置同步 + 配置刷新 | `flush_control_mode_to_device()`, `sync_control_mode_from_device()` |
| `XxxOperator` | 动作命令（flush_to_device + cmd） | `flush_position_to_device()`, `cmd_reboot()` |

### 4.3 Sync 方法模板（读总线 → 写影子寄存器）

```rust
pub trait Jc2804Register {
    fn id(&self) -> u8;

    /// sync_from_device: 读 CAN 总线电源电压 → 写入影子寄存器
    /// - 不返回值（Client 直接从影子寄存器读）
    /// - 总线读取失败时静默跳过（保留旧值）
    async fn sync_voltage_from_device(&self, can: &mut Tja1050, reg: &Jc2804TelemetryReg) {
        let rx = RX_ID_BASE + self.id() as u16;
        if let Some(v) = raw_read_u16_reg(can, self.id(), REG_VOLTAGE, rx).await {
            // 协议单位 → 影子寄存器单位（*0.1V → mV）
            reg.voltage.store(v.saturating_mul(100), Ordering::Relaxed);
        }
    }
}
```

**规则**：
- 函数名用 `sync_*` 前缀
- 不返回值
- 影子寄存器引用作为参数传入
- 单位转换在 `store` 之前完成
- 失败时**不修改影子寄存器**（保留旧值）

### 4.4 Flush 方法模板（读影子寄存器 → 写总线）

```rust
pub trait Jc2804Operator {
    fn id(&self) -> u8;

    /// flush_to_device: 从影子寄存器 `target_position` 读目标值 → CAN 写
    /// - 不接受数值参数（由上层业务逻辑先 store 到影子寄存器）
    /// - 可返回 bool 表示执行结果
    async fn flush_position_to_device(
        &self,
        can: &mut Tja1050,
        reg: &Jc2804TelemetryReg,
    ) -> bool {
        let target_mrad = reg.target_position.load(Ordering::Acquire);
        // 影子单位 mrad → 协议单位 *0.01°
        let deg = target_mrad as f32 * 360.0 / (core::f32::consts::TAU * 1000.0);
        let raw = (deg * 100.0) as i32;
        let rx_id = RX_ID_BASE + self.id() as u16;
        raw_write_i32_reg(can, self.id(), REG_ABS_POSITION, raw, rx_id).await.is_some()
    }
}

pub trait Jc2804Settings {
    fn id(&self) -> u8;

    /// flush_to_device: 从影子寄存器 `target_control_mode` 读目标值 → CAN 写
    /// 上层需先 `reg.target_control_mode.store(mode, Ordering::Relaxed)` 再调用
    async fn flush_control_mode_to_device(
        &self,
        can: &mut Tja1050,
        reg: &Jc2804SettingReg,
    ) -> bool {
        let mode = reg.target_control_mode.load(Ordering::Acquire);
        let rx_id = RX_ID_BASE + self.id() as u16;
        raw_write_u16_reg(can, self.id(), REG_CONTROL_MODE, mode as u16, rx_id).await.is_some()
    }
}
```

**规则**：
- 函数名用 `flush_*` 前缀
- **绝对不接受数值参数** — 从影子寄存器的 `target_*` 字段读取
- 上层调用 flush 之前**必须先** `store` 到对应的 `target_*` 字段
- 可返回 bool / Option<StatusReply> 表示执行结果
- 成功后**同步更新**相关影子字段（如 `flush_closed_loop_to_device` 成功后置 `closed_loop = true`）

**为什么不允许参数传递？**

影子寄存器是"上层意图"的唯一载体。如果 flush 方法既接受参数又从影子读，就会出现两个真相来源，行为取决于调用者用哪个路径。统一走影子寄存器路径后：
- 多个 task 可以独立设置目标值（各自 store 到各自的字段）
- Flush 只需知道"从哪读"，不需知道"读什么值"
- 影子寄存器成为断点/日志的自然锚点（读一下影子就知道系统当前意图）

### 4.5 复合方法模板（先写总线，再同步影子）

```rust
/// 进入闭环：CAN 写 0xA2 → 更新影子状态
async fn flush_closed_loop_to_device(
    &self,
    can: &mut Tja1050,
    reg: &Jc2804StatusReg,
) -> bool {
    let rx_id = RX_ID_BASE + self.id() as u16;
    let ok = raw_write_u16_reg(can, self.id(), REG_CLOSED_LOOP, 1, rx_id).await.is_some();
    if ok {
        reg.closed_loop.store(true, Ordering::Relaxed);  // 成功后同步影子
    } else {
        defmt::warn!("id={} flush_closed_loop_to_device timeout", self.id());
    }
    ok
}
```

**规则**：
- 总线操作成功 → 同步更新影子寄存器
- 总线操作失败 → 不修改影子寄存器
- 返回值表示操作是否成功

### 4.6 低层 raw_* 函数规范

```rust
// ── 低层：只发 CAN 帧，不做业务逻辑 ──

/// 读 u16 寄存器（返回协议原始值，不做单位转换）
async fn raw_read_u16_reg(can: &mut Tja1050, dev_id: u8, reg: u16, rx_id: u16) -> Option<u16>;

/// 写 u16 寄存器 → 等待 StatusReply
async fn raw_write_u16_reg(can: &mut Tja1050, dev_id: u8, reg: u16, value: u16, rx_id: u16) -> Option<StatusReply>;
```

**规则**：
- `raw_` 前缀表示原始协议操作
- 不做单位转换，不做业务语义
- 返回 `Option`，失败为 `None`
- 内部使用 `embassy_time::with_timeout` 防死等
- 超时时间统一用常量（如 `SCAN_TIMEOUT_MS: u64 = 10`）

### 4.7 通信接口封装规范

```rust
/// TJA1050 CAN 收发器 — 使用 Deref 模式透明暴露底层 HAL
pub struct Tja1050 {
    can: Can<'static>,
}

impl Deref for Tja1050 {
    type Target = Can<'static>;
    fn deref(&self) -> &Self::Target { &self.can }
}

impl DerefMut for Tja1050 {
    fn deref_mut(&mut self) -> &mut Self::Target { &mut self.can }
}
```

**规则**：
- 通信接口用 **Newtype + Deref** 模式封装
- 屏蔽硬件细节（中断绑定、引脚配置、回环自检）
- 对外透明暴露底层 HAL 的全部 API

---

## 五、Client / Task 层规范（software/tasks.rs）

### 5.1 只读写影子寄存器

```rust
#[embassy_executor::task]
pub async fn cdc_send_task(mut cdc: WchLinkCdc) {
    loop {
        // ✅ 正确：直接从影子寄存器读
        let v = register::JC2804_D1.voltage.load(Ordering::Relaxed);
        let p = register::JC2804_D1.position.load(Ordering::Relaxed);
        let err = status::JC2804_S1.error_kinds.load(Ordering::Relaxed);

        // ❌ 错误：绝对不能直接调 CAN 读写
        // raw_read_u16_reg(can, 1, REG_VOLTAGE, rx).await;  // 禁止！

        cdc.write_buf().await;
        Timer::after_millis(100).await;
    }
}
```

### 5.2 Sync 任务模板

```rust
/// telemetry_loop: 定期从 CAN 总线读取 → 更新影子寄存器
#[embassy_executor::task]
pub async fn telemetry_loop(can: &'static SharedCan, d1: &'static Option<Jc2804>, d2: &'static Option<Jc2804>) {
    loop {
        {
            let mut can = can.lock().await;           // 获取总线锁
            let can = can.as_mut().unwrap();

            // 逐个 sync 影子寄存器
            Jc2804Register::sync_voltage_from_device(d1, can, &register::JC2804_D1).await;
            Jc2804Register::sync_current_from_device(d1, can, &register::JC2804_D1).await;
            Jc2804Register::sync_speed_from_device(d1, can, &register::JC2804_D1).await;
            Jc2804Register::sync_position_from_device(d1, can, &register::JC2804_D1).await;
            // ... 更多 sync 调用

            Jc2804Status::sync_error_from_device(d1, can, &status::JC2804_S1).await;
            Jc2804Settings::sync_control_mode_from_device(d1, can, &setting::JC2804_SET1).await;
        }
        Timer::after_millis(100).await;              // 同步周期
    }
}
```

**规则**：
- 取锁 → 批量 sync → 释放锁 → 延时
- 锁的持有时间尽量短（只做 I/O，不做计算）
- 延时移到锁释放之后

### 5.3 Flush / 命令任务模板

```rust
/// sweep_loop: 从影子寄存器读目标值 → CAN 写
#[embassy_executor::task]
pub async fn sweep_loop(can: &'static SharedCan, d1: &'static Option<Jc2804>, d2: &'static Option<Jc2804>) {
    loop {
        for &(a1, a2) in &path {
            {
                let mut can = can.lock().await;
                let can = can.as_mut().unwrap();
                // 从业务逻辑拿到目标值 → 通过 Proxy 发 CAN 命令
                Jc2804Operator::flush_position_to_device(d1, can, a1).await;
                Jc2804Operator::flush_position_to_device(d2, can, a2).await;
            }
            Timer::after_millis(1000).await;
        }
    }
}
```

### 5.4 共享总线保护

```rust
/// 多个 task 共享同一物理总线时，使用 Mutex 保护
pub type SharedCan = Mutex<CriticalSectionRawMutex, Option<Tja1050>>;
```

**规则**：
- 所有需要访问总线的 task 共享同一个 `&'static SharedCan`
- 进入临界区前 `can.lock().await`
- 锁内只做 I/O，不做长延时
- 跨 task 的延时放到锁外

---

## 六、命名规范速查表

### 6.1 函数命名

| 前缀 | 含义 | 参数 | 返回值 | 示例 |
|------|------|------|--------|------|
| `sync_*_from_device` | 读总线 → 写影子 | `(&self, can, reg)` | 无 | `sync_voltage_from_device` |
| `flush_*_to_device` | 读影子 → 写总线 | `(&self, can, reg)` | bool/Option | `flush_position_to_device`, `flush_control_mode_to_device` |
| `cmd_*` | 设备命令（固定值） | `(&self, can)` | bool | `cmd_reboot`, `cmd_erase`, `cmd_save` |
| `raw_read_*` | 低层: 发读帧 → 返回原始值 | `(can, dev_id, reg, rx_id)` | Option<T> | `raw_read_u16_reg` |
| `raw_write_*` | 低层: 发写帧 → 返回状态回复 | `(can, dev_id, reg, val, rx_id)` | Option<StatusReply> | `raw_write_u16_reg` |

### 6.2 模块/结构体命名

| 命名 | 含义 | 示例 |
|------|------|------|
| `XxxTelemetryReg` | 遥测影子寄存器 | `Jc2804TelemetryReg` |
| `XxxStatusReg` | 状态影子寄存器 | `Jc2804StatusReg` |
| `XxxSettingReg` | 设置影子寄存器 | `Jc2804SettingReg` |
| `Xxx` | Proxy struct | `Jc2804` |
| `XXX_D1` / `XXX_S1` / `XXX_SET1` | 设备 #1 的影子实例 | `JC2804_D1`, `JC2804_S1`, `JC2804_SET1` |

### 6.3 Trait 命名

| 后缀 | 职责 | 示例 |
|------|------|------|
| `*Register` | sync_from_device 读操作 | `Jc2804Register` |
| `*Status` | 状态读写 | `Jc2804Status` |
| `*Settings` | 配置读写 | `Jc2804Settings` |
| `*Operator` | 运动/动作命令 | `Jc2804Operator` |

---

## 七、完整代码模板（新增外设时参照）

### 步骤 1：创建影子寄存器

```rust
// src/proxy/foo.rs

use core::sync::atomic::{AtomicU16, AtomicI32, AtomicBool, Ordering};

/// Foo 设备影子寄存器
pub struct FooReg {
    /// 温度 (0.1°C) — sync_from_device
    pub temperature: AtomicU16,
    /// 当前角度 (mrad) — sync_from_device
    pub angle: AtomicI32,
    /// 目标角度 (mrad) — flush_to_device
    pub target_angle: AtomicI32,
    /// 是否使能 — sync_from_device
    pub enabled: AtomicBool,
}

impl FooReg {
    pub const fn new() -> Self {
        Self {
            temperature: AtomicU16::new(0),
            angle: AtomicI32::new(0),
            target_angle: AtomicI32::new(0),
            enabled: AtomicBool::new(false),
        }
    }
}

/// 全局实例（有几个设备就建几个）
pub static FOO_D1: FooReg = FooReg::new();
```

### 步骤 2：创建 Proxy struct

```rust
// src/driver/board/foo_device.rs

/// Foo 设备代理（只存寻址信息）
pub struct FooDevice {
    pub bus_addr: u8,
}

impl FooDevice {
    pub fn new(addr: u8) -> Self { Self { bus_addr: addr } }
}
```

### 步骤 3：实现 Sync trait（读总线 → 写影子）

```rust
pub trait FooRegister {
    fn addr(&self) -> u8;

    /// sync_from_device: 读总线温度 → FooReg.temperature
    async fn sync_temperature_from_device(&self, bus: &mut BusType, reg: &FooReg) {
        if let Some(raw) = raw_read_temperature(bus, self.addr()).await {
            reg.temperature.store(raw, Ordering::Relaxed);
        }
    }

    /// sync_from_device: 读总线角度 → FooReg.angle
    async fn sync_angle_from_device(&self, bus: &mut BusType, reg: &FooReg) {
        if let Some(raw) = raw_read_angle(bus, self.addr()).await {
            // 协议单位 → 影子单位转换
            let mrad = (raw as f32 * 0.01).to_radians() * 1000.0;
            reg.angle.store(mrad as i32, Ordering::Relaxed);
        }
    }
}

impl FooRegister for FooDevice {
    fn addr(&self) -> u8 { self.bus_addr }
}
```

### 步骤 4：实现 Flush trait（读影子/参数 → 写总线）

```rust
pub trait FooOperator {
    fn addr(&self) -> u8;

    /// flush_to_device: 设置目标角度
    /// - 参数来自业务逻辑层（如扫摆任务的目标坐标）
    async fn flush_target_angle_to_device(&self, bus: &mut BusType, degrees: f32) -> bool {
        let raw = (degrees * 100.0) as i32;
        raw_write_angle(bus, self.addr(), raw).await.is_some()
    }
}

impl FooOperator for FooDevice {
    fn addr(&self) -> u8 { self.bus_addr }
}
```

### 步骤 5：在 Client task 中使用

```rust
#[embassy_executor::task]
pub async fn foo_sync_task(bus: &'static SharedBus, dev: &'static Option<FooDevice>) {
    let dev = dev.as_ref().unwrap();
    loop {
        {
            let mut bus = bus.lock().await;
            let bus = bus.as_mut().unwrap();
            FooRegister::update_temperature(dev, bus, &proxy::foo::FOO_D1).await;
            FooRegister::update_angle(dev, bus, &proxy::foo::FOO_D1).await;
        }
        Timer::after_millis(50).await;
    }
}

#[embassy_executor::task]
pub async fn foo_control_task(bus: &'static SharedBus, dev: &'static Option<FooDevice>) {
    let dev = dev.as_ref().unwrap();
    loop {
        // ✅ 读影子寄存器做决策，零延迟
        let current_angle = proxy::foo::FOO_D1.angle.load(Ordering::Relaxed);

        // ✅ 先写影子寄存器（目标值），再 flush
        let target_mrad = (90.0_f32 * core::f32::consts::TAU / 360.0 * 1000.0) as i32;
        proxy::foo::FOO_D1.target_angle.store(target_mrad, Ordering::Relaxed);

        {
            let mut bus = bus.lock().await;
            let bus = bus.as_mut().unwrap();
            // flush 从影子读 target_angle，不接受参数
            FooOperator::set_target_angle(dev, bus, &proxy::foo::FOO_D1).await;
        }
        Timer::after_millis(500).await;
    }
}
```

---

## 八、检查清单（Agent 写代码时逐条对照）

### 影子寄存器

- [ ] 所有字段使用 `Atomic*` 类型（不允许普通字段）
- [ ] 每个字段注释标注了**物理单位**和**数据流向**
- [ ] 提供了 `const fn new()` 常量构造器
- [ ] 每个物理设备对应一个全局 `static` 实例
- [ ] `Ordering` 选择正确（绝大多数场景用 `Relaxed`）

### Proxy 层

- [ ] Proxy struct 只存寻址信息（地址/ID），不存状态
- [ ] 按语义领域拆分了 trait（Register / Status / Settings / Operator）
- [ ] `sync_*` 方法：读总线 → store 到影子寄存器 → 无返回值
- [ ] `flush_*` 方法：从影子寄存器的 `target_*` 字段读 → 写总线
- [ ] `flush_*` 方法不接受数值参数（上层调用前必须先 store 到 `target_*` 字段）
- [ ] 操作成功后同步更新影子寄存器
- [ ] 操作失败后不修改影子寄存器
- [ ] 低层 `raw_*` 函数纯粹做协议收发，不含业务逻辑
- [ ] 超时时间用命名常量，不硬编码数字

### Client 层

- [ ] Task 中不直接调用 `raw_*` 函数
- [ ] Task 读取状态时只从影子寄存器 load
- [ ] 共享总线用 Mutex 保护
- [ ] 锁持有时间最小化（只做 I/O）
- [ ] 延时放在锁释放之后

### 禁止事项

- ❌ 禁止在 driver/ 中直接修改 proxy/ 的全局状态（除了通过影子寄存器引用）
- ❌ 禁止在 software/ task 中直接调用 driver/ 的 raw_* 函数
- ❌ 禁止 Proxy struct 中存放 runtime 状态
- ❌ 禁止 sync 方法返回值（Client 自己从影子寄存器读）
- ❌ 禁止 flush 方法接受业务参数 — 必须从影子寄存器的 `target_*` 字段读取

---

## 九、项目当前实现对照（重构后）

| 规范要求 | 当前 JC2804 实现 | 状态 |
|---------|-----------------|------|
| Proxy struct 只存 id | `Jc2804 { pub id: u8 }` | ✅ |
| 影子寄存器用 Atomic | `AtomicU16/AtomicI32/AtomicU8/AtomicBool` | ✅ |
| 按语义拆分 trait | `Jc2804Register / Jc2804Status / Jc2804Settings / Jc2804Operator` | ✅ |
| sync 方法无返回值 | `sync_*` 全部返回 `()` | ✅ |
| flush 方法从影子读 | `flush_position_to_device` 从 `reg.target_position` 读 | ✅ |
| flush 方法从影子读 | `flush_control_mode_to_device` 从 `reg.target_control_mode` 读 | ✅ |
| 操作成功后同步影子 | `sync_closed_loop_from_device` → `reg.closed_loop.store(...)` | ✅ |
| 操作前先 store 到影子 | `sweep_loop` 先 `store(target_position)` 再调 `flush_position_to_device` | ✅ |
| 操作前先 store 到影子 | `init`/`sweep_loop` 先 `store(target_control_mode)` 再调 `flush_control_mode_to_device` | ✅ |
| Client 只读影子 | `cdc_send_task` 全部 load 原子变量 | ✅ |
| 共享总线 Mutex | `SharedCan = Mutex<..., Option<Tja1050>>` | ✅ |
| 锁外延时 | telemetry_loop 的 `Timer::after` 在锁外 | ✅ |
| 目标/实际字段分离 | `control_mode`(实际) vs `target_control_mode`(期望) | ✅ |

---

## 十、演进原则

1. **先有影子寄存器，再有 Proxy**：设计新外设时，先定义「上层需要知道什么状态」，再考虑「怎么从硬件读到这些状态」。
2. **影子寄存器是真相来源**：Client 永远相信影子寄存器。如果影子寄存器错了，是 Proxy 的 sync 逻辑有问题，不是 Client 的问题。
3. **一个物理设备 → 一套影子寄存器**：不要多个设备共享一套影子寄存器。
4. **不要为"方便"打破模式**：每一条 flush 都必须从影子读。如果觉得 store-then-flush 太啰嗦，封装一个辅助方法，但底层路径不能绕过。
5. **新增外设时，不要修改已有影子寄存器的结构**：给新外设建新文件、新结构体、新 static。
