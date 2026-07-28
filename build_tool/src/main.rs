use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

// ---------- toolchain paths ----------

const SYSCONFIG: &str =
    "C:/ti/ccs2100/ccs/utils/sysconfig_1.28.0/sysconfig_cli.bat";
const PRODUCT_JSON: &str =
    "C:/ti/mspm0_sdk_2_10_00_04/.metadata/product.json";
const TICLANG: &str =
    "C:/ti/ccs2100/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang.exe";
const TIARMHEX: &str =
    "C:/ti/ccs2100/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmhex.exe";

const SDK_SOURCE: &str = "C:/ti/mspm0_sdk_2_10_00_04/source";
const SDK_KERNEL: &str = "C:/ti/mspm0_sdk_2_10_00_04/kernel";
const TI_DRIVERS_LIB: &str =
    "C:/ti/mspm0_sdk_2_10_00_04/source/ti/drivers/lib/ticlang/m0p/drivers_mspm0g1x0x_g3x0x.a";
const DRIVERLIB_LIB: &str =
    "C:/ti/mspm0_sdk_2_10_00_04/source/ti/driverlib/lib/ticlang/m0p/mspm0g1x0x_g3x0x/driverlib.a";
const CMSIS_INCLUDE: &str =
    "C:/ti/mspm0_sdk_2_10_00_04/source/third_party/CMSIS/Core/Include";
const COMPILER_LIB: &str =
    "C:/ti/ccs2100/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/lib";

const ARCH_FLAGS: &[&str] = &[
    "-march=thumbv6m",
    "-mcpu=cortex-m0plus",
    "-mfloat-abi=soft",
    "-mlittle-endian",
    "-mthumb",
];

/// (label, source_path) — source_path is project‑relative unless it starts with a drive letter.
const C_FILES: &[(&str, &str)] = &[
    // ---- SysConfig generated ----
    ("ti_msp_dl_config.c", "Debug/ti_msp_dl_config.c"),
    ("startup_mspm0g350x_ticlang.c",
     "C:/ti/mspm0_sdk_2_10_00_04/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c"),

    // ---- Application ----
    ("main.c",       "src/main.c"),
    ("task.c",       "src/software/task.c"),
    ("app_hooks.c",  "src/software/app_hooks.c"),

    // ---- Driver (minimal — only LED) ----
    ("build_in_led.c", "src/driver/board/build_in_led.c"),
    ("XDS110_cdc.c",   "src/driver/board/XDS110_cdc.c"),
    ("registers.c",    "src/proxy/registers.c"),
    ("I2C_test.c",     "src/driver/chip/I2C_test.c"),
    ("ti_drivers_i2c_config.c", "src/driver/chip/ti_drivers_i2c_config.c"),
    // ("ti_drivers_i2c1_config.c", "src/driver/chip/ti_drivers_i2c1_config.c"),  // not needed for GPIO bit-bang I2C
    // NOTE: board_drivers_config.c is included above — if ti_drivers_i2c_config.c
    // also defines GPIO_config/I2C_config, they will conflict at link time.
    // We include the per‑board drivers_config variant here.
    // ("board_drivers_config.c", "src/driver/board/board_drivers_config.c"),
    ("motor_driver_uart.c", "src/driver/board/motor_driver_uart.c"),
    // ---- IMU (LSM6DSV16X) ----
    ("LSM6DSV16X.c",    "src/driver/board/LSM6DSV16X.c"),
    ("lsm6dsv16x_reg.c", "src/driver/board/lsm6dsv16x_reg.c"),

    // ---- TI Drivers DPL (FreeRTOS porting layer) ----
    ("HwiPMSPM0_freertos.c", "C:/ti/mspm0_sdk_2_10_00_04/kernel/freertos/dpl/HwiPMSPM0_freertos.c"),
    ("SemaphoreP_freertos.c", "C:/ti/mspm0_sdk_2_10_00_04/kernel/freertos/dpl/SemaphoreP_freertos.c"),
    ("ClockP_freertos.c", "C:/ti/mspm0_sdk_2_10_00_04/kernel/freertos/dpl/ClockP_freertos.c"),
    ("DebugP_freertos.c", "C:/ti/mspm0_sdk_2_10_00_04/kernel/freertos/dpl/DebugP_freertos.c"),
    ("SystemP_freertos.c", "C:/ti/mspm0_sdk_2_10_00_04/kernel/freertos/dpl/SystemP_freertos.c"),

    // ---- POSIX thread-local storage (needed by vTaskDelete) ----
    ("PTLS.c", "rtos/posix/PTLS.c"),

    // ---- FreeRTOS kernel ----
    ("tasks.c",         "rtos/FreeRTOS/tasks.c"),
    ("queue.c",         "rtos/FreeRTOS/queue.c"),
    ("list.c",          "rtos/FreeRTOS/list.c"),
    ("timers.c",        "rtos/FreeRTOS/timers.c"),
    ("event_groups.c",  "rtos/FreeRTOS/event_groups.c"),
    ("stream_buffer.c", "rtos/FreeRTOS/stream_buffer.c"),
    ("port.c",          "rtos/FreeRTOS/portable/TI_ARM_CLANG/ARM_CM0/port.c"),
    ("portasm.c",       "rtos/FreeRTOS/portable/TI_ARM_CLANG/ARM_CM0/portasm.c"),
    ("heap_4.c",        "rtos/FreeRTOS/MemMang/heap_4.c"),
];

