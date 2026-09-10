// 单次链接/生命周期冒烟验证；不执行 1.4 的循环压力测试。
fn main() {
    let mut runtime = bongo_cat_lib::native_runtime::NativeRuntime::new().expect("创建失败");
    let version = runtime.core_version().expect("读取 Core 版本失败");
    assert_ne!(version, 0);
    println!("Native Runtime 创建成功，Core 版本：{version:#010x}");
    use bongo_cat_lib::native_runtime::NativeError;
    assert!(!runtime.is_dirty().unwrap());
    assert!(!runtime.is_animating().unwrap());
    assert_eq!(runtime.render(), Err(NativeError::INVALID_STATE));
    assert_eq!(
        runtime.load("missing.model3.json"),
        Err(NativeError::INVALID_STATE)
    );
    assert_eq!(
        runtime.load("bad\0path"),
        Err(NativeError::INVALID_ARGUMENT)
    );
    // 空句柄必须返回统一错误码，并强制链接所有约定的 ABI 符号。
    use bongo_cat_lib::native_runtime::ffi::*;
    let null = std::ptr::null_mut();
    unsafe {
        assert_eq!(runtime_create(std::ptr::null_mut()), 1);
        assert_eq!(runtime_destroy(std::ptr::null_mut()), 1);
        for status in [
            model_load(null, std::ptr::null()),
            runtime_attach_window(null, std::ptr::null_mut()),
            model_unload(null),
            runtime_resize(null, 1, 1),
            parameter_set(null, std::ptr::null(), 0.0),
            parameter_add(null, std::ptr::null(), 0.0),
            parameter_get(null, std::ptr::null(), std::ptr::null_mut()),
            motion_start(null, std::ptr::null(), 0),
            motion_stop(null),
            expression_set(null, std::ptr::null()),
            runtime_update(null, 0.0),
            runtime_render(null),
            runtime_present(null),
            runtime_is_dirty(null, std::ptr::null_mut()),
            runtime_is_animating(null, std::ptr::null_mut()),
        ] {
            assert_eq!(status, 1);
        }
    }
    drop(runtime);
    println!("Native Runtime 已销毁");
}
