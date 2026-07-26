# 🐛 历险记: TI Drivers I2C 探针三次翻车

> **日期**: 2026-07-26  
> **平台**: MSPM0G3507 + TI Drivers I2C (mspm0_sdk_2.10.00.04)  
> **严重程度**: 🔴 高（I2C 总线完全无波形，或发一次就卡死）  
> **状态**: ✅ 已修复

---

## 背景

写了个 I2C bus scanner：启动时扫一遍 0x08–0x77，发现有设备就记录地址 + WHO_AM_I。

硬件很简单：I2C0, PA0=SDA, PA1=SCL, 400kHz。用 TI Drivers（`/ti/drivers/I2C`）而非 DriverLib（`/ti/driverlib/I2C`）。配置手工写在 `ti_drivers_i2c_config.c/h`，不通过 SysConfig。

上线后示波器夹 PA0/PA1 — 全程高电平，什么都没有。于是故事开始。

---

## 第一幕: "示波器啥也没有"

### 现象

I2C 总线 SDA/SCL 始终高电平。没有 START，没有地址，没有任何信号。

### 排查

看代码，probe 的实现是：

```c
// I2C_test.c (初版)
txn.writeCount = 0;
txn.readCount  = 0;
I2C_transfer(handle, &txn);
```

自然想到：把地址 NACK 当"设备不存在"，不发 read/write 数据。`writeCount=0 && readCount=0` 看起来像是"只发地址+START，测 ACK/NACK"的最小合法交易。

### 根因

翻 TI Drivers `I2C.c:122`:

```c
if ((!transaction->writeCount) && (!transaction->readCount)) {
    transaction->status = I2C_STATUS_INVALID_TRANS;
    return (transaction->status);  // ← 直接返回，不动硬件
}
```

**TI Drivers 认为 `writeCount=0 && readCount=0` 是非法交易**。它需要至少一次读或写才会真正操作 I2C 外设。

### 修复

读 1 个 dummy 字节 → `readCount=1`，driver 发出 START + 地址 + R/W 位。

---

## 第二幕: "第一次有波形,第二次卡死了"

### 现象

改完后烧进去，示波器上第一次 probe 有波形——START → 地址 → NACK → STOP，完美。但第二次 probe 就没了——示波器又回到全程高电平，程序卡在 `I2C_transfer()` 里不返回。

用 BKPT 打点确认：第一个 `I2C_transfer` 正常返回 false（设备不存在），第二个 `I2C_transfer` 进去后永远不出来。

### 排查

问题显然在两次调用之间状态没清干净。追 TI Drivers `I2CMSPM0.c` 的读路径：

```c
// I2CMSPM0.c:627 — primeReadBurst 的第一行
static void I2CMSPM0_primeReadBurst(...)
{
    if (object->isReadInProgress)
        return;                    // ← 直接返回，什么都不做
    else
        object->isReadInProgress = true;
    // ...
}
```

那 `isReadInProgress` 什么时候清回 false？只有一处：

```c
// I2CMSPM0.c:449 — 中断处理函数中
} else if (intStatus & DL_I2C_INTERRUPT_CONTROLLER_RX_DONE) {
    // ...
    object->isReadInProgress = false;  // ← 只有这里！
}
```

**完整卡死链路**：

```
第 1 次 probe (read 1 byte, addr 0x08)
 ├─ primeReadBurst → isReadInProgress = true
 ├─ 设备不存在 → ADDR_NACK → STOP
 ├─ 中断: NACK → status = I2C_STATUS_ADDR_NACK
 ├─ 中断: STOP → completeTransfer → post semaphore → I2C_transfer 返回 false
 └─ 但是… RX_DONE 没来 → isReadInProgress 永远 = true  💀

第 2 次 probe (read 1 byte, addr 0x09)
 ├─ primeReadBurst → if (isReadInProgress) return;  ← NOP！
 ├─ 没有硬件操作 → 没有中断 → semaphore 永远不会被 post
 └─ SemaphoreP_pend(transferComplete, WAIT_FOREVER) → 卡死 💀
```

### 根因

