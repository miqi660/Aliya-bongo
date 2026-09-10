//! Phase 4 Render Scheduler 验收：四态状态机、FPS 限制与单调时间。
use std::ffi::c_uint;

unsafe extern "C" {
    fn native_scheduler_run(repeats: c_uint) -> i32;
}

fn main() {
    // 强制链接包含 Scheduler 验收入口的 Native Bridge。
    let runtime = bongo_cat_lib::native_runtime::NativeRuntime::new().expect("创建失败");
    drop(runtime);
    let repeats = std::env::args()
        .nth(1)
        .map(|value| value.parse().expect("重复次数必须为整数"))
        .unwrap_or(1);
    assert!(repeats > 0, "重复次数必须大于零");
    let status = unsafe { native_scheduler_run(repeats) };
    assert_eq!(status, 0, "Phase 4 Render Scheduler 验收失败");
}
