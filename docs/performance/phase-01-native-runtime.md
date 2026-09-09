# Phase 1 — Native Runtime Skeleton

日期：2026-09-09。起点：`master` / `1d27057c4e28baa9663bfb2d82cc8eb68dc86864`，开始时工作区干净。

本次边界：完成 1.1–1.3，在 **1.4 Skeleton 压力测试开始前停止**。Phase 1 尚未通过全部 Exit Criteria，不进入 Phase 2。

## SDK 与构建

使用用户提供的 `D:\code\Aliya-bongo\CubismSdkForNative-5-r.5`，不复制或修改 SDK。构建优先读取 `CUBISM_SDK_ROOT`，未设置时使用仓库同级的 `CubismSdkForNative-5-r.5`，代码不包含个人绝对路径。

`src-tauri/build.rs` 在启用 `native-runtime` feature 时调用 `native/build.rs`。后者用 `cc` 编译 C++17 Bridge 与 SDK Framework CPU 源文件，生成静态库，链接官方 Windows x64 / MSVC 143 Core。CRT 按 Rust 的 `crt-static` 选择 MT 或 MD；Debug 也使用对应的 Release CRT，与 cc 默认行为一致。当前 Native feature 明确只支持 Windows x64 MSVC，默认构建不要求 Native SDK。

Framework 的 Rendering 目录、Framework 初始化、Allocator、模型加载和 OpenGL 上下文属于 Phase 2，本次不接入。CPU 源码进入静态库不代表所有 Framework 模块都已执行；Core 的 `csmGetVersion` 在冒烟程序中实际调用。

复现（仓库根目录 PowerShell）：

```powershell
$env:CUBISM_SDK_ROOT = 'D:\code\Aliya-bongo\CubismSdkForNative-5-r.5'
cargo check -p bongo-cat --features native-runtime --offline
cargo run -p bongo-cat --example native_smoke --features native-runtime --offline
cargo run -p bongo-cat --example native_smoke --features native-runtime --release --offline
cargo build -p bongo-cat --bins --features native-runtime --offline
cargo build -p bongo-cat --bins --features native-runtime --release --offline
pnpm build
```

## FFI 与所有权

- `native/runtime.h` 为 C ABI 合约：opaque handle、固定宽度整数错误码、借用的 NUL 结尾 UTF-8 字符串。
- create 由 C++ 分配，destroy 接受句柄地址并清空；同一已清空变量可再次 destroy。不得销毁悬空别名或伪造地址。create 的输出位置不得覆盖尚未释放的所有权。
- 调用限定创建线程；C++ 检查 owner，Rust `NativeRuntime` 通过 `PhantomData<Rc<()>>` 禁止 Send/Sync，不实现 Clone，Drop 配对释放。
- C++ 分配和释放边界捕获异常，所有导出函数声明 noexcept；FFI 不传递异常、复杂 C++ 对象或大型纹理数据。
- 原始 `ffi` 接口为 unsafe，调用者必须满足头文件中的有效指针、线程和生命周期前提。安全 Rust 包装只暴露创建、Core 版本查询与自动销毁。

| 错误码 | 含义             |
| ------ | ---------------- |
| 0      | 成功             |
| 1      | 空指针等无效参数 |
| 2      | 非创建线程调用   |
| 3      | 当前阶段尚未实现 |
| 4      | 分配失败         |
| 5      | 内部异常         |

## 最小 API 的实际行为

`runtime_create`、`runtime_destroy`、`runtime_core_version` 可执行。空 Skeleton 的 `runtime_is_dirty` 和 `runtime_is_animating` 返回 false。

`model_load/unload`、`runtime_resize`、`parameter_set/add`、`motion_start/stop`、`expression_set`、`runtime_update/render` 已定义并导出，合法句柄返回 `ALIYA_NOT_IMPLEMENTED`。这里的“1.3 完成”指接口契约与链接完成，不表示模型和渲染功能完成；占位不会伪报成功。

## 验证

| 检查                                                      | 实测结果                                                                  |
| --------------------------------------------------------- | ------------------------------------------------------------------------- |
| Native C++ 编译与 `cargo check --features native-runtime` | PASS                                                                      |
| Debug `native_smoke`                                      | PASS，Core `0x06000001`，单次创建/销毁与空句柄检查通过                    |
| Release `native_smoke`                                    | PASS，Core `0x06000001`，单次创建/销毁与空句柄检查通过                    |
| 应用本体 Debug / Release（启用 Native feature）           | PASS，生成 `target/debug/bongo-cat.exe` 与 `target/release/bongo-cat.exe` |
| `pnpm build`                                              | PASS                                                                      |
| Rust 格式检查、`git diff --check`                         | PASS                                                                      |

日志保存在本地 `target/native-check.log`、`target/native-debug.log`、`target/native-release.log`、`target/native-frontend-build.log`、`target/native-app-debug.log`、`target/native-app-release.log`，不纳入 Git。

`native_smoke` 每次仅创建和销毁一个 Runtime，读取 Core 版本，检查所有最小 API 对空句柄返回错误码 1，同时验证符号链接。

前端 `pnpm build` 已通过。首次 Debug 构建遇到 Tauri 缓存 E0463（找不到 tauri crate），执行 `cargo clean -p tauri` 后重建恢复；未修改依赖业务代码。

## 下一步与未验证项

- 停在 1.4：未执行 create/destroy ×10、×100，未测持续内存趋势、double-free 压力或长时间稳定性；不勾选这些验收项。
- 未替换现有 WebView 常驻渲染，不声称 Native 模型或可见窗口已运行。
- Phase 0 的应用计数器、部分场景及长时测量缺口仍保留；本次用户明确要求进入 Phase 1，未据此补写基线或声称性能提升。
- Before：无 Native Bridge。After：增加可选 C++/Rust Skeleton 和接口。CPU/GPU/内存性能结论均为 Unknown，尚无优化对比。
- 下次从 1.4 开始，完成压力与内存检查后再评估 Phase 1 Exit Criteria。Phase 2 才接入真实 Framework/模型/OpenGL 生命周期。
