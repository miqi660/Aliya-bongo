//! Cubism Native + OpenGL C ABI；所有操作限定创建线程。
use std::{marker::PhantomData, ptr::NonNull, rc::Rc};
pub mod ffi {
    #[repr(C)]
    pub struct AliyaRuntime {
        _private: [u8; 0],
    }
    // 安全约束见 native/runtime.h；所有指针必须有效，调用限定创建线程。
    unsafe extern "C" {
        pub fn runtime_create(out: *mut *mut AliyaRuntime) -> i32;
        pub fn runtime_attach_window(
            runtime: *mut AliyaRuntime,
            hwnd: *mut std::ffi::c_void,
        ) -> i32;
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
        pub fn parameter_get(
            runtime: *mut AliyaRuntime,
            id: *const std::ffi::c_char,
            out: *mut f32,
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
        pub fn runtime_present(runtime: *mut AliyaRuntime) -> i32;
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
    pub const INVALID_STATE: Self = Self(6);
    pub const IO_ERROR: Self = Self(7);
    pub const ASSET_ERROR: Self = Self(8);
    pub const GL_ERROR: Self = Self(9);
    pub const BUSY: Self = Self(10);
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
    /// 绑定借用的 CS_OWNDC 窗口；失败后应丢弃本 Runtime。
    ///
    /// # Safety
    /// hwnd 必须由当前线程创建，并在本 Runtime 销毁前保持有效。
    /// 窗口不能与其他 Renderer 共用；调用方必须在销毁窗口前 drop Runtime。
    pub unsafe fn attach_window(&mut self, hwnd: *mut std::ffi::c_void) -> Result<(), NativeError> {
        check(unsafe { ffi::runtime_attach_window(self.handle.as_ptr(), hwnd) })
    }
    pub fn load(&mut self, path: &str) -> Result<(), NativeError> {
        let path = std::ffi::CString::new(path).map_err(|_| NativeError::INVALID_ARGUMENT)?;
        check(unsafe { ffi::model_load(self.handle.as_ptr(), path.as_ptr()) })
    }
    pub fn unload(&mut self) -> Result<(), NativeError> {
        check(unsafe { ffi::model_unload(self.handle.as_ptr()) })
    }
    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), NativeError> {
        check(unsafe { ffi::runtime_resize(self.handle.as_ptr(), width, height) })
    }
    pub fn set_parameter(&mut self, id: &str, value: f32) -> Result<(), NativeError> {
        let id = std::ffi::CString::new(id).map_err(|_| NativeError::INVALID_ARGUMENT)?;
        check(unsafe { ffi::parameter_set(self.handle.as_ptr(), id.as_ptr(), value) })
    }
    pub fn add_parameter(&mut self, id: &str, value: f32) -> Result<(), NativeError> {
        let id = std::ffi::CString::new(id).map_err(|_| NativeError::INVALID_ARGUMENT)?;
        check(unsafe { ffi::parameter_add(self.handle.as_ptr(), id.as_ptr(), value) })
    }
    pub fn parameter(&self, id: &str) -> Result<f32, NativeError> {
        let id = std::ffi::CString::new(id).map_err(|_| NativeError::INVALID_ARGUMENT)?;
        let mut value = 0.0;
        check(unsafe { ffi::parameter_get(self.handle.as_ptr(), id.as_ptr(), &mut value) })?;
        Ok(value)
    }
    pub fn start_motion(&mut self, group: &str, index: u32) -> Result<(), NativeError> {
        let group = std::ffi::CString::new(group).map_err(|_| NativeError::INVALID_ARGUMENT)?;
        check(unsafe { ffi::motion_start(self.handle.as_ptr(), group.as_ptr(), index) })
    }
    pub fn stop_motion(&mut self) -> Result<(), NativeError> {
        check(unsafe { ffi::motion_stop(self.handle.as_ptr()) })
    }
    pub fn set_expression(&mut self, id: &str) -> Result<(), NativeError> {
        let id = std::ffi::CString::new(id).map_err(|_| NativeError::INVALID_ARGUMENT)?;
        check(unsafe { ffi::expression_set(self.handle.as_ptr(), id.as_ptr()) })
    }
    pub fn update(&mut self, delta_seconds: f32) -> Result<(), NativeError> {
        check(unsafe { ffi::runtime_update(self.handle.as_ptr(), delta_seconds) })
    }
    pub fn render(&mut self) -> Result<(), NativeError> {
        check(unsafe { ffi::runtime_render(self.handle.as_ptr()) })
    }
    pub fn present(&mut self) -> Result<(), NativeError> {
        check(unsafe { ffi::runtime_present(self.handle.as_ptr()) })
    }
    pub fn is_dirty(&self) -> Result<bool, NativeError> {
        let mut value = 0;
        check(unsafe { ffi::runtime_is_dirty(self.handle.as_ptr(), &mut value) })?;
        Ok(value != 0)
    }
    pub fn is_animating(&self) -> Result<bool, NativeError> {
        let mut value = 0;
        check(unsafe { ffi::runtime_is_animating(self.handle.as_ptr(), &mut value) })?;
        Ok(value != 0)
    }
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
