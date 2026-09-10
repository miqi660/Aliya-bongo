//! Phase 7 Native/OpenGL/Cubism 资源生命周期压力验收。
use std::ffi::{CString, c_char};

unsafe extern "C" {
    fn native_resource_lifecycle_run(
        model: *const c_char,
        load_cycles: u32,
        show_hide_cycles: u32,
        resize_cycles: u32,
        motion_cycles: u32,
    ) -> i32;
}

fn main() {
    // 强制链接包含资源生命周期验收入口的 Native Bridge。
    let runtime = bongo_cat_lib::native_runtime::NativeRuntime::new().expect("创建失败");
    drop(runtime);
    let mut args = std::env::args().skip(1);
    let model = args
        .next()
        .unwrap_or_else(|| "src-tauri/assets/models/standard/cat.model3.json".into());
    let load_cycles = args
        .next()
        .map(|value| value.parse().expect("Load/Unload 次数必须是整数"))
        .unwrap_or(100);
    let show_hide_cycles = args
        .next()
        .map(|value| value.parse().expect("Show/Hide 次数必须是整数"))
        .unwrap_or(500);
    let resize_cycles = args
        .next()
        .map(|value| value.parse().expect("Resize 次数必须是整数"))
        .unwrap_or(500);
    let motion_cycles = args
        .next()
        .map(|value| value.parse().expect("Motion 次数必须是整数"))
        .unwrap_or(1000);
    let model = CString::new(model).expect("模型路径包含 NUL");
    let status = unsafe {
        native_resource_lifecycle_run(
            model.as_ptr(),
            load_cycles,
            show_hide_cycles,
            resize_cycles,
            motion_cycles,
        )
    };
    assert_eq!(status, 0, "Phase 7 Resource Lifecycle 验收失败");
}
