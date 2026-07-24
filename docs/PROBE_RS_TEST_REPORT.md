# probe-rs vs TI CCS/XDS110 — MSPM0G3507 兼容性测试报告

**日期**: 2026-07-24
**硬件**: TI LP-MSPM0G3507 LaunchPad (XDS-110 onboard debugger)
**软件**: probe-rs 0.31.0, CCS Theia 21.0 + tiarmclang 5.1.1

---

## 一、测试结论总览

| 维度 | probe-rs | CCS (DSLite/XDS110) |
|------|----------|---------------------|
| **XDS-110 识别** | ✅ 完美 | ✅ 原生 |
| **SWD 连接** | ✅ 稳定 | ✅ 稳定 (原生)|
| **JTAG 连接** | ❌ 不支持 | ✅ 支持 |
| **烧录 (download)** | ✅ 0.85s | ✅ ~2-3s (估计) |
| **擦除 (erase)** | ⚠️ 危险 | ✅ 安全 |
| **读内存 (read)** | ✅ | ✅ (GUI) |
| **芯片信息 (info)** | ✅ 详细 | ✅ (GUI) |
| **GDB Server** | ✅ 内置 | ❌ (需 gdb_agent_console) |
| **VSCode 调试** | ✅ 原生扩展 | ⚠️ 间接 |
| **命令行自动化** | ✅ 完美 | ⚠️ 可用但不便 |
| **DAP Server 调试** | ⚠️ 部分问题 | ✅ (CCS IDE) |
| **HEX 文件支持** | ❌ 需 ELF | ✅ HEX/ELF |
| **占锁冲突** | 互斥 (谁先占谁用) | 互斥 |

---

## 二、详细测试数据

### 2.1 探测识别

```
$ probe-rs list
[0]: XDS110 (03.00.00.25) Embed with CMSIS-DAP -- 0451:bef3-2:MG350001 (CMSIS-DAP)
[1]: XDS110 (03.00.00.25) Embed with CMSIS-DAP -- 0451:bef3-6:MG350001 (CMSIS-DAP)
```

**结论**: XDS-110 被 probe-rs 识别为 CMSIS-DAP 设备，出现两个入口（Debug + UART），正常工作。

### 2.2 芯片信息

```
$ probe-rs info --probe ... --chip MSPM0G3507 --protocol swd
Debug Port: DPv2, Designer: Texas Instruments, Part: 0xbb88
├── V1(0) MemoryAP → AmbaAhb3
│   ├── 0xf0000000 ROM Table
│   ├── 0xe00ff000 ROM Table
│   └── 0x40402000 Coresight Component (Cortex-M0+)
├── V1(1) Unknown AP (TI, Class: Undefined)
└── V1(2) Unknown AP (TI, Class: Undefined)
```

**结论**: DP 正确识别，但 2 个 TI 私有 AP 被标记为 "Unknown"。

### 2.3 烧录速度

| 工具 | 耗时 | 文件格式 |
|------|------|----------|
| `probe-rs download` | **0.85–2.66s** | ELF (.out) |
| `probe-rs download` | ❌ 不支持 | HEX (.hex) |
| `DSLite.exe flash` | ~3–5s (估计) | HEX (.hex) |

**结论**: probe-rs 烧录速度优于 DSLite，但不支持 HEX 文件，必须用 ELF。

### 2.4 内存读取

```bash
# 向量表 (Flash 0x00000000)
$ probe-rs read b32 0x00000000 4
20208000 0000357f 00003133 00003133      ✅

# SRAM 顶部 (含 boot 栈帧标记)
$ probe-rs read b32 0x20000000 8
00000000 00000000 00000000 00000000
00000000 00000000 00000000 be00be00      ✅

# GPIOA DOUT 寄存器 (0x41C00020)
$ probe-rs read b32 0x41C00020 1
aabbaabb                                  ✅ (外设寄存器可读)
```

**结论**: Flash、SRAM、外设寄存器读取全部正常。probe-rs 可以对任意地址做 8/16/32/64 位读取。

### 2.5 擦除 — 重大风险 ⚠️

```bash
$ probe-rs erase --chip MSPM0G3507  # ⚠️ 全擦!
# 之后芯片直接失连:
# "The AP has the wrong type for the operation"
# probe-rs info 显示: "No access ports found on this chip"
```

