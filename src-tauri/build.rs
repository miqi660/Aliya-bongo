fn main() {
    if std::env::var_os("CARGO_FEATURE_NATIVE_RUNTIME").is_some() {
        native_build::build();
    }
    tauri_build::build()
}

#[path = "../native/build.rs"]
mod native_build;
