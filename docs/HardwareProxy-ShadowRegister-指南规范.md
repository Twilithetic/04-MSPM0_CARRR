# Hardware Proxy + Shadow Register 鏋舵瀯鎸囧崡瑙勮寖

> 鐗堟湰锛歷1.0 | 閫傜敤鑼冨洿锛氭湰椤圭洰鎵€鏈夊祵鍏ュ紡澶栬椹卞姩寮€鍙?| 鏈€鍚庢洿鏂帮細2026-07-20

---

## 涓€銆佹ā寮忔€昏

### 1.1 妯″紡鍚嶇О

**Hardware Proxy Pattern with Shadow Registers锛堢‖浠朵唬鐞?+ 褰卞瓙瀵勫瓨鍣ㄦā寮忥級**

### 1.2 鏍稿績鎬濇兂

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹? Application / Tasks锛圕lient 灞傦級                      鈹?鈹? 鈫?鍙鍐欐湰鍦?Shadow Register锛屼笉纰颁换浣曟€荤嚎/澶栬       鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹? Hardware Proxy锛堜唬鐞嗗眰锛?                             鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?    鈹?鈹? 鈹? Shadow Registers锛堝奖瀛愬瘎瀛樺櫒锛?                鈹?    鈹?鈹? 鈹? - 鍘熷瓙绫诲瀷瀛楁锛屼腑鏂?task 瀹夊叏                鈹?    鈹?鈹? 鈹? - 鏄繙绔‖浠剁姸鎬佺殑"鏈湴鐪熺悊"                  鈹?    鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?    鈹?鈹? Sync/Flush 鎿嶄綔鎺ュ彛                               鈹?鈹? 鈹溾攢 sync_*():  浠庢€荤嚎璇?鈫?鏇存柊褰卞瓙瀵勫瓨鍣紙鏃犺繑鍥炲€硷級 鈹?鈹? 鈹斺攢 flush_*(): 浠庡奖瀛愬瘎瀛樺櫒璇?鈫?鍐欐€荤嚎锛堜笉鎺ュ彈鍙傛暟锛?鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹? Hardware锛堢湡瀹炵‖浠讹級                                 鈹?鈹? CAN / SPI / I2C / UART / GPIO ...                   鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

**涓€鍙ヨ瘽姒傛嫭**锛歅roxy 鏄锛孲hadow Register 鏄粦鏉库€斺€擟lient 鍙湅榛戞澘锛孭roxy 璐熻矗鎶婂澶栫殑鐪熷疄涓栫晫鍚屾鍒伴粦鏉夸笂銆?
### 1.3 涓轰粈涔堢敤杩欎釜妯″紡

| 闂 | 鏈ā寮忕殑瑙ｅ喅鏂规 |
|------|-----------------|
| 姣忔璇诲彇閮借绛夋€荤嚎鍥炲簲锛堝欢杩熼珮锛?| 褰卞瓙瀵勫瓨鍣ㄧ紦瀛樻渶鏂板€硷紝璇诲彇闆跺欢杩?|
| 澶氫釜 task 骞跺彂璇诲啓瀵勫瓨鍣?| 鍘熷瓙绫诲瀷澶╃劧绾跨▼瀹夊叏 |
| 涓婂眰浠ｇ爜鍜屾€荤嚎鍗忚绱ц€﹀悎 | Proxy 灏佽鍏ㄩ儴鍗忚锛孋lient 鏃犳劅鐭?|
| 鐘舵€佸垎鏁ｅ湪 driver 鍐呴儴 | 鎵€鏈夌姸鎬侀泦涓湪 software 妯″潡锛屽叏灞€鍙 |

### 1.4 鍜屽叾浠栨ā寮忕殑鍖哄垎

