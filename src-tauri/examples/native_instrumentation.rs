//! Phase 8 Instrumentation + Release 调优验收。
use std::ffi::c_uint;

unsafe extern "C" {
    fn native_instrumentation_run(repeats: c_uint) -> i32;
}

fn main() {
    // 强制链接包含 Phase 8 instrumentation 验收入口的 Native Bridge。
    let runtime = bongo_cat_lib::native_runtime::NativeRuntime::new().expect("创建失败");
    drop(runtime);
    let repeats = std::env::args()
        .nth(1)
        .map(|value| value.parse().expect("重复次数必须为整数"))
        .unwrap_or(100);
    assert!((1..=10_000).contains(&repeats), "重复次数必须在 1..10000");
    let status = unsafe { native_instrumentation_run(repeats) };
    assert_eq!(status, 0, "Phase 8 Instrumentation 验收失败");
}
