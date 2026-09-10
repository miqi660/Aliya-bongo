# Phase 7 — Native / OpenGL / Cubism 资源生命周期

日期：2026-09-10。起点：`master` / `09b1e7b`（Phase 6 Native Input State 已推送）。
本阶段只处理独立 Native 验收窗口中的 Native、OpenGL、Cubism 资源所有权、释放顺序和短时压力；不进入 Phase 8 Instrumentation + Release 调优，也不声称已经接入 Tauri 生产 Native Render Loop。

## 完成内容

- `native/runtime.cpp`
  - `AliyaRuntime` 和 `Model` 禁止复制/移动，避免复制 HWND、HDC、HGLRC、Cubism 对象树或纹理句柄。
  - `Model` 析构时先停止 Motion/Expression，再在当前 OpenGL Context 中调用 `DeleteRenderer`，最后删除 Model 持有的 OpenGL Texture。
  - `AliyaRuntime` 析构时保持 Context current，依次释放 Model、Cubism Renderer/SDK Shader、Cubism Framework，再解除 current、删除 WGL Context，最后 `ReleaseDC`。
  - `model_load` 先在临时 `Model` 中完成加载，成功后才替换旧模型；加载异常由 `unique_ptr` 清理临时资源，旧模型保持可用。
- `native/validation.cpp`
  - 新增 `native_resource_lifecycle_run` 独立验收入口。
  - 使用真实 HWND/WGL/Cubism standard 模型，覆盖 Load/Unload、Show/Hide、Resize、Motion Start/Stop、Update/Render/Present、double-unload、Render-after-unload、double-destroy 和 Context current 检查。
  - 销毁后保留 500 ms 采样窗口，记录释放后的进程内存状态。
- `src-tauri/examples/native_resource_lifecycle.rs`、`src-tauri/Cargo.toml`
  - 注册带 `native-runtime` feature 的 Rust 验收入口。
- `scripts/measure-native-phase7.ps1`
  - 每 100 ms 采样 Release 验收进程的 Working Set、Private Bytes、CPU，并输出 `target/phase-07-release/metrics.json`。

## Resource Owner

| 资源                               | Owner                                                | 释放边界                                                             |
| ---------------------------------- | ---------------------------------------------------- | -------------------------------------------------------------------- |
| HWND                               | Phase 7 `Session` 验收宿主                           | `runtime_destroy` 成功后 `DestroyWindow`                             |
| HDC / HGLRC                        | `AliyaRuntime`                                       | Model、Framework 释放后解除 current、删除 Context、`ReleaseDC`       |
| Cubism Framework / Shader callback | `AliyaRuntime` 的 `option` 与 Framework 全局状态     | `CubismRenderer::StaticRelease` → `CubismFramework::Dispose/CleanUp` |
| Cubism Model / Motion / Expression | `std::unique_ptr<Model>` 及 Model 内部容器           | `Model` 析构；Motion/Expression 先停止                               |
| Cubism Renderer / FBO              | Cubism `Model` 的 Renderer，由 SDK 管理内部 GPU 对象 | `Model::~Model` 中 `DeleteRenderer`，仍在 current Context 中         |
| OpenGL Texture                     | `Model::textures`                                    | `Model::~Model` 中 `glDeleteTextures`                                |
| Scheduler                          | Phase 5 `DirtySleepController`，不属于本 Runtime     | 生产接线由调度器 Owner 停止；本独立验收不启动 Scheduler              |
| Input State                        | Phase 6 `InputState`，不属于本 Runtime               | 生产接线由 Input State Owner 停止消费；本独立验收不启动输入线程      |

窗口由验收宿主持有，Runtime 只借用 HWND；因此窗口必须在 Runtime 销毁之后才销毁。Scheduler/Input State 的边界明确，但本阶段没有把生产线程接入这个独立资源验收窗口。

## 实际销毁顺序

```text
停止/不再提交验收帧
  ↓
model_unload（Model、Renderer、Texture）
  ↓
runtime_destroy 中保持 WGL Context current
  ↓
Model reset（含 Motion/Expression、Renderer、Texture）
  ↓
CubismRenderer::StaticRelease
  ↓
CubismFramework::Dispose / CleanUp
  ↓
wglMakeCurrent(nullptr, nullptr) / wglDeleteContext
  ↓
ReleaseDC
  ↓
DestroyWindow
```

`runtime_destroy` 和 `model_unload` 都是幂等边界；验收还确认了最终 Unload 后 Update 返回 `ALIYA_INVALID_STATE`，Render/Present 不因空 Model 崩溃，重复 Destroy 不重复释放，销毁后 `wglGetCurrentContext()` 为空。

## 压力场景与复现

