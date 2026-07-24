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
    ("empty.c", "empty.c"),
    ("ti_msp_dl_config.c", "Debug/ti_msp_dl_config.c"),
    (
        "startup_mspm0g350x_ticlang.c",
        "C:/ti/mspm0_sdk_2_10_00_04/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c",
    ),
];

const OBJS: &[&str] = &["empty", "ti_msp_dl_config", "startup_mspm0g350x_ticlang"];

// ---------- helpers ----------

/// Convert a Path to a plain string.  Strips the Windows `\\?\` verbatim
/// prefix when present so the TI toolchain doesn't choke on it.
fn plain(p: &Path) -> String {
    let s = p.to_str().unwrap_or(".");
    if s.starts_with("\\\\?\\") {
        // "\\\\?\\D:\\..."  →  "D:\\..."
        s[4..].to_owned()
    } else {
        s.to_owned()
    }
}

fn project_dir() -> PathBuf {
    // CARGO_MANIFEST_DIR = .../empty/build_tool  → project = .../empty
    PathBuf::from(
        std::env::var("CARGO_MANIFEST_DIR")
            .unwrap_or_else(|_| ".".into()),
    )
    .parent()
    .unwrap()
    .to_owned()
}

fn run_or_die(cmd: &mut Command, label: &str) {
    let status = cmd
        .status()
        .unwrap_or_else(|e| panic!("{label}: failed to launch — {e}"));
    if !status.success() {
        eprintln!("ERROR: {label} (exit code: {:?})", status.code());
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
    let mut sc = Command::new(SYSCONFIG);
    sc.args(["-s", PRODUCT_JSON])
        .arg("--script")
        .arg(plain(&project.join("empty.syscfg")))
        .arg("-o")
        .arg(plain(&debug))
        .arg("--compiler")
        .arg("ticlang")
        .current_dir(plain(&debug));
    run_or_die(&mut sc, "SysConfig failure");

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

        let dev_opt = format!("@{}", plain(&debug.join("device.opt")));
        let mut cc = Command::new(TICLANG);
        cc.arg("-c")
            .arg(&dev_opt)
            .args(ARCH_FLAGS)
            .args(["-O2", "-gdwarf-3", "-Wall"])
            .args(["-D__MSPM0G3507__", "-D__USE_SYSCONFIG__"])
            .args(["-I", &plain(&project)])
            .args(["-I", &plain(&debug)])
            .args(["-I", CMSIS_INCLUDE])
            .args(["-I", SDK_SOURCE])
            .args(["-o", &obj])
            .arg(&src_path)
            .current_dir(plain(&debug));
        run_or_die(&mut cc, &format!("compile {label} failed"));
    }

    // --- 3. Link ---
    println!("\n--- Link ---");
    let elf = plain(&debug.join("empty.out"));
    let map = plain(&debug.join("empty.map"));
    let link_xml = plain(&debug.join("empty_linkInfo.xml"));
    let linker_cmd = plain(&debug.join("device_linker.cmd"));

    let mut ld = Command::new(TICLANG);
    ld.arg(format!("@{}", plain(&debug.join("device.opt"))))
        .args(ARCH_FLAGS)
        .args(["-O2", "-gdwarf-3", "-Wall"])
        .args(["-Wl,-m", &map])
        .args(["-Wl,-i", SDK_SOURCE])
        .args(["-Wl,-i", &plain(&project)])
        .args(["-Wl,-i", COMPILER_LIB])
        .arg("-Wl,--diag_wrap=off")
        .arg("-Wl,--display_error_number")
        .arg("-Wl,--warn_sections")
        .args(["-Wl,--xml_link_info", &link_xml])
        .arg("-Wl,--rom_model")
        .args(["-o", &elf]);
    for ob in OBJS {
        ld.arg(plain(&debug.join(format!("{ob}.o"))));
    }
    ld.args(["-Wl,-l", &linker_cmd])
        .arg("-Wl,-ldevice.cmd.genlibs")
        .arg("-Wl,-llibc.a")
        .current_dir(plain(&debug));
    run_or_die(&mut ld, "link failure");

    // --- 4. Hex ---
    println!("\n--- Hex ---");
    let hex = plain(&debug.join("empty.hex"));
    let mut hx = Command::new(TIARMHEX);
    hx.args(["--memwidth=8", "--romwidth=8", "--diag_wrap=off", "--intel"])
        .args(["-o", &hex])
        .arg(&elf)
        .current_dir(plain(&debug));
    run_or_die(&mut hx, "hex generation failure");

    println!("\n=== BUILD SUCCESS ===");
    println!("ELF  : {elf}");
    println!("HEX  : {hex}");
    ExitCode::SUCCESS
}
