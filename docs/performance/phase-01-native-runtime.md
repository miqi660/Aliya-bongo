# Phase 1 — Native Runtime Skeleton

日期：2026-09-09。起点：`master` / `1d27057c4e28baa9663bfb2d82cc8eb68dc86864`，开始时工作区干净。

本次完成：完成 1.1–1.4 Skeleton 压力与内存检查，并评估 Phase 1 Exit Criteria。Phase 1 Skeleton Exit Criteria 通过；不进入 Phase 2。

Phase 1.4 状态：通过评估。

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
cargo run -p bongo-cat --example native_stress --features native-runtime --offline -- 10
cargo run -p bongo-cat --example native_stress --features native-runtime --offline -- 100
cargo run -p bongo-cat --example native_stress --features native-runtime --release --offline -- 10
cargo run -p bongo-cat --example native_stress --features native-runtime --release --offline -- 100
cargo build -p bongo-cat --bins --features native-runtime --offline
cargo build -p bongo-cat --bins --features native-runtime --release --offline
pnpm build
```

内存趋势检查使用 Release 压力程序 `target/release/examples/native_stress.exe 20000 1`，每轮 create/destroy 后暂停 1 ms；外部 PowerShell 每 100 ms 采样进程 `WorkingSet64` 与 `PrivateMemorySize64`，排除前 20% 预热样本后比较四个连续区段。

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

| 检查                                                      | 实测结果                                                                        |
| --------------------------------------------------------- | ------------------------------------------------------------------------------- |
| Native C++ 编译与 `cargo check --features native-runtime` | PASS                                                                            |
| Debug `native_smoke`                                      | PASS，Core `0x06000001`，单次创建/销毁与空句柄检查通过                          |
| Release `native_smoke`                                    | PASS，Core `0x06000001`，单次创建/销毁与空句柄检查通过                          |
| Debug `native_stress` ×10 / ×100                          | PASS，Core `0x06000001`，每轮 create/destroy 与重复空句柄 destroy 检查通过      |
| Release `native_stress` ×10 / ×100                        | PASS，Core `0x06000001`，每轮 create/destroy 与重复空句柄 destroy 检查通过      |
| Release `native_stress` ×20000 + 内存趋势                 | PASS，程序完成；预热后四段 Working Set 均 `3.79 MB`、Private Bytes 均 `0.62 MB` |
| 应用本体 Debug / Release（启用 Native feature）           | PASS，生成 `target/debug/bongo-cat.exe` 与 `target/release/bongo-cat.exe`       |
| `pnpm build`                                              | PASS                                                                            |
| Rust 格式检查、`git diff --check`                         | 待本次文档更新完成后复核                                                        |

日志保存在本地 `target/native-check.log`、`target/native-debug.log`、`target/native-release.log`、`target/native-frontend-build.log`、`target/native-app-debug.log`、`target/native-app-release.log`，不纳入 Git。

`native_smoke` 每次仅创建和销毁一个 Runtime，读取 Core 版本，检查所有最小 API 对空句柄返回错误码 1，同时验证符号链接。`native_stress` 支持传入迭代次数和每轮暂停毫秒数；每轮都会验证 Rust Drop 生命周期，并通过 C ABI 验证 destroy 后句柄清空及重复 destroy 的安全行为。

前端 `pnpm build` 已通过。首次 Debug 构建遇到 Tauri 缓存 E0463（找不到 tauri crate），执行 `cargo clean -p tauri` 后重建恢复；未修改依赖业务代码。

## Phase 1.4 结果：通过评估与 Exit Criteria

- `create/destroy ×10`：Debug 与 Release 均 PASS。
- `create/destroy ×100`：Debug 与 Release 均 PASS。
- 无 crash：上述测试及 Release ×20000 均 PASS。
- 无 double free：每轮重复 destroy 已清空句柄，均返回成功；未观察到 double free 或崩溃。
- 无明显持续内存上涨：Release ×20000 共采集 188 个样本，排除 37 个预热样本后，四个连续区段的 Working Set 和 Private Bytes 均保持在 `3.79 MB` / `0.62 MB`。

| Phase 1 Exit Criterion              | 评估                                            |
| ----------------------------------- | ----------------------------------------------- |
| Rust 可稳定 create Native Runtime   | PASS，Debug/Release ×10、×100、×20000           |
| Rust 可稳定 destroy Native Runtime  | PASS，同上；Drop 与重复空句柄 destroy 均通过    |
| Debug Build 通过                    | PASS                                            |
| Release Build 通过                  | PASS                                            |
| FFI 所有权清晰                      | PASS，opaque handle、线程约束和 Drop 配对已实现 |
| 已生成 `phase-01-native-runtime.md` | PASS                                            |

结论：Phase 1 Native Runtime Skeleton 的 Exit Criteria 已通过。本结论仅覆盖当前 Skeleton 的生命周期和内存采样范围，不代表 Native 模型、OpenGL 渲染或长期 30 分钟/2 小时稳定性已经完成。

## 下一步与未验证项

- 未执行 30 分钟、2 小时连续运行及对应长期内存趋势；本次 ×20000 是短时进程级采样。
- 未替换现有 WebView 常驻渲染，不声称 Native 模型或可见窗口已运行。
- Phase 0 的应用计数器、部分场景及长时测量缺口仍保留；本次用户明确要求进入 Phase 1，未据此补写基线或声称性能提升。
- Before：无 Native Bridge。After：增加可选 C++/Rust Skeleton 和接口。CPU/GPU 性能结论仍为 Unknown；内存仅在上述 Skeleton 压力范围内观察到稳定平台，尚无优化对比。
- 下一步才进入 Phase 2，接入真实 Framework、模型和 OpenGL 生命周期。