const OBJS: &[&str] = &[
    "ti_msp_dl_config",
    "startup_mspm0g350x_ticlang",
    "main",
    "task",
    "app_hooks",
    "build_in_led",
    "XDS110_cdc",
    "registers",
    "I2C_test",
    "ti_drivers_i2c_config",
    "ti_drivers_i2c_config",
    // "ti_drivers_i2c1_config",  // not needed — GPIO bit-bang I2C
    // "board_drivers_config",    // conflicts with ti_drivers_i2c_config
    "motor_driver_uart",
    "LSM6DSV16X",
    "lsm6dsv16x_reg",
    "HwiPMSPM0_freertos",
    "SemaphoreP_freertos",
    "ClockP_freertos",
    "DebugP_freertos",
    "SystemP_freertos",
    "tasks",
    "queue",
    "list",
    "timers",
    "event_groups",
    "stream_buffer",
    "port",
    "portasm",
    "heap_4",
    "PTLS",
];

// ---------- helpers ----------

fn plain(p: &Path) -> String {
    let s = p.to_str().unwrap_or(".");
    if s.starts_with("\\\\?\\") {
        s[4..].to_owned()
    } else {
        s.to_owned()
    }
}

fn project_dir() -> PathBuf {
    PathBuf::from(
        std::env::var("CARGO_MANIFEST_DIR")
            .unwrap_or_else(|_| ".".into()),
    )
    .parent()
    .unwrap()
    .to_owned()
}

fn run_with_retry(make_cmd: impl Fn() -> Command, label: &str) {
    let mut cmd = make_cmd();
    let status = cmd
        .status()
        .unwrap_or_else(|e| panic!("{label}: failed to launch — {e}"));
    if status.success() {
        return;
    }
    eprintln!(
        "WARN: {label} failed (exit {:?}) — file may be locked, retrying in 2 s…",
        status.code()
    );
    std::thread::sleep(std::time::Duration::from_secs(2));
    let mut retry = make_cmd();
    let s2 = retry
        .status()
        .unwrap_or_else(|e| panic!("{label} retry: failed to launch — {e}"));
    if !s2.success() {
        eprintln!("ERROR: {label} retry also failed (exit {:?})", s2.code());
        if label.contains("link") || label.contains("hex") {
            eprintln!();
            eprintln!("HINT: A process is still holding the output file open.");
            eprintln!(
                "      Kill probe-rs / DSLite / debugger sessions, then try again."
            );
            eprintln!(
                "      PowerShell:  Get-Process probe*,DSLite* | Stop-Process -Force"
            );
        }
        std::process::exit(1);
    }
}

// ---------- main ----------