| 妯″紡 | Query锛堣锛?| Command锛堝啓锛?| 鍜屾湰妯″紡鐨勫尯鍒?|
|------|------------|--------------|---------------|
| **CQS / CQRS** | 杩斿洖鏁版嵁锛屾棤鍓綔鐢?| 鏈夊壇浣滅敤锛屼笉杩斿洖鍊?| CQS 鐨?Query 蹇呴』杩斿洖鍊?|
| **Observer** | 琚姩鎺ユ敹閫氱煡鏇存柊 | 涓嶆秹鍙?| Observer 涓嶅惈涓诲姩鍚屾 |
| **Shadow Register (绾‖浠?** | 纭欢鑷姩澶嶅埗 | 纭欢鑷姩澶嶅埗 | 绾‖浠跺奖瀛愬瘎瀛樺櫒鐢辫姱鐗囪嚜鍔ㄧ鐞?|
| **鈽呮湰妯″紡** | **鏃犺繑鍥炲€?*锛屽啓鍏ュ奖瀛愬瘎瀛樺櫒 | **涓嶆帴鍙楁暟鎹弬鏁?*锛屼粠褰卞瓙璇?| 涓诲姩鍚屾 + 涓诲姩鍒锋柊 |

---

## 浜屻€佹ā鍧楃粍缁囪鑼?
### 2.1 鏂囦欢缁撴瀯

```
src/
鈹溾攢鈹€ driver/                          # 浠ｇ悊灞傦紙涓庣‖浠堕€氫俊锛?鈹?  鈹斺攢鈹€ board/
鈹?      鈹溾攢鈹€ jc2804.rs                # Proxy 瀹炵幇锛欳AN 鍗忚銆丼ync/Flush 鎿嶄綔
鈹?      鈹斺攢鈹€ tja1050.rs               # 閫氫俊鎺ュ彛灏佽锛圕AN 鎬荤嚎锛?鈹溾攢鈹€ proxy/                           # 褰卞瓙瀵勫瓨鍣ㄥ眰锛堟湰鍦扮姸鎬佺紦瀛橈級
鈹?  鈹溾攢鈹€ register.rs                  # 閬ユ祴/鏁版嵁褰卞瓙瀵勫瓨鍣?鈹?  鈹溾攢鈹€ status.rs                    # 鐘舵€佸奖瀛愬瘎瀛樺櫒
鈹?  鈹斺攢鈹€ setting.rs                   # 璁剧疆/閰嶇疆褰卞瓙瀵勫瓨鍣?鈹溾攢鈹€ software/                        # Client 灞傦紙娑堣垂鑰咃級
鈹?  鈹斺攢鈹€ tasks.rs                     # 寮傛浠诲姟
鈹斺攢鈹€ main.rs                          # 鍚姩鍏ュ彛锛歅eripherals::init 鈫?spawn tasks
```

### 2.2 閾佸緥

> **`driver/` 缁濆涓?import `software/` 鐨勯€昏緫灞傦紝鍙?import `proxy/` 鐨勫奖瀛愬瘎瀛樺櫒 struct 绫诲瀷銆?*
>
> **`software/` 缁濆涓?import `driver/` 鐨勬€荤嚎鎿嶄綔鍑芥暟锛屽彧 import Proxy 鎻愪緵鐨?trait 鍜?`proxy/` 鐨勫奖瀛愬瘎瀛樺櫒銆?*

璺ㄥ眰鍙€氳繃 **trait + 褰卞瓙瀵勫瓨鍣ㄥ紩鐢?* 閫氫俊銆?
---

## 涓夈€佸奖瀛愬瘎瀛樺櫒瑙勮寖锛坧roxy/register.rs | status.rs | setting.rs锛?
### 3.1 绫诲瀷閫夋嫨

| 鏁版嵁绫诲瀷 | 浣跨敤鍘熷瓙绫诲瀷 | 璇存槑 |
|---------|------------|------|
| 甯冨皵鏍囧織 | `AtomicBool` | 濡?closed_loop銆乮dle銆乺eady |
| 灏忔暣鏁扮姸鎬?| `AtomicU8` | 濡?control_mode |
| 16浣嶆暟鍊?| `AtomicU16` | 濡傜數鍘?mV)銆佺數娴?mA)銆佹俯搴?0.1掳C) |
| 32浣嶆湁绗﹀彿鏁板€?| `AtomicI32` | 濡傞€熷害(mrad/s)銆佷綅缃?mrad) |
| 32浣嶆爣蹇椾綅鍥?| `AtomicU32` | 濡傞敊璇爜浣嶅浘 |
| 瀛楃涓?鏁扮粍 | `static mut` + 涓寸晫鍖?| 濡?CDC_MSG锛屼粎鍦ㄥ崟涓?task 鍐欏叆鏃跺彲鐢?unsafe |

### 3.2 缁撴瀯浣撳畾涔夋ā鏉?
```rust
/// JC2804 閬ユ祴褰卞瓙瀵勫瓨鍣紙鍘熷瓙璁块棶锛屼腑鏂?task 瀹夊叏锛?pub struct Jc2804TelemetryReg {
    /// 鐢垫簮鐢靛帇 (mV) 鈥?sync_from_device 鏇存柊
    pub voltage: AtomicU16,
    /// 瀹炴椂浣嶇疆 (mrad) 鈥?sync_from_device 鏇存柊
    pub position: AtomicI32,
    /// 鐩爣浣嶇疆 (mrad) 鈥?flush_to_device 璇诲彇
    pub target_position: AtomicI32,
}

impl Jc2804TelemetryReg {
    /// 甯搁噺鏋勯€犲櫒锛堜粠 0 鍒濆鍖栵級
    pub const fn new() -> Self {
        Self {
            voltage: AtomicU16::new(0),
            position: AtomicI32::new(0),
            target_position: AtomicI32::new(0),
        }
    }
}
```

### 3.3 鍏ㄥ眬闈欐€佸疄渚?
```rust
/// 姣忎釜鐗╃悊璁惧 鈫?涓€涓叏灞€闈欐€佸奖瀛愬瘎瀛樺櫒瀹炰緥
pub static JC2804_D1: Jc2804TelemetryReg = Jc2804TelemetryReg::new();
pub static JC2804_D2: Jc2804TelemetryReg = Jc2804TelemetryReg::new();
```

### 3.4 瀛楁娉ㄩ噴瑙勫垯

