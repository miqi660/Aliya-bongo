use std::{env, path::PathBuf};

pub fn build() {
    assert_eq!(
        env::var("TARGET").unwrap(),
        "x86_64-pc-windows-msvc",
        "当前 Native Runtime 仅支持 Windows x64 MSVC"
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
    println!(
        "cargo:rerun-if-changed={}",
        sdk.join("Samples/OpenGL/thirdParty/stb/stb_image.h")
            .display()
    );
    // CPU Framework 与唯一的 OpenGL 后端分别加入。
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
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let mut shaders = String::from(
        "static const struct { const char* name; const char* source; } shaders[] = {\n",
    );
    for entry in std::fs::read_dir(framework.join("Rendering/OpenGL/Shaders/Standard")).unwrap() {
        let path = entry.unwrap().path();
        if path.is_file() {
            let source = std::fs::read_to_string(&path).unwrap();
            shaders.push_str(&format!(
                "{{\"{}\", R\"SHADER({})SHADER\"}},\n",
                path.file_name().unwrap().to_str().unwrap(),
                source
            ));
        }
    }
    shaders.push_str("};\n");
    std::fs::write(out.join("embedded_shaders.h"), shaders).unwrap();
    cc::Build::new()
        .file("../native/vendor/glew/src/glew.c")
        .include("../native/vendor/glew/include")
        .define("GLEW_STATIC", None)
        .warnings(false)
        .compile("aliya_glew");
    build
        .cpp(true)
        .std("c++17")
        .flag("/EHsc")
        .flag("/utf-8")
        .define("CSM_TARGET_WIN_GL", None)
        .define("GLEW_STATIC", None)
        .define("NOMINMAX", None)
        .include(&out)
        .include("../native/vendor/glew/include")
        .include(sdk.join("Samples/OpenGL/thirdParty/stb"))
        .include(&framework)
        .include(sdk.join("Core/include"));
    sources(&framework, &mut build);
    sources(&framework.join("Rendering/OpenGL"), &mut build);
    build.file(framework.join("Rendering/CubismRenderer.cpp"));
    build.file(framework.join("Rendering/csmBlendMode.cpp"));
    build
        .file("../native/runtime.cpp")
        .file("../native/scheduler.cpp")
        .file("../native/dirty_sleep.cpp")
        .file("../native/validation.cpp")
        .compile("aliya_native");
    for lib in ["opengl32", "gdi32", "user32", "dwmapi"] {
        println!("cargo:rustc-link-lib={lib}");
    }
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
