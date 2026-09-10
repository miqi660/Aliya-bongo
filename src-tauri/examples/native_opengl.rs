// 独立 Native 验收窗口，不启动 Tauri/WebView。
use std::ffi::{CString, c_char};
unsafe extern "C" {
    fn native_validation_run(model: *const c_char, output: *const c_char, seconds: u32) -> i32;
}
fn main() {
    // 强制链接包含验收入口的 Native Bridge。
    let runtime = bongo_cat_lib::native_runtime::NativeRuntime::new().expect("创建失败");
    drop(runtime);
    let mut args = std::env::args().skip(1);
    let model = args
        .next()
        .unwrap_or_else(|| "src-tauri/assets/models/standard/cat.model3.json".into());
    let output = args.next().unwrap_or_else(|| "target/phase-02".into());
    let seconds = args
        .next()
        .map(|s| s.parse().expect("时长必须为秒数"))
        .unwrap_or(0);
    let model = CString::new(model).expect("模型路径包含 NUL");
    let output = CString::new(output).expect("输出路径包含 NUL");
    let status = unsafe { native_validation_run(model.as_ptr(), output.as_ptr(), seconds) };
    assert_eq!(status, 0, "Phase 2 验收失败");
}
