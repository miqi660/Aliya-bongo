//! Phase 3 Native Window Lifecycle 验收：Window 与 Cubism/OpenGL Runtime 同线程联动。
use std::ffi::{CString, c_char};

unsafe extern "C" {
    fn native_window_lifecycle_run(
        model: *const c_char,
        resize_cycles: u32,
        visibility_cycles: u32,
    ) -> i32;
}

fn main() {
    // 强制链接包含生命周期验收入口的 Native Bridge。
    let runtime = bongo_cat_lib::native_runtime::NativeRuntime::new().expect("创建失败");
    drop(runtime);
    let mut args = std::env::args().skip(1);
    let model = args
        .next()
        .unwrap_or_else(|| "src-tauri/assets/models/standard/cat.model3.json".into());
    let resize_cycles = args
        .next()
        .map(|value| value.parse().expect("Resize 次数必须是整数"));
    let visibility_cycles = args
        .next()
        .map(|value| value.parse().expect("Show/Hide 次数必须是整数"));
    let model = CString::new(model).expect("模型路径包含 NUL");
    let status = unsafe {
        native_window_lifecycle_run(
            model.as_ptr(),
            resize_cycles.unwrap_or(500),
            visibility_cycles.unwrap_or(100),
        )
    };
    assert_eq!(status, 0, "Phase 3 Native Window Lifecycle 验收失败");
}
