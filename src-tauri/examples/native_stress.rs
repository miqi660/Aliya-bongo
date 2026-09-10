//! 无窗口 Runtime 的 create/destroy 回归；模型与 OpenGL 由 native_opengl 验证。
use std::{env, ptr, thread, time::Duration};

use bongo_cat_lib::native_runtime::{NativeRuntime, ffi::*};

fn argument<T: std::str::FromStr>(args: &[String], index: usize, default: T) -> T {
    args.get(index)
        .and_then(|value| value.parse().ok())
        .unwrap_or(default)
}

fn main() {
    let args: Vec<_> = env::args().collect();
    let iterations: usize = argument(&args, 1, 100);
    let pause_ms: u64 = argument(&args, 2, 0);
    assert!(iterations > 0, "迭代次数必须大于 0");

    let mut version = None;
    for iteration in 1..=iterations {
        let runtime = NativeRuntime::new().expect("创建 Native Runtime 失败");
        let current_version = runtime.core_version().expect("读取 Core 版本失败");
        if let Some(expected) = version {
            assert_eq!(current_version, expected, "Core 版本发生变化");
        } else {
            assert_ne!(current_version, 0, "Core 版本不应为 0");
            version = Some(current_version);
        }
        drop(runtime);

        // 验证 destroy 后句柄已清空，重复 destroy 不会触发 double free。
        let mut raw = ptr::null_mut();
        unsafe {
            assert_eq!(runtime_create(&mut raw), 0);
            assert_eq!(runtime_destroy(&mut raw), 0);
            assert!(raw.is_null());
            assert_eq!(runtime_destroy(&mut raw), 0);
        }

        if pause_ms > 0 {
            thread::sleep(Duration::from_millis(pause_ms));
        }
        if iteration == 1 || iteration == iterations || iteration % 1_000 == 0 {
            println!("progress: {iteration}/{iterations}");
        }
    }

    println!(
        "Native Runtime create/destroy ×{iterations} PASS，Core 版本：{:#010x}",
        version.expect("至少执行一次迭代")
    );
}