姣忎釜瀛楁蹇呴』鏍囨敞锛?1. **鐗╃悊鍗曚綅**锛堝 mV銆乵A銆乵rad/s锛?2. **鏁版嵁娴佹柟鍚?*锛坄sync_from_device` 鎴?`flush_to_device`锛?3. **绮惧害淇℃伅**锛堝 `raw*0.1掳C`锛?
### 3.5 Ordering 閫夋嫨

| 鍦烘櫙 | 鎺ㄨ崘 Ordering |
|------|--------------|
| 閬ユ祴鏇存柊锛堝啓锛?| `Relaxed` 鈥?鍙叧蹇冩渶缁堜竴鑷存€?|
| 鏍囧織浣嶈缃紙濡?can_ready锛?| `Release` / `Acquire` 鈥?鏈?happens-before 鍏崇郴 |
| 閫氱敤璇诲啓 | `Relaxed` 鈥?鏈」鐩潎涓洪潪鍏抽敭鍚屾璺緞 |

---

## 鍥涖€丳roxy 灞傝鑼冿紙driver/board/jc2804.rs锛?
### 4.1 Proxy struct 鏈€灏忓寲鍘熷垯

```rust
/// Proxy struct 鍙瓨瀵诲潃淇℃伅锛屼笉瀛樼姸鎬?pub struct Jc2804 {
    pub id: u8,           // CAN 璁惧鍦板潃
    // 涓嶆斁鐘舵€侊紒涓嶆斁缂撳瓨锛?}
```

### 4.2 Trait 鍒掑垎鍘熷垯

鎸?**璇箟棰嗗煙** 鎷嗗垎 trait锛屾瘡涓?trait 鍙仛涓€浠朵簨锛?
| Trait | 鑱岃矗 | 绀轰緥鏂规硶 |
|-------|------|---------|
| `XxxRegister` | sync_from_device锛氳鎬荤嚎 鈫?鍐欏奖瀛愬瘎瀛樺櫒 | `sync_voltage_from_device()`, `sync_position_from_device()` |
| `XxxStatus` | 鐘舵€佸悓姝?+ 璁惧鍛戒护 | `sync_error_from_device()`, `flush_closed_loop_to_device()` |
| `XxxSettings` | 閰嶇疆鍚屾 + 閰嶇疆鍒锋柊 | `flush_control_mode_to_device()`, `sync_control_mode_from_device()` |
| `XxxOperator` | 鍔ㄤ綔鍛戒护锛坒lush_to_device + cmd锛?| `flush_position_to_device()`, `cmd_reboot()` |

### 4.3 Sync 鏂规硶妯℃澘锛堣鎬荤嚎 鈫?鍐欏奖瀛愬瘎瀛樺櫒锛?
```rust
pub trait Jc2804Register {
    fn id(&self) -> u8;

    /// sync_from_device: 璇?CAN 鎬荤嚎鐢垫簮鐢靛帇 鈫?鍐欏叆褰卞瓙瀵勫瓨鍣?    /// - 涓嶈繑鍥炲€硷紙Client 鐩存帴浠庡奖瀛愬瘎瀛樺櫒璇伙級
    /// - 鎬荤嚎璇诲彇澶辫触鏃堕潤榛樿烦杩囷紙淇濈暀鏃у€硷級
    async fn sync_voltage_from_device(&self, can: &mut Tja1050, reg: &Jc2804TelemetryReg) {
        let rx = RX_ID_BASE + self.id() as u16;
        if let Some(v) = raw_read_u16_reg(can, self.id(), REG_VOLTAGE, rx).await {
            // 鍗忚鍗曚綅 鈫?褰卞瓙瀵勫瓨鍣ㄥ崟浣嶏紙*0.1V 鈫?mV锛?            reg.voltage.store(v.saturating_mul(100), Ordering::Relaxed);
        }
    }
}
```

**瑙勫垯**锛?- 鍑芥暟鍚嶇敤 `sync_*` 鍓嶇紑
- 涓嶈繑鍥炲€?- 褰卞瓙瀵勫瓨鍣ㄥ紩鐢ㄤ綔涓哄弬鏁颁紶鍏?- 鍗曚綅杞崲鍦?`store` 涔嬪墠瀹屾垚
- 澶辫触鏃?*涓嶄慨鏀瑰奖瀛愬瘎瀛樺櫒**锛堜繚鐣欐棫鍊硷級

### 4.4 Flush 鏂规硶妯℃澘锛堣褰卞瓙瀵勫瓨鍣?鈫?鍐欐€荤嚎锛?
```rust
pub trait Jc2804Operator {
    fn id(&self) -> u8;

    /// flush_to_device: 浠庡奖瀛愬瘎瀛樺櫒 `target_position` 璇荤洰鏍囧€?鈫?CAN 鍐?    /// - 涓嶆帴鍙楁暟鍊煎弬鏁帮紙鐢变笂灞備笟鍔￠€昏緫鍏?store 鍒板奖瀛愬瘎瀛樺櫒锛?    /// - 鍙繑鍥?bool 琛ㄧず鎵ц缁撴灉
    async fn flush_position_to_device(
        &self,
        can: &mut Tja1050,
        reg: &Jc2804TelemetryReg,
    ) -> bool {
        let target_mrad = reg.target_position.load(Ordering::Acquire);
        // 褰卞瓙鍗曚綅 mrad 鈫?鍗忚鍗曚綅 *0.01掳
        let deg = target_mrad as f32 * 360.0 / (core::f32::consts::TAU * 1000.0);
        let raw = (deg * 100.0) as i32;
        let rx_id = RX_ID_BASE + self.id() as u16;
        raw_write_i32_reg(can, self.id(), REG_ABS_POSITION, raw, rx_id).await.is_some()
    }
}