**TI Drivers `I2CMSPM0.c` 的 bug**: `isReadInProgress` 只在 `RX_DONE` 中断时清零。当地址 NACK 时（设备不存在、总线忙等），没有 RX 数据 → 没有 `RX_DONE` → 这个 flag 就"只升不降"了。任何后续的读交易都会被第一行的 `if (isReadInProgress) return;` 直接静默掉，像没被调用过一样。

说人话：**读交易的 NACK 路径忘了重置 `isReadInProgress`，导致后面的所有读都变成空操作。**

### 修复

**绕开**: probe 改用 write 1 byte dummy，走 `primeWriteBurst` 路径。

write 路径没有 `isReadInProgress` 这种状态 flag——它的状态变量 (`burstCount`/`burstStarted`/`writeBuf`/`writeCount`) 全在 `I2CSupport_primeTransfer`（[I2CMSPM0.c:764-768](C:\ti\mspm0_sdk_2_10_00_04\source\ti\drivers\i2c\I2CMSPM0.c#L764-L768)）里被重置：

```c
object->writeBuf     = transaction->writeBuf;
object->writeCount   = transaction->writeCount;
object->readBuf      = transaction->readBuf;
object->readCount    = transaction->readCount;
object->burstCount   = 0;
object->burstStarted = false;
```

每次 `I2C_transfer` → `I2CSupport_primeTransfer` → 这些值重新初始化 → `primeWriteBurst` 正确运行。

**直接修复**（如果要改 SDK 源码，这里不做了）：在 `I2CMSPM0_completeTransfer` 或 NACK 中断处理中加上 `object->isReadInProgress = false;` 即可。

---

## 对比: 三种 probe 方式的结果

| 方式 | I2C 总线行为 | 第一次 NACK 后 |
|------|-------------|---------------|
| `writeCount=0, readCount=0` | 不动硬件，直接返回 | 不变（一直不动） |
| `readCount=1` (纯读 probe) | START → NACK → STOP ✅ | 卡死在 semaphore 💀 |
| `writeCount=1` (纯写 probe) | START → NACK → STOP ✅ | 正常工作 ✅ |

---

## 经验教训

1. **SDK 的 bug 比你想象的更接近表面**  
   这两个 bug 都在 SDK 代码的前几行就暴露了，不需要深挖。探针式 I2C scanner 是个很常见的需求，但显然 TI 的测试没有覆盖"连续快速调 `I2C_transfer` 且地址 NACK"的场景。

2. **读路径比写路径复杂，容易出 bug**  
   `primeReadBurst` 有 `isReadInProgress` / `burstStarted` 两个 flag，而 `primeWriteBurst` 只有一个 `burstStarted`（而且每次都在 `primeTransfer` 中重置）。读路径多出来的状态机节点就是 bug 的温床。

3. **阻塞式 API 的"静默卡死"是所有 bug 里最难调的**  
   没有 hard fault，没有 assert，没有看门狗复位——就是某个线程不跑了。示波器确认总线不动是唯一线索。

4. **用 write probe 而不是 read probe 来做 I2C 扫描**  
   这对任何使用 MSPM0 SDK 2.10 的人都是有效建议。哪怕未来 TI 修了这个 bug，write probe 也比 read probe 少一个状态机分支，更安全。

---

## 关联文件

| 文件 | 角色 |
|------|------|
| `src/driver/chip/I2C_test.c` | 应用层 I2C scanner（修复在此） |
| `C:\ti\mspm0_sdk_2_10_00_04\source\ti\drivers\I2C.c` | `I2C_transfer`—第 122 行拒绝 `wc=0 && rc=0` |
| `C:\ti\mspm0_sdk_2_10_00_04\source\ti\drivers\i2c\I2CMSPM0.c` | `primeReadBurst:627`—`isReadInProgress` 只升不降（root cause） |
| `C:\ti\mspm0_sdk_2_10_00_04\source\ti\drivers\i2c\I2CMSPM0.c` | `hwiFxn:449`—唯一清零 `isReadInProgress` 的地方 |
| `docs/debug-2025-07-26-dma-uart-silent-failure.md` | 前一次"静默失败"的 bug 记录 |

---

> *"第一个 NACK 之后，`isReadInProgress` 成为 I2C 墓地永远的旗帜。"*  
> *— 示波器*
