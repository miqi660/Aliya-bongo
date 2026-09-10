# Phase 3 — Native Window Lifecycle

日期：2026-09-10。起点：`master` / `0d49d41`（Phase 2 Native OpenGL 验收通过）。
本阶段只处理独立 Native 验收窗口的 Create、Show、Hide、Move、Resize、Destroy 以及它与 Cubism/OpenGL Runtime 的同线程生命周期；不进入 Phase 4 Scheduler、Phase 5 Sleep 或 Phase 6 Input。

## 修改内容与范围

- `native/validation.cpp`：增加 `native_window_lifecycle_run`，在同一线程创建 `CS_OWNDC` Win32 窗口、绑定 Runtime、加载 standard 模型，并执行 Show/Hide/Move/Resize/Destroy 验收。
- `src-tauri/examples/native_window_lifecycle.rs`：注册独立 Native 验收入口；通过创建并销毁 `NativeRuntime` 强制链接 Native Bridge，再调用 C ABI 验收函数。
- `src-tauri/Cargo.toml`：注册 `native_window_lifecycle` example。
- `scripts/measure-native-phase3.ps1`：对 Release 验收进程采样 CPU、Working Set、Private Bytes，并保存 stdout/stderr 与 `metrics.json`。
- `plan.md`、`PROJECT_MODIFICATION_CHECKLIST(2).md`：记录本阶段完成项与下一阶段边界。

验收窗口不替换 Tauri 主应用的 WebView 渲染。Native Runtime 仍要求所有窗口绑定、Resize、Update、Render、Present 和 Destroy 调用发生在创建线程；窗口由验收驱动持有，Runtime 在窗口销毁前释放。

## 生命周期验收

`native_window_lifecycle_run` 的顺序为：

1. 注册窗口类并 `CreateWindowExW` 创建 `CS_OWNDC` 窗口。
2. `ShowWindow`/`SetWindowPos(SWP_SHOWWINDOW)` 显示窗口，创建 Runtime，绑定 HWND，读取 Client 像素尺寸并加载模型。
3. 执行一次 Update → Render → Present，确认 Show 后 Runtime 可工作。
4. Hide 后泵送消息；隐藏期间不调用 Update、Render 或 Present，并用计数器断言三者均未增加。
5. Re-show 后恢复一帧；Move 后断言窗口位置变化。
6. 重复 Resize；每次读取实际 Client 尺寸，调用 `runtime_resize`，断言 `dirty` 被置位，再完成一帧。
7. 重复 Show/Hide；每个隐藏段再次断言没有 Native 帧提交。
8. `model_unload` → `runtime_destroy` → 检查 current WGL Context 为空 → `DestroyWindow`，最后检查 HWND 已失效。

Runtime 的 `runtime_resize` 会更新 Renderer target size、调用 `glViewport` 并设置 `dirty = true`；`runtime_destroy` 释放 Cubism Renderer、模型纹理、Framework、WGL Context 和 DC。验收驱动保留 HWND 值，在 `DestroyWindow` 返回后再检查 `IsWindow`，避免使用已清空的 Session 字段误判。

隐藏段的停止断言属于本阶段的生命周期驱动验收：当前还没有生产 Scheduler，故不存在需要暂停的后台线程。真正的无条件 Render Loop 移除和 Scheduler 状态机属于 Phase 4；本阶段不把手动驱动器的计数结果表述为生产 Scheduler 已完成。

## 复现

在仓库根目录 PowerShell 执行，SDK 默认取仓库同级的 `CubismSdkForNative-5-r.5`：

```powershell
cargo build -p bongo-cat --example native_window_lifecycle --features native-runtime --offline
cargo run -p bongo-cat --example native_window_lifecycle --features native-runtime --offline -- src-tauri/assets/models/standard/cat.model3.json 100 100
cargo build -p bongo-cat --release --example native_window_lifecycle --features native-runtime --offline
target/release/examples/native_window_lifecycle.exe (Resolve-Path src-tauri/assets/models/standard/cat.model3.json) 500 100
./scripts/measure-native-phase3.ps1 -ResizeCycles 500 -VisibilityCycles 100
```