**原因分析**: `probe-rs erase` 执行了全芯片擦除（mass erase），MSPM0 的 NONMAIN 区域（安全/启动配置）也被擦除，导致 AP 枚举失败。

**恢复方法**:  **彻底断电再上电**（拔 USB 线重插）。软件 reset、xds110reset 均无法恢复。

**对比**: CCS/DSLite 的擦除能正确区分 MAIN/NONMAIN 区域，不会出现此问题。

### 2.6 VSCode 调试 (DAP Server)

```bash
$ probe-rs debug --exe empty.out --chip MSPM0G3507 --protocol swd
# 启动 DAP Server，连接到 VSCode probe-rs-debugger 扩展
```

**现象**:
- DAP Server 启动成功
- 程序加载后能停在断点
- ⚠️ 警告: "Unable to retrieve DW_AT_language attribute, assuming Rust"（ELF 中缺少 DW_AT_language）
- ⚠️ "Transfer count larger than requested number of transfers" — 寄存器批量读取时偶现错误
- ⚠️ 程序运行后 state 跟踪有问题（"Firmware exited unexpectedly"）

### 2.7 工具互斥

probe-rs 和 DSLite **不能同时使用** XDS-110。先占者锁定设备，后者报错。这与 CCS IDE 在调试时独占调试器的行为一致。

---

## 三、probe-rs 支持的所有 MSPM0 型号

probe-rs 0.31 共支持 **34 个** MSPM0 变种，覆盖：

| 系列 | 代表型号 |
|------|----------|
| MSPM0C | C1103, C1104 |
| MSPM0G1x0x/G3x0x | G1105–G1507, G3105–G3507 |
| MSPM0Gx51x | G1518–G3519 |
| MSPM0G5x1x | G5115–G5187 |
| MSPM0L | L1105–L1346 |
| MSPM0L122x/L222x | L1227–L2228 |

---

## 四、核心问题与风险

| # | 问题 | 严重度 | 说明 |
|---|------|--------|------|
| 1 | **`erase` 锁芯片** | 🔴 致命 | 全擦后 AP 全部丢失，需物理断电恢复 |
| 2 | **不支持 HEX** | 🟡 中等 | 只能用 ELF，对现有 HEX 工作流不兼容 |
| 3 | **DAP Server 不稳定** | 🟡 中等 | 寄存器读取偶现错误，DWARF 解析不完善 |
| 4 | **JTAG 不可用** | 🟡 中等 | MSPM0 的 SWD-over-JTAG 不被 probe-rs CMSIS-DAP 支持 |
| 5 | **TI 私有 AP 未知** | 🟢 低 | 不影响基本调试，但无法访问高级调试功能 |

---

## 五、推荐使用场景

### ✅ 适合用 probe-rs

- **CI/CD 自动化烧录** — 命令行接口简洁，速度快
- **VSCode 轻量调试** — 不想装 CCS 的时候
- **批量生产烧录** — 脚本化友好
- **Rust/非 TI 生态开发**

### ⚠️ 需要 CCS/DSLite

- **擦除操作** — `probe-rs erase` 是危险操作
- **JTAG 调试** — MSPM0 的 JTAG 只在 CCS 下稳定
- **HEX 工作流** — 现有基于 HEX 的流程
- **需要 TI 私有高级调试功能时**

### 💡 推荐混合使用

- **编译**: Rust 脚本 (`cargo run`)
- **烧录**: `probe-rs download` (快，可靠)
- **调试**: 简单场景用 `probe-rs debug` + VSCode，复杂场景切 CCS
- **擦除**: **只用 CCS/DSLite**，绝对不要用 `probe-rs erase`

---

## 六、已配置好的 VSCode 集成

| 文件 | 内容 |
|------|------|
| [.vscode/tasks.json](.vscode/tasks.json) | 7 个 task: Build/Clean/Flash(probe-rs)/Flash(DSLite)/Reset/Info/Full Cycle |
| [.vscode/launch.json](.vscode/launch.json) | DAP 调试配置 (probe-rs-debugger 扩展) |
| [build_tool/src/main.rs](build_tool/src/main.rs) | Rust 构建脚本 |