fn main() -> ExitCode {
    let project = project_dir();
    let debug = project.join("Debug");

    println!("=== MSPM0 Build Tool (Rust) ===");
    println!("project : {}", plain(&project));
    println!("debug   : {}", plain(&debug));

    // --- 1. SysConfig ---
    println!("\n--- SysConfig ---");
    run_with_retry(
        || {
            let mut c = Command::new(SYSCONFIG);
            c.args(["-s", PRODUCT_JSON])
                .arg("--script")
                .arg(plain(&project.join("empty.syscfg")))
                .arg("-o")
                .arg(plain(&debug))
                .arg("--compiler")
                .arg("ticlang")
                .current_dir(plain(&debug));
            c
        },
        "SysConfig",
    );

    // --- 2. Compile ---
    println!("\n--- Compile ---");
    for (label, src) in C_FILES {
        let src_path = if src.starts_with("C:") || src.starts_with("c:") {
            src.to_string()
        } else {
            plain(&project.join(src))
        };
        let stem = std::path::Path::new(src)
            .file_stem()
            .unwrap()
            .to_str()
            .unwrap();
        let obj = plain(&debug.join(format!("{stem}.o")));

        println!("  {label}");
        println!("    src  {src_path}");
        println!("    obj  {obj}");

        let project_dir = plain(&project);
        let debug_dir = plain(&debug);
        let src_dir = plain(&project.join("src"));
        let rtos_inc = plain(&project.join("rtos/FreeRTOS/include"));
        let rtos_port = plain(&project.join("rtos/FreeRTOS/portable/TI_ARM_CLANG/ARM_CM0"));
        let rtos_root = plain(&project.join("rtos"));
        let ti_drivers_inc = format!("{}/ti/drivers", SDK_SOURCE);
        let dpl_inc = format!("{}/freertos/dpl", SDK_KERNEL);
        let display_inc = format!("{}/ti/display", SDK_SOURCE);
        let dev_opt = format!("@{}", plain(&debug.join("device.opt")));
        run_with_retry(
            || {
                let mut c = Command::new(TICLANG);
                c.arg("-c")
                    .arg(&dev_opt)
                    .args(ARCH_FLAGS)
                    .args(["-O2", "-gdwarf-3", "-Wall"])
                    .args(["-D__MSPM0G3507__", "-D__USE_SYSCONFIG__"])
                    .args(["-I", &project_dir])
                    .args(["-I", &debug_dir])
                    .args(["-I", &src_dir])
                    .args(["-I", &rtos_inc])
                    .args(["-I", &rtos_port])
                    .args(["-I", &rtos_root])
                    .args(["-I", &ti_drivers_inc])
                    .args(["-I", &dpl_inc])
                    .args(["-I", &display_inc])
                    .args(["-I", CMSIS_INCLUDE])
                    .args(["-I", SDK_SOURCE])
                    .args(["-o", &obj])
                    .arg(&src_path)
                    .current_dir(&debug_dir);
                c
            },
            &format!("compile {label}"),
        );
    }

    // --- 3. Link ---
    println!("\n--- Link ---");
    let elf = plain(&debug.join("empty.out"));
    let map = plain(&debug.join("empty.map"));
    let link_xml = plain(&debug.join("empty_linkInfo.xml"));
    let linker_cmd = plain(&debug.join("device_linker.cmd"));
    let project_str = plain(&project);
    let debug_str = plain(&debug);

    run_with_retry(
        || {
            let mut c = Command::new(TICLANG);
            c.arg(format!("@{}", plain(&debug.join("device.opt"))))
                .args(ARCH_FLAGS)
                .args(["-O2", "-gdwarf-3", "-Wall"])
                .arg(format!("-Wl,-m={}", &map))
                .args(["-Wl,-i", SDK_SOURCE])
                .args(["-Wl,-i", &project_str])
                .args(["-Wl,-i", COMPILER_LIB])
                .arg("-Wl,--diag_wrap=off")
                .arg("-Wl,--display_error_number")
                .arg("-Wl,--warn_sections")
                .arg(format!("-Wl,--xml_link_info={}", &link_xml))
                .arg("-Wl,--rom_model")
                .args(["-o", &elf]);
            for ob in OBJS {
                c.arg(plain(&debug.join(format!("{ob}.o"))));
            }
            c.args(["-Wl,-l", &linker_cmd])
                .arg("-Wl,-ldevice.cmd.genlibs")
                .arg(TI_DRIVERS_LIB)
                .arg(DRIVERLIB_LIB)
                .arg("-Wl,-llibc.a")
                .current_dir(&debug_str);
            c
        },
        "link",
    );

    // --- 4. Hex ---
    println!("\n--- Hex ---");
    let hex = plain(&debug.join("empty.hex"));
    run_with_retry(
        || {
            let mut c = Command::new(TIARMHEX);
            c.args(["--memwidth=8", "--romwidth=8", "--diag_wrap=off", "--intel"])
                .args(["-o", &hex])
                .arg(&elf)
                .current_dir(plain(&debug));
            c
        },
        "hex",
    );

    println!("\n=== BUILD SUCCESS ===");
    println!("ELF  : {elf}");
    println!("HEX  : {hex}");
    ExitCode::SUCCESS
}