验收入口参数依次为模型路径、Resize 次数、Show/Hide 次数；默认值为 500 和 100。脚本输出保存在 `target/phase-03-release/`，该目录不纳入 Git。

## 验证结果

状态：Phase 3 Exit Criteria 通过；不进入 Phase 4 实现。

| 检查项                | 结果 | 实测证据                                                                                                   |
| --------------------- | ---- | ---------------------------------------------------------------------------------------------------------- |
| Debug 构建            | PASS | `cargo build -p bongo-cat --example native_window_lifecycle --features native-runtime --offline`           |
| Debug 生命周期        | PASS | Resize×100、Show/Hide×100；Update/Render/Present 各 202 次；进程退出码 0                                   |
| Release 构建          | PASS | `cargo build -p bongo-cat --release --example native_window_lifecycle --features native-runtime --offline` |
| Release 生命周期      | PASS | Resize×100、Show/Hide×100；Update/Render/Present 各 202 次；进程退出码 0                                   |
| Resize 压力           | PASS | Release Resize×500、Show/Hide×100；Update/Render/Present 各 602 次；进程退出码 0                           |
| Hidden 停止 Native 帧 | PASS | 每个隐藏段的 Update/Render/Present 计数保持不变，重新显示后恢复提交                                        |
| Move / Resize / dirty | PASS | Move 后窗口坐标变化；每次 Resize 读取有效 Client 尺寸并观察到 `dirty != 0`                                 |
| Destroy 顺序          | PASS | Model unload、Runtime destroy、current Context 为空、DestroyWindow 与 HWND 失效检查均通过                  |
| Release 采样          | PASS | 500/100，52 个样本，观测 5.849 秒；Working Set 峰值/结束 164.36 MB，Private Bytes 峰值/结束 166.08 MB      |

## Benchmark / Before / After

以下是本阶段 Release 功能验收进程的短时采样，不是性能优化前后对比，也不是长期稳定性证明：

```text
Test ID: phase3-release-window-lifecycle-500-100
Base: 0d49d41（Phase 2 已推送；本阶段工作树未提交）
Build: cargo build -p bongo-cat --release --example native_window_lifecycle --features native-runtime --offline
Scenario: Native Window Create/Show/Hide/Move/Resize/Destroy，Resize×500，Show/Hide×100
Duration: 5.849 s（PowerShell 进程采样）
Samples: 52
CPU Avg: 0.902%（按 16 logical processors 归一化）
Working Set: Start 2.73 MB；Peak/End 164.36 MB
Private Bytes: Start 0.48 MB；Peak/End 166.08 MB
```

Start 样本发生在进程启动早期，Peak/End 包含模型、纹理、OpenGL/驱动首次分配；进程在最后一个采样后很快退出，因此本结果不声称“Destroy 后进程内存已回到启动值”。在本次 500 次 Resize 的短时运行中未观察到因 Resize 每次重新创建 Runtime 或模型资源导致的无界增长；长期 30 分钟/2 小时和 GPU 显存采样仍未执行。

Before/After：不可填写。当前数据是功能验收进程采样，不是 Phase 0 基线或生产 Scheduler 的性能对比。

## 未解决事项与下一阶段

- Phase 4 负责生产 Render Scheduler、无条件 Render Loop 移除以及 Scheduler 的 Stop/Resume 状态机；本阶段的 Hidden 计数断言不替代该实现。
- 尚未把 Native Window 接入 Tauri 主应用的生产 WebView/窗口事件链；本阶段只使用独立 Native 验收窗口。
- 尚未执行 Phase 5 的 0 FPS Sleep、Phase 6 的物理键鼠输入或 Phase 7 的长期资源压力测试。
- 仅验证 Windows x64 MSVC、OpenGL、仓库 standard 模型；不承诺多窗口、多 Context 并行或第三方模型资源兼容性。
