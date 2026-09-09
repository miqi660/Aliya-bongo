//! Phase 1 C ABI；模型与渲染 API 为明确返回未实现的占位。
use std::{marker::PhantomData, ptr::NonNull, rc::Rc};
pub mod ffi {
    #[repr(C)]
    pub struct AliyaRuntime {
        _private: [u8; 0],
    }
    // 安全约束见 native/runtime.h；所有指针必须有效，调用限定创建线程。
    unsafe extern "C" {
        pub fn runtime_create(out: *mut *mut AliyaRuntime) -> i32;
        pub fn runtime_destroy(runtime: *mut *mut AliyaRuntime) -> i32;
        pub fn runtime_core_version(runtime: *mut AliyaRuntime, out: *mut u32) -> i32;
        pub fn model_load(runtime: *mut AliyaRuntime, path: *const std::ffi::c_char) -> i32;
        pub fn model_unload(runtime: *mut AliyaRuntime) -> i32;
        pub fn runtime_resize(runtime: *mut AliyaRuntime, width: u32, height: u32) -> i32;
        pub fn parameter_set(
            runtime: *mut AliyaRuntime,
            id: *const std::ffi::c_char,
            value: f32,
        ) -> i32;
        pub fn parameter_add(
            runtime: *mut AliyaRuntime,
            id: *const std::ffi::c_char,
            value: f32,
        ) -> i32;
        pub fn motion_start(
            runtime: *mut AliyaRuntime,
            group: *const std::ffi::c_char,
            index: u32,
        ) -> i32;
        pub fn motion_stop(runtime: *mut AliyaRuntime) -> i32;
        pub fn expression_set(runtime: *mut AliyaRuntime, id: *const std::ffi::c_char) -> i32;
        pub fn runtime_update(runtime: *mut AliyaRuntime, delta_seconds: f32) -> i32;
        pub fn runtime_render(runtime: *mut AliyaRuntime) -> i32;
        pub fn runtime_is_dirty(runtime: *mut AliyaRuntime, out: *mut u8) -> i32;
        pub fn runtime_is_animating(runtime: *mut AliyaRuntime, out: *mut u8) -> i32;
    }
}
#[derive(Debug, PartialEq, Eq)]
pub struct NativeError(pub i32);
impl NativeError {
    pub const INVALID_ARGUMENT: Self = Self(1);
    pub const WRONG_THREAD: Self = Self(2);
    pub const NOT_IMPLEMENTED: Self = Self(3);
    pub const OUT_OF_MEMORY: Self = Self(4);
    pub const INTERNAL_ERROR: Self = Self(5);
}
fn check(status: i32) -> Result<(), NativeError> {
    if status == 0 {
        Ok(())
    } else {
        Err(NativeError(status))
    }
}
/// Rust 唯一所有者；Rc 标记禁止 Send/Sync，Drop 在创建线程释放。
pub struct NativeRuntime {
    handle: NonNull<ffi::AliyaRuntime>,
    _thread: PhantomData<Rc<()>>,
}
impl NativeRuntime {
    pub fn new() -> Result<Self, NativeError> {
        let mut raw = std::ptr::null_mut();
        check(unsafe { ffi::runtime_create(&mut raw) })?;
        Ok(Self {
            handle: NonNull::new(raw).ok_or(NativeError::INTERNAL_ERROR)?,
            _thread: PhantomData,
        })
    }
    pub fn core_version(&self) -> Result<u32, NativeError> {
        let mut version = 0;
        check(unsafe { ffi::runtime_core_version(self.handle.as_ptr(), &mut version) })?;
        Ok(version)
    }
}
impl Drop for NativeRuntime {
    fn drop(&mut self) {
        let mut raw = self.handle.as_ptr();
        let status = unsafe { ffi::runtime_destroy(&mut raw) };
        debug_assert_eq!(status, 0);
    }
}
