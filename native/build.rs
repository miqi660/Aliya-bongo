use std::{env, path::PathBuf};

pub fn build() {
    assert_eq!(
        env::var("TARGET").unwrap(),
        "x86_64-pc-windows-msvc",
        "当前 Native Skeleton 仅验证 Windows x64 MSVC"
    );
    println!("cargo:rerun-if-env-changed=CUBISM_SDK_ROOT");
    println!("cargo:rerun-if-changed=../native");
    let sdk = env::var_os("CUBISM_SDK_ROOT")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap())
                .join("../../CubismSdkForNative-5-r.5")
        })
        .canonicalize()
        .expect("SDK 不存在，请设置 CUBISM_SDK_ROOT");
    let sdk = PathBuf::from(sdk.to_string_lossy().trim_start_matches(r"\\?\"));
    let framework = sdk.join("Framework/src");
    println!("cargo:rerun-if-changed={}", framework.display());
    println!("cargo:rerun-if-changed={}", sdk.join("Core").display());
    // Skeleton 编译 Framework 的 CPU 部分；Renderer 后端在 Phase 2 接入。
    fn sources(dir: &std::path::Path, build: &mut cc::Build) {
        for entry in std::fs::read_dir(dir).unwrap() {
            let path = entry.unwrap().path();
            if path.is_dir() {
                if path.file_name().unwrap() != "Rendering" {
                    sources(&path, build);
                }
            } else if path.extension().is_some_and(|ext| ext == "cpp") {
                build.file(path);
            }
        }
    }
    let mut build = cc::Build::new();
    build
        .cpp(true)
        .std("c++17")
        .flag("/EHsc")
        .flag("/utf-8")
        .include(&framework)
        .include(sdk.join("Core/include"));
    sources(&framework, &mut build);
    build.file("../native/runtime.cpp").compile("aliya_native");
    let crt = if env::var("CARGO_CFG_TARGET_FEATURE")
        .unwrap_or_default()
        .split(',')
        .any(|v| v == "crt-static")
    {
        "MT"
    } else {
        "MD"
    };
    // cc 与 Rust 使用 Release CRT，即使 Cargo profile 为 Debug。
    println!(
        "cargo:rustc-link-search=native={}",
        sdk.join("Core/lib/windows/x86_64/143").display()
    );
    println!("cargo:rustc-link-lib=static=Live2DCubismCore_{crt}");
}