pub trait Jc2804Settings {
    fn id(&self) -> u8;

    /// flush_to_device: 浠庡奖瀛愬瘎瀛樺櫒 `target_control_mode` 璇荤洰鏍囧€?鈫?CAN 鍐?    /// 涓婂眰闇€鍏?`reg.target_control_mode.store(mode, Ordering::Relaxed)` 鍐嶈皟鐢?    async fn flush_control_mode_to_device(
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

**瑙勫垯**锛?- 鍑芥暟鍚嶇敤 `flush_*` 鍓嶇紑
- **缁濆涓嶆帴鍙楁暟鍊煎弬鏁?* 鈥?浠庡奖瀛愬瘎瀛樺櫒鐨?`target_*` 瀛楁璇诲彇
- 涓婂眰璋冪敤 flush 涔嬪墠**蹇呴』鍏?* `store` 鍒板搴旂殑 `target_*` 瀛楁
- 鍙繑鍥?bool / Option<StatusReply> 琛ㄧず鎵ц缁撴灉
- 鎴愬姛鍚?*鍚屾鏇存柊**鐩稿叧褰卞瓙瀛楁锛堝 `flush_closed_loop_to_device` 鎴愬姛鍚庣疆 `closed_loop = true`锛?
**涓轰粈涔堜笉鍏佽鍙傛暟浼犻€掞紵**

褰卞瓙瀵勫瓨鍣ㄦ槸"涓婂眰鎰忓浘"鐨勫敮涓€杞戒綋銆傚鏋?flush 鏂规硶鏃㈡帴鍙楀弬鏁板張浠庡奖瀛愯锛屽氨浼氬嚭鐜颁袱涓湡鐩告潵婧愶紝琛屼负鍙栧喅浜庤皟鐢ㄨ€呯敤鍝釜璺緞銆傜粺涓€璧板奖瀛愬瘎瀛樺櫒璺緞鍚庯細
- 澶氫釜 task 鍙互鐙珛璁剧疆鐩爣鍊硷紙鍚勮嚜 store 鍒板悇鑷殑瀛楁锛?- Flush 鍙渶鐭ラ亾"浠庡摢璇?锛屼笉闇€鐭ラ亾"璇讳粈涔堝€?
- 褰卞瓙瀵勫瓨鍣ㄦ垚涓烘柇鐐?鏃ュ織鐨勮嚜鐒堕敋鐐癸紙璇讳竴涓嬪奖瀛愬氨鐭ラ亾绯荤粺褰撳墠鎰忓浘锛?
### 4.5 澶嶅悎鏂规硶妯℃澘锛堝厛鍐欐€荤嚎锛屽啀鍚屾褰卞瓙锛?
```rust
/// 杩涘叆闂幆锛欳AN 鍐?0xA2 鈫?鏇存柊褰卞瓙鐘舵€?async fn flush_closed_loop_to_device(
    &self,
    can: &mut Tja1050,
    reg: &Jc2804StatusReg,
) -> bool {
    let rx_id = RX_ID_BASE + self.id() as u16;
    let ok = raw_write_u16_reg(can, self.id(), REG_CLOSED_LOOP, 1, rx_id).await.is_some();
    if ok {
        reg.closed_loop.store(true, Ordering::Relaxed);  // 鎴愬姛鍚庡悓姝ュ奖瀛?    } else {
        defmt::warn!("id={} flush_closed_loop_to_device timeout", self.id());
    }
    ok
}
```

**瑙勫垯**锛?- 鎬荤嚎鎿嶄綔鎴愬姛 鈫?鍚屾鏇存柊褰卞瓙瀵勫瓨鍣?- 鎬荤嚎鎿嶄綔澶辫触 鈫?涓嶄慨鏀瑰奖瀛愬瘎瀛樺櫒
- 杩斿洖鍊艰〃绀烘搷浣滄槸鍚︽垚鍔?
### 4.6 浣庡眰 raw_* 鍑芥暟瑙勮寖

```rust
// 鈹€鈹€ 浣庡眰锛氬彧鍙?CAN 甯э紝涓嶅仛涓氬姟閫昏緫 鈹€鈹€

/// 璇?u16 瀵勫瓨鍣紙杩斿洖鍗忚鍘熷鍊硷紝涓嶅仛鍗曚綅杞崲锛?async fn raw_read_u16_reg(can: &mut Tja1050, dev_id: u8, reg: u16, rx_id: u16) -> Option<u16>;

/// 鍐?u16 瀵勫瓨鍣?鈫?绛夊緟 StatusReply
async fn raw_write_u16_reg(can: &mut Tja1050, dev_id: u8, reg: u16, value: u16, rx_id: u16) -> Option<StatusReply>;
```

**瑙勫垯**锛?- `raw_` 鍓嶇紑琛ㄧず鍘熷鍗忚鎿嶄綔
- 涓嶅仛鍗曚綅杞崲锛屼笉鍋氫笟鍔¤涔?- 杩斿洖 `Option`锛屽け璐ヤ负 `None`
- 鍐呴儴浣跨敤 `embassy_time::with_timeout` 闃叉绛?- 瓒呮椂鏃堕棿缁熶竴鐢ㄥ父閲忥紙濡?`SCAN_TIMEOUT_MS: u64 = 10`锛?
### 4.7 閫氫俊鎺ュ彛灏佽瑙勮寖

```rust
/// TJA1050 CAN 鏀跺彂鍣?鈥?浣跨敤 Deref 妯″紡閫忔槑鏆撮湶搴曞眰 HAL
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

**瑙勫垯**锛?- 閫氫俊鎺ュ彛鐢?**Newtype + Deref** 妯″紡灏佽
- 灞忚斀纭欢缁嗚妭锛堜腑鏂粦瀹氥€佸紩鑴氶厤缃€佸洖鐜嚜妫€锛?- 瀵瑰閫忔槑鏆撮湶搴曞眰 HAL 鐨勫叏閮?API

---

## 浜斻€丆lient / Task 灞傝鑼冿紙software/tasks.rs锛?
### 5.1 鍙鍐欏奖瀛愬瘎瀛樺櫒

```rust
#[embassy_executor::task]
pub async fn cdc_send_task(mut cdc: WchLinkCdc) {
    loop {
        // 鉁?姝ｇ‘锛氱洿鎺ヤ粠褰卞瓙瀵勫瓨鍣ㄨ
        let v = register::JC2804_D1.voltage.load(Ordering::Relaxed);
        let p = register::JC2804_D1.position.load(Ordering::Relaxed);
        let err = status::JC2804_S1.error_kinds.load(Ordering::Relaxed);

        // 鉂?閿欒锛氱粷瀵逛笉鑳界洿鎺ヨ皟 CAN 璇诲啓
        // raw_read_u16_reg(can, 1, REG_VOLTAGE, rx).await;  // 绂佹锛?
        cdc.write_buf().await;
        Timer::after_millis(100).await;
    }
}
```

### 5.2 Sync 浠诲姟妯℃澘

```rust
/// telemetry_loop: 瀹氭湡浠?CAN 鎬荤嚎璇诲彇 鈫?鏇存柊褰卞瓙瀵勫瓨鍣?#[embassy_executor::task]
pub async fn telemetry_loop(can: &'static SharedCan, d1: &'static Option<Jc2804>, d2: &'static Option<Jc2804>) {
    loop {
        {
            let mut can = can.lock().await;           // 鑾峰彇鎬荤嚎閿?            let can = can.as_mut().unwrap();

            // 閫愪釜 sync 褰卞瓙瀵勫瓨鍣?            Jc2804Register::sync_voltage_from_device(d1, can, &register::JC2804_D1).await;
            Jc2804Register::sync_current_from_device(d1, can, &register::JC2804_D1).await;
            Jc2804Register::sync_speed_from_device(d1, can, &register::JC2804_D1).await;
            Jc2804Register::sync_position_from_device(d1, can, &register::JC2804_D1).await;
            // ... 鏇村 sync 璋冪敤

            Jc2804Status::sync_error_from_device(d1, can, &status::JC2804_S1).await;
            Jc2804Settings::sync_control_mode_from_device(d1, can, &setting::JC2804_SET1).await;
        }
        Timer::after_millis(100).await;              // 鍚屾鍛ㄦ湡
    }
}
```

**瑙勫垯**锛?- 鍙栭攣 鈫?鎵归噺 sync 鈫?閲婃斁閿?鈫?寤舵椂
- 閿佺殑鎸佹湁鏃堕棿灏介噺鐭紙鍙仛 I/O锛屼笉鍋氳绠楋級
- 寤舵椂绉诲埌閿侀噴鏀句箣鍚?
### 5.3 Flush / 鍛戒护浠诲姟妯℃澘

```rust
/// sweep_loop: 浠庡奖瀛愬瘎瀛樺櫒璇荤洰鏍囧€?鈫?CAN 鍐?#[embassy_executor::task]
pub async fn sweep_loop(can: &'static SharedCan, d1: &'static Option<Jc2804>, d2: &'static Option<Jc2804>) {
    loop {
        for &(a1, a2) in &path {
            {
                let mut can = can.lock().await;
                let can = can.as_mut().unwrap();
                // 浠庝笟鍔￠€昏緫鎷垮埌鐩爣鍊?鈫?閫氳繃 Proxy 鍙?CAN 鍛戒护
                Jc2804Operator::flush_position_to_device(d1, can, a1).await;
                Jc2804Operator::flush_position_to_device(d2, can, a2).await;
            }
            Timer::after_millis(1000).await;
        }
    }
}
```

### 5.4 鍏变韩鎬荤嚎淇濇姢

```rust
/// 澶氫釜 task 鍏变韩鍚屼竴鐗╃悊鎬荤嚎鏃讹紝浣跨敤 Mutex 淇濇姢
pub type SharedCan = Mutex<CriticalSectionRawMutex, Option<Tja1050>>;
```

**瑙勫垯**锛?- 鎵€鏈夐渶瑕佽闂€荤嚎鐨?task 鍏变韩鍚屼竴涓?`&'static SharedCan`
- 杩涘叆涓寸晫鍖哄墠 `can.lock().await`
- 閿佸唴鍙仛 I/O锛屼笉鍋氶暱寤舵椂
- 璺?task 鐨勫欢鏃舵斁鍒伴攣澶?
---

## 鍏€佸懡鍚嶈鑼冮€熸煡琛?
### 6.1 鍑芥暟鍛藉悕

| 鍓嶇紑 | 鍚箟 | 鍙傛暟 | 杩斿洖鍊?| 绀轰緥 |
|------|------|------|--------|------|
| `sync_*_from_device` | 璇绘€荤嚎 鈫?鍐欏奖瀛?| `(&self, can, reg)` | 鏃?| `sync_voltage_from_device` |
| `flush_*_to_device` | 璇诲奖瀛?鈫?鍐欐€荤嚎 | `(&self, can, reg)` | bool/Option | `flush_position_to_device`, `flush_control_mode_to_device` |
| `cmd_*` | 璁惧鍛戒护锛堝浐瀹氬€硷級 | `(&self, can)` | bool | `cmd_reboot`, `cmd_erase`, `cmd_save` |
| `raw_read_*` | 浣庡眰: 鍙戣甯?鈫?杩斿洖鍘熷鍊?| `(can, dev_id, reg, rx_id)` | Option<T> | `raw_read_u16_reg` |
| `raw_write_*` | 浣庡眰: 鍙戝啓甯?鈫?杩斿洖鐘舵€佸洖澶?| `(can, dev_id, reg, val, rx_id)` | Option<StatusReply> | `raw_write_u16_reg` |

### 6.2 妯″潡/缁撴瀯浣撳懡鍚?
| 鍛藉悕 | 鍚箟 | 绀轰緥 |
|------|------|------|
| `XxxTelemetryReg` | 閬ユ祴褰卞瓙瀵勫瓨鍣?| `Jc2804TelemetryReg` |
| `XxxStatusReg` | 鐘舵€佸奖瀛愬瘎瀛樺櫒 | `Jc2804StatusReg` |
| `XxxSettingReg` | 璁剧疆褰卞瓙瀵勫瓨鍣?| `Jc2804SettingReg` |
| `Xxx` | Proxy struct | `Jc2804` |
| `XXX_D1` / `XXX_S1` / `XXX_SET1` | 璁惧 #1 鐨勫奖瀛愬疄渚?| `JC2804_D1`, `JC2804_S1`, `JC2804_SET1` |

### 6.3 Trait 鍛藉悕

| 鍚庣紑 | 鑱岃矗 | 绀轰緥 |
|------|------|------|
| `*Register` | sync_from_device 璇绘搷浣?| `Jc2804Register` |
| `*Status` | 鐘舵€佽鍐?| `Jc2804Status` |
| `*Settings` | 閰嶇疆璇诲啓 | `Jc2804Settings` |
| `*Operator` | 杩愬姩/鍔ㄤ綔鍛戒护 | `Jc2804Operator` |

---

## 涓冦€佸畬鏁翠唬鐮佹ā鏉匡紙鏂板澶栬鏃跺弬鐓э級

### 姝ラ 1锛氬垱寤哄奖瀛愬瘎瀛樺櫒

```rust
// src/proxy/foo.rs

use core::sync::atomic::{AtomicU16, AtomicI32, AtomicBool, Ordering};

/// Foo 璁惧褰卞瓙瀵勫瓨鍣?pub struct FooReg {
    /// 娓╁害 (0.1掳C) 鈥?sync_from_device
    pub temperature: AtomicU16,
    /// 褰撳墠瑙掑害 (mrad) 鈥?sync_from_device
    pub angle: AtomicI32,
    /// 鐩爣瑙掑害 (mrad) 鈥?flush_to_device
    pub target_angle: AtomicI32,
    /// 鏄惁浣胯兘 鈥?sync_from_device
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

/// 鍏ㄥ眬瀹炰緥锛堟湁鍑犱釜璁惧灏卞缓鍑犱釜锛?pub static FOO_D1: FooReg = FooReg::new();
```

### 姝ラ 2锛氬垱寤?Proxy struct

```rust
// src/driver/board/foo_device.rs

/// Foo 璁惧浠ｇ悊锛堝彧瀛樺鍧€淇℃伅锛?pub struct FooDevice {
    pub bus_addr: u8,
}

impl FooDevice {
    pub fn new(addr: u8) -> Self { Self { bus_addr: addr } }
}
```

### 姝ラ 3锛氬疄鐜?Sync trait锛堣鎬荤嚎 鈫?鍐欏奖瀛愶級

```rust
pub trait FooRegister {
    fn addr(&self) -> u8;

    /// sync_from_device: 璇绘€荤嚎娓╁害 鈫?FooReg.temperature
    async fn sync_temperature_from_device(&self, bus: &mut BusType, reg: &FooReg) {
        if let Some(raw) = raw_read_temperature(bus, self.addr()).await {
            reg.temperature.store(raw, Ordering::Relaxed);
        }
    }

    /// sync_from_device: 璇绘€荤嚎瑙掑害 鈫?FooReg.angle
    async fn sync_angle_from_device(&self, bus: &mut BusType, reg: &FooReg) {
        if let Some(raw) = raw_read_angle(bus, self.addr()).await {
            // 鍗忚鍗曚綅 鈫?褰卞瓙鍗曚綅杞崲
            let mrad = (raw as f32 * 0.01).to_radians() * 1000.0;
            reg.angle.store(mrad as i32, Ordering::Relaxed);
        }
    }
}

impl FooRegister for FooDevice {
    fn addr(&self) -> u8 { self.bus_addr }
}
```

### 姝ラ 4锛氬疄鐜?Flush trait锛堣褰卞瓙/鍙傛暟 鈫?鍐欐€荤嚎锛?
```rust
pub trait FooOperator {
    fn addr(&self) -> u8;

    /// flush_to_device: 璁剧疆鐩爣瑙掑害
    /// - 鍙傛暟鏉ヨ嚜涓氬姟閫昏緫灞傦紙濡傛壂鎽嗕换鍔＄殑鐩爣鍧愭爣锛?    async fn flush_target_angle_to_device(&self, bus: &mut BusType, degrees: f32) -> bool {
        let raw = (degrees * 100.0) as i32;
        raw_write_angle(bus, self.addr(), raw).await.is_some()
    }
}

impl FooOperator for FooDevice {
    fn addr(&self) -> u8 { self.bus_addr }
}
```

### 姝ラ 5锛氬湪 Client task 涓娇鐢?
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
        // 鉁?璇诲奖瀛愬瘎瀛樺櫒鍋氬喅绛栵紝闆跺欢杩?        let current_angle = proxy::foo::FOO_D1.angle.load(Ordering::Relaxed);