在仓库根目录 PowerShell 执行：

```powershell
cargo fmt --all -- --check
cargo build -p bongo-cat --example native_resource_lifecycle --features native-runtime --offline
cargo run -p bongo-cat --example native_resource_lifecycle --features native-runtime --offline -- src-tauri/assets/models/standard/cat.model3.json 2 3 3 5
cargo build -p bongo-cat --release --example native_resource_lifecycle --features native-runtime --offline
target/release/examples/native_resource_lifecycle.exe (Resolve-Path src-tauri/assets/models/standard/cat.model3.json) 100 500 500 1000
./scripts/measure-native-phase7.ps1
```

验收入口参数依次为模型路径、Load/Unload、Show/Hide、Resize、Motion 次数；脚本默认使用 `100 / 500 / 500 / 1000`。输出目录 `target/phase-07-release/` 不纳入 Git。

## 验证结果

| 检查项                          | 结果         | 实测证据                                                                                                                        |
| ------------------------------- | ------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| Debug 格式                      | PASS         | `cargo fmt --all -- --check`                                                                                                    |
| Debug 验收构建                  | PASS         | `native_resource_lifecycle`，`native-runtime` feature                                                                           |
| Debug 小压力                    | PASS         | `2 / 3 / 3 / 5`，退出码 0                                                                                                       |
| Debug 全量压力                  | PASS         | `100 / 500 / 500 / 1000`，退出码 0                                                                                              |
| Release 构建                    | PASS         | Release example 构建成功                                                                                                        |
| Release 全量压力                | PASS         | 三次独立运行均退出码 0；每次 Load/Unload=100、Show/Hide=500、Resize=500、Motion=1000；Update/Render/Present 均为 2101           |
| Model / Renderer / Texture 顺序 | PASS         | Model unload、Renderer 删除、Texture 删除均发生在 Context current 期间                                                          |
| Context / Window 顺序           | PASS         | Runtime destroy 后 current Context 为空，再 DestroyWindow 且 HWND 失效                                                          |
| 幂等与错误边界                  | PASS         | double-unload、Render-after-unload、double-destroy 断言通过                                                                     |
| Release 内存重复采样            | PASS（短时） | 三次结果均在 post-destroy 500 ms 窗口稳定：Working Set 结束 151.40/151.67/151.42 MB；Private Bytes 结束 155.46/156.06/154.55 MB |

## Release 采样记录

环境为 Windows x64 MSVC、独立 Native WGL 验收窗口、仓库 standard `cat.model3.json`、16 logical processors；PowerShell 每 100 ms 采样。以下为第三次全量运行，三次运行的内存结束值见上表。

```text
Test ID: phase7-release-resource-lifecycle-100-500-500-1000
Base: 09b1e7b
Duration: 7.814 s
Samples: 70
CPU Avg: 1.250%（按 16 logical processors 归一化）
Working Set: Start 2.73 MB；Peak 172.81 MB；End 151.42 MB
Private Bytes: Start 0.48 MB；Peak 174.41 MB；End 154.55 MB
```

Start 样本在进程初始化早期，Peak 包含模型、纹理、OpenGL/驱动首次分配；End 是销毁后 500 ms 的进程值，不等同于驱动显存，也不要求回到启动值。三次独立全量运行的 End 接近，且每次均在销毁后从 Peak 下降；这支持“本短时场景未观察到明显持续单向增长”的结论，但不替代 30 分钟、2 小时或 GPU 显存采样。

## Exit Criteria

- [x] Model、Renderer、Texture、Framework、Context 的释放顺序明确且通过独立验收。
- [x] create/destroy、load/unload 成对；double-free 边界有断言。
- [x] Release 全量短时压力通过，未观察到明显持续单向内存增长。
- [x] FFI allocation 的成功/失败边界由临时 `Model`、`unique_ptr` 和 C ABI 幂等 destroy 覆盖。
- [ ] 30 分钟 / 2 小时 Memory Trend：本阶段未执行。
- [ ] GPU 显存逐对象 Before/After：当前只有 OpenGL 错误检查和释放顺序证据，未接入显存计数器。

## 未解决事项与下一阶段

- 本阶段验证的是独立 Native example，不是 Tauri 主应用的生产 Native Render Loop；生产 Scheduler、Input State 仍按 Phase 5/6 的 Owner 边界单独接线。
- 进程 Working Set / Private Bytes 受 Windows heap、OpenGL 驱动和 SDK 缓存影响；不能把 End 高于 Start 单独判定为泄漏。
- 长时趋势、GPU 利用率/显存和 Phase 9 Before/After 回归留待后续阶段。
- 下一阶段为 Phase 8 Instrumentation + Release 调优；本阶段不在本窗口提前实现。
