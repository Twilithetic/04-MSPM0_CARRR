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

const C_FILES: &[(&str, &str)] = &[
    // ---- SysConfig generated ----
    ("ti_msp_dl_config.c", "Debug/ti_msp_dl_config.c"),
    (
        "startup_mspm0g350x_ticlang.c",
        "C:/ti/mspm0_sdk_2_10_00_04/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c",
    ),

    // ---- Proxy: shadow register definitions ----
    ("registers.c", "src/proxy/registers.c"),

    // ---- Driver: hardware proxy implementations ----
    ("motor.c", "src/driver/motor/motor.c"),
    ("line.c", "src/driver/line/line.c"),
    ("uart_debug.c", "src/driver/uart/uart_debug.c"),

    // ---- Software: application layer ----
    ("controller.c", "src/software/controller.c"),
    ("main.c", "src/main.c"),
];

const OBJS: &[&str] = &[
    "ti_msp_dl_config",
    "startup_mspm0g350x_ticlang",
    "registers",
    "motor",
    "line",
    "uart_debug",
    "controller",
    "main",
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

/// Run a command.  On failure, retry once after a short delay — this handles
/// transient Windows file locks left behind by probe-rs / DSLite / VSCode
/// debugger sessions that haven't fully released their output-file handles.
fn run_with_retry(make_cmd: impl Fn() -> Command, label: &str) {
    let mut cmd = make_cmd();
    let status = cmd
        .status()
        .unwrap_or_else(|e| panic!("{label}: failed to launch — {e}"));
    if status.success() {
        return;
    }
    // first attempt failed — wait for stale locks to drain
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
                .args(["-Wl,-m", &map])
                .args(["-Wl,-i", SDK_SOURCE])
                .args(["-Wl,-i", &project_str])
                .args(["-Wl,-i", COMPILER_LIB])
                .arg("-Wl,--diag_wrap=off")
                .arg("-Wl,--display_error_number")
                .arg("-Wl,--warn_sections")
                .args(["-Wl,--xml_link_info", &link_xml])
                .arg("-Wl,--rom_model")
                .args(["-o", &elf]);
            for ob in OBJS {
                c.arg(plain(&debug.join(format!("{ob}.o"))));
            }
            c.args(["-Wl,-l", &linker_cmd])
                .arg("-Wl,-ldevice.cmd.genlibs")
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