        // 鉁?鍏堝啓褰卞瓙瀵勫瓨鍣紙鐩爣鍊硷級锛屽啀 flush
        let target_mrad = (90.0_f32 * core::f32::consts::TAU / 360.0 * 1000.0) as i32;
        proxy::foo::FOO_D1.target_angle.store(target_mrad, Ordering::Relaxed);

        {
            let mut bus = bus.lock().await;
            let bus = bus.as_mut().unwrap();
            // flush 浠庡奖瀛愯 target_angle锛屼笉鎺ュ彈鍙傛暟
            FooOperator::set_target_angle(dev, bus, &proxy::foo::FOO_D1).await;
        }
        Timer::after_millis(500).await;
    }
}
```

---

## 鍏€佹鏌ユ竻鍗曪紙Agent 鍐欎唬鐮佹椂閫愭潯瀵圭収锛?
### 褰卞瓙瀵勫瓨鍣?
- [ ] 鎵€鏈夊瓧娈典娇鐢?`Atomic*` 绫诲瀷锛堜笉鍏佽鏅€氬瓧娈碉級
- [ ] 姣忎釜瀛楁娉ㄩ噴鏍囨敞浜?*鐗╃悊鍗曚綅**鍜?*鏁版嵁娴佸悜**
- [ ] 鎻愪緵浜?`const fn new()` 甯搁噺鏋勯€犲櫒
- [ ] 姣忎釜鐗╃悊璁惧瀵瑰簲涓€涓叏灞€ `static` 瀹炰緥
- [ ] `Ordering` 閫夋嫨姝ｇ‘锛堢粷澶у鏁板満鏅敤 `Relaxed`锛?
### Proxy 灞?
- [ ] Proxy struct 鍙瓨瀵诲潃淇℃伅锛堝湴鍧€/ID锛夛紝涓嶅瓨鐘舵€?- [ ] 鎸夎涔夐鍩熸媶鍒嗕簡 trait锛圧egister / Status / Settings / Operator锛?- [ ] `sync_*` 鏂规硶锛氳鎬荤嚎 鈫?store 鍒板奖瀛愬瘎瀛樺櫒 鈫?鏃犺繑鍥炲€?- [ ] `flush_*` 鏂规硶锛氫粠褰卞瓙瀵勫瓨鍣ㄧ殑 `target_*` 瀛楁璇?鈫?鍐欐€荤嚎
- [ ] `flush_*` 鏂规硶涓嶆帴鍙楁暟鍊煎弬鏁帮紙涓婂眰璋冪敤鍓嶅繀椤诲厛 store 鍒?`target_*` 瀛楁锛?- [ ] 鎿嶄綔鎴愬姛鍚庡悓姝ユ洿鏂板奖瀛愬瘎瀛樺櫒
- [ ] 鎿嶄綔澶辫触鍚庝笉淇敼褰卞瓙瀵勫瓨鍣?- [ ] 浣庡眰 `raw_*` 鍑芥暟绾补鍋氬崗璁敹鍙戯紝涓嶅惈涓氬姟閫昏緫
- [ ] 瓒呮椂鏃堕棿鐢ㄥ懡鍚嶅父閲忥紝涓嶇‖缂栫爜鏁板瓧

### Client 灞?
- [ ] Task 涓笉鐩存帴璋冪敤 `raw_*` 鍑芥暟
- [ ] Task 璇诲彇鐘舵€佹椂鍙粠褰卞瓙瀵勫瓨鍣?load
- [ ] 鍏变韩鎬荤嚎鐢?Mutex 淇濇姢
- [ ] 閿佹寔鏈夋椂闂存渶灏忓寲锛堝彧鍋?I/O锛?- [ ] 寤舵椂鏀惧湪閿侀噴鏀句箣鍚?
### 绂佹浜嬮」

- 鉂?绂佹鍦?driver/ 涓洿鎺ヤ慨鏀?proxy/ 鐨勫叏灞€鐘舵€侊紙闄や簡閫氳繃褰卞瓙瀵勫瓨鍣ㄥ紩鐢級
- 鉂?绂佹鍦?software/ task 涓洿鎺ヨ皟鐢?driver/ 鐨?raw_* 鍑芥暟
- 鉂?绂佹 Proxy struct 涓瓨鏀?runtime 鐘舵€?- 鉂?绂佹 sync 鏂规硶杩斿洖鍊硷紙Client 鑷繁浠庡奖瀛愬瘎瀛樺櫒璇伙級
- 鉂?绂佹 flush 鏂规硶鎺ュ彈涓氬姟鍙傛暟 鈥?蹇呴』浠庡奖瀛愬瘎瀛樺櫒鐨?`target_*` 瀛楁璇诲彇

---

## 涔濄€侀」鐩綋鍓嶅疄鐜板鐓э紙閲嶆瀯鍚庯級

| 瑙勮寖瑕佹眰 | 褰撳墠 JC2804 瀹炵幇 | 鐘舵€?|
|---------|-----------------|------|
| Proxy struct 鍙瓨 id | `Jc2804 { pub id: u8 }` | 鉁?|
| 褰卞瓙瀵勫瓨鍣ㄧ敤 Atomic | `AtomicU16/AtomicI32/AtomicU8/AtomicBool` | 鉁?|
| 鎸夎涔夋媶鍒?trait | `Jc2804Register / Jc2804Status / Jc2804Settings / Jc2804Operator` | 鉁?|
| sync 鏂规硶鏃犺繑鍥炲€?| `sync_*` 鍏ㄩ儴杩斿洖 `()` | 鉁?|
| flush 鏂规硶浠庡奖瀛愯 | `flush_position_to_device` 浠?`reg.target_position` 璇?| 鉁?|
| flush 鏂规硶浠庡奖瀛愯 | `flush_control_mode_to_device` 浠?`reg.target_control_mode` 璇?| 鉁?|
| 鎿嶄綔鎴愬姛鍚庡悓姝ュ奖瀛?| `sync_closed_loop_from_device` 鈫?`reg.closed_loop.store(...)` | 鉁?|
| 鎿嶄綔鍓嶅厛 store 鍒板奖瀛?| `sweep_loop` 鍏?`store(target_position)` 鍐嶈皟 `flush_position_to_device` | 鉁?|
| 鎿嶄綔鍓嶅厛 store 鍒板奖瀛?| `init`/`sweep_loop` 鍏?`store(target_control_mode)` 鍐嶈皟 `flush_control_mode_to_device` | 鉁?|
| Client 鍙褰卞瓙 | `cdc_send_task` 鍏ㄩ儴 load 鍘熷瓙鍙橀噺 | 鉁?|
| 鍏变韩鎬荤嚎 Mutex | `SharedCan = Mutex<..., Option<Tja1050>>` | 鉁?|
| 閿佸寤舵椂 | telemetry_loop 鐨?`Timer::after` 鍦ㄩ攣澶?| 鉁?|
| 鐩爣/瀹為檯瀛楁鍒嗙 | `control_mode`(瀹為檯) vs `target_control_mode`(鏈熸湜) | 鉁?|

---

## 鍗併€佹紨杩涘師鍒?
1. **鍏堟湁褰卞瓙瀵勫瓨鍣紝鍐嶆湁 Proxy**锛氳璁℃柊澶栬鏃讹紝鍏堝畾涔夈€屼笂灞傞渶瑕佺煡閬撲粈涔堢姸鎬併€嶏紝鍐嶈€冭檻銆屾€庝箞浠庣‖浠惰鍒拌繖浜涚姸鎬併€嶃€?2. **褰卞瓙瀵勫瓨鍣ㄦ槸鐪熺浉鏉ユ簮**锛欳lient 姘歌繙鐩镐俊褰卞瓙瀵勫瓨鍣ㄣ€傚鏋滃奖瀛愬瘎瀛樺櫒閿欎簡锛屾槸 Proxy 鐨?sync 閫昏緫鏈夐棶棰橈紝涓嶆槸 Client 鐨勯棶棰樸€?3. **涓€涓墿鐞嗚澶?鈫?涓€濂楀奖瀛愬瘎瀛樺櫒**锛氫笉瑕佸涓澶囧叡浜竴濂楀奖瀛愬瘎瀛樺櫒銆?4. **涓嶈涓?鏂逛究"鎵撶牬妯″紡**锛氭瘡涓€鏉?flush 閮藉繀椤讳粠褰卞瓙璇汇€傚鏋滆寰?store-then-flush 澶暟鍡︼紝灏佽涓€涓緟鍔╂柟娉曪紝浣嗗簳灞傝矾寰勪笉鑳界粫杩囥€?5. **鏂板澶栬鏃讹紝涓嶈淇敼宸叉湁褰卞瓙瀵勫瓨鍣ㄧ殑缁撴瀯**锛氱粰鏂板璁惧缓鏂版枃浠躲€佹柊缁撴瀯浣撱€佹柊 static銆?
