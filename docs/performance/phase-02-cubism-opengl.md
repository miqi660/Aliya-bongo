# Phase 2 — Cubism Native + OpenGL

日期：2026-09-09 至 2026-09-10。起点：`master` / `dfc3299cc6011acb9f4c285ae71cc2e5f1132483`。
上一阶段：Phase 1 Skeleton Exit Criteria 已通过。开始时 `docs/performance/phase-01-native-runtime.md` 已有未提交修改，本阶段保留该修改。

本窗口只处理 Phase 2，不进入 Phase 3。当前阶段最终验收结果见下方验证表。

## 修改内容与范围

通过可选 `native-runtime` feature，实现 Rust → C ABI → Cubism Native → OpenGL 的真实渲染。继续使用仓库同级的 `CubismSdkForNative-5-r.5`，也支持 `CUBISM_SDK_ROOT`；不修改 SDK。

- `native/build.rs`：编译 Framework CPU、公共 Rendering 和 OpenGL 后端；静态链接 GLEW 2.2.0、Windows OpenGL；将 SDK Standard Shader 源码嵌入生成头文件，运行时无需依赖 Shader 文件目录。
- `native/runtime.cpp`、`native/runtime.h`：Framework/Allocator、WGL Context、模型与纹理加载、Motion/Expression、参数读写、Viewport/Matrix、Render/Present、Unload/Reload/Destroy。
- `native/vendor/glew/`：SDK 下载脚本指定的官方 GLEW 2.2.0 原始源码、头文件、许可证及来源/hash。构建不进行网络下载。
- `src-tauri/src/native_runtime.rs`：新增安全的资源/参数/动画/渲染方法；窗口绑定保留为带明确生命周期前提的 unsafe 方法。
- `native/validation.cpp`、`src-tauri/examples/native_opengl.rs`：独立 Native 验收窗口、参数断言、像素读回、透明检查和生命周期测试。
- `src-tauri/examples/native_smoke.rs`：空句柄、新 ABI、无 Context 状态和 NUL 路径回归；`native_stress.rs` 更新过时注释。
- `scripts/measure-native-phase2.ps1`：对本次启动的 Release 验收进程采样 CPU、Working Set、Private Bytes，保存原始样本。
- `src-tauri/Cargo.toml`：注册 Native OpenGL example；同步 `plan.md` 和 `PROJECT_MODIFICATION_CHECKLIST(2).md`。

独立验收窗口是 Phase 2 的最小渲染宿主。Tauri 主应用仍保留现有 Web 渲染；生产窗口的 Show/Hide、输入和 Scheduler 接入属于后续阶段。

## 所有权、线程与 API

`runtime_create` 仍可无窗口创建。`runtime_attach_window` 借用当前线程的有效 `CS_OWNDC` HWND，取得 DC、创建并设为 current 的 WGL Context，再初始化 GLEW 和 Framework。窗口必须存活到 Runtime 销毁完成，不能与另一个 Renderer 共用。

Runtime 创建、窗口绑定、加载、Update、Render、Present、Resize 和 Destroy 均限创建线程。C++ 检查线程；Rust 通过 `PhantomData<Rc<()>>` 禁止 Send/Sync。SDK Framework/Shader 和 GLEW 有全局状态，本阶段明确只允许一个已绑定 Runtime；第二个绑定返回 `BUSY`。多个无窗口 Skeleton 仍可存在。

Framework 会借用 `Option` 指针，因此 Allocator 保持静态生命周期，Option 作为 Runtime 成员保存到 Framework `CleanUp` 完成。首次启动中发现并修复了临时 Option 引发的失效指针问题，最终版本已重新运行生命周期测试。

释放顺序：确保 Context current → 停止 Motion/Expression → 删除 Renderer/模型纹理/模型/动画 → 释放 SDK 全局 Shader → Framework Dispose/CleanUp → 清除 current 并删除 WGL Context → ReleaseDC → 宿主销毁 HWND。部分模型加载失败通过 RAII 释放新资源，原模型保持可用。

`runtime_render` 仅绘制；`runtime_present` 单独交换缓冲。Resize 接收物理像素，设置 Viewport 和 Renderer target size；宽画布默认按宽度适配，之后应用模型 Layout，并保持窗口变化时的比例。验收窗口使用线程级 Per-Monitor V2 DPI 上下文及 DWM 透明合成。

字符串为调用期借用的 UTF-8/NUL 字符串，文件系统读取支持 UTF-8 路径。模型按 `.model3.json` 加载 `.moc3`、PNG 纹理和全部 Motion/Expression；Moc/Motion 启用 SDK consistency validation。Motion 使用组名/index，Expression 使用 model3 中的 Name。Parameter Set/Add 作用于真实参数，未知 ID 返回错误，避免 SDK 隐式创建虚拟参数；Parameter Get 用于验证。

| 错误码 | 含义                                |
| ------ | ----------------------------------- |
| 0      | 成功                                |
| 1      | 无效参数                            |
| 2      | 错误线程                            |
| 3      | 保留的未实现状态码                  |
| 4      | 内存分配失败                        |
| 5      | 内部异常                            |
| 6      | 未绑定 Context/未加载模型等无效状态 |
| 7      | 文件 I/O 失败                       |
| 8      | 无效模型/资源                       |
| 9      | OpenGL/WGL 错误                     |
| 10     | 已有绑定 Runtime                    |

所有 C ABI 入口捕获 C++ 异常，保持 opaque handle 和明确的所有权。attach 失败后应销毁 Runtime 再重试。

## 渲染资源约束

Shader 由 SDK 首次需要时创建并缓存；纹理仅在 Model Load 时解码、上传、生成 mipmap，Unload 时释放。帧循环不读取模型 JSON/文件、不重新上传纹理、不主动重建 Framebuffer。SDK Renderer 复用其遮罩/离屏资源，尺寸变化由 SDK 处理。本阶段为源码路径核对和功能运行验证，未进行 GPU 调用拦截计数。

生产 `runtime_update/render/present` 路径没有 `glFinish` 或 `glReadPixels`。像素读回仅在独立验收驱动生成证据时调用；有限播放使用消息等待控制提交，不是生产 Scheduler。

## 复现

在仓库根目录 PowerShell 执行，SDK 默认取仓库同级目录：

```powershell
cargo check -p bongo-cat --features native-runtime --offline
cargo build -p bongo-cat --bins --examples --features native-runtime --offline
cargo build -p bongo-cat --bins --examples --features native-runtime --release --offline
cargo run -p bongo-cat --example native_smoke --features native-runtime --offline
cargo run -p bongo-cat --example native_stress --features native-runtime --offline -- 100
cargo run -p bongo-cat --example native_opengl --features native-runtime --offline
cargo run -p bongo-cat --example native_smoke --features native-runtime --release --offline
cargo run -p bongo-cat --example native_stress --features native-runtime --release --offline -- 100
./scripts/measure-native-phase2.ps1 -Seconds 20
pnpm build
```

`native_opengl` 参数依次为模型路径、输出目录和可选播放秒数。验收断言使用本仓库 standard 模型的参数、Motion 与 Expression 名称，不是任意第三方模型的通用验收器。

## 验证结果

状态：Phase 2 Exit Criteria 通过；本窗口不进入 Phase 3。

| 检查项                             | 结果 | 实测证据                                                                                                             |
| ---------------------------------- | ---- | -------------------------------------------------------------------------------------------------------------------- |
| Rust/C++ 构建                      | PASS | `cargo check -p bongo-cat --features native-runtime --offline`；Debug/Release `cargo build --bins --examples` 均通过 |
| 前端保持性构建                     | PASS | `pnpm build` 通过                                                                                                    |
| ABI 冒烟                           | PASS | Debug/Release `native_smoke`，Core `0x06000001`                                                                      |
| Runtime create/destroy             | PASS | Debug/Release `native_stress -- 100`，100/100                                                                        |
| Native + OpenGL 验收               | PASS | Debug/Release `native_opengl`，3/3 生命周期轮次                                                                      |
| 模型与透明渲染                     | PASS | 638×624 Client、DPI 144；baseline 可见像素 58,765、透明像素 338,528                                                  |
| Parameter / Motion / Expression    | PASS | `Param` 范围 0.000..1.000；`Param3` Set/Add 为 0.5；Expression 使 `Param4 > 0.9`                                     |
| Resize / Unload / Reload / Destroy | PASS | resize 像素读回通过；Unload 重复调用、重新加载各 3 轮；销毁后 current Context 为空                                   |
| 线程与错误路径                     | PASS | 跨线程 Render 返回 `WRONG_THREAD`；损坏 JSON/缺失资源/NUL 路径/NaN/未知参数均返回预期错误码                          |
| 格式与差异检查                     | PASS | `cargo fmt --all -- --check`、`git diff --check`                                                                     |

验收窗口报告的 OpenGL 为 `4.6.0 NVIDIA 610.88 / NVIDIA GeForce RTX 4060 Laptop GPU/PCIe/SSE2`。仓库保留了四张肉眼复核图：
[`baseline.png`](phase-02-images/baseline.png)、[`parameter.png`](phase-02-images/parameter.png)、[`expression.png`](phase-02-images/expression.png)、[`resize.png`](phase-02-images/resize.png)。

故意加载损坏 JSON 时 SDK 会向 stderr 输出 `Invalid Json document.`；该日志对应错误路径断言，最终模型加载和三轮验收均通过。

## Benchmark / Before / After

以下是 Release 功能验收全进程采样，属于本阶段的资源/运行证据，不是静态功耗或 Before/After 性能对比：

```text
Test ID: phase2-release-functional-20s
Commit: dfc3299cc6011acb9f4c285ae71cc2e5f1132483（工作树未提交）
Build: cargo build --release --features native-runtime --offline
Windows: Windows 10 Pro（build 未从当前权限环境确认）
CPU: 16 logical processors（型号未确认）
GPU: NVIDIA GeForce RTX 4060 Laptop GPU；OpenGL 4.6.0
Display: 验收窗口 Client 638x624；DPI 144（150%）；刷新率/鼠标 polling rate 未确认

Scenario: native_opengl，加载/参数/动作/表情/resize/重载/销毁，首轮有限播放 20 秒
Duration: 36.386 s（采样脚本观测）；有限播放 20.031 s

CPU Avg: 0.483%（按 16 logical processors 归一化）
CPU Peak: 6.434%
GPU Avg/Peak: 未测量（本阶段没有独立 GPU 利用率/显存采样）
Memory Start: Working Set 2.73 MB；Private Bytes 0.47 MB
Memory End: Working Set 176.12 MB；Private Bytes 181.16 MB
Memory Peak: Working Set 176.12 MB；Private Bytes 181.16 MB

Update Rate: 19.17/s（有限播放；与 Render/Present 各 384 次）
Render Rate: 19.17/s（有限播放）
Present Rate: 19.17/s（有限播放）
Wakeups/s: 未测量（生产 Scheduler 属于后续阶段）

Notes: 采样脚本输出保存于 target/phase-02-release/metrics.json；不将本结果表述为性能提升。
```

Before/After：不可填写。Phase 0 完整基线与生产 Scheduler 尚未完成，因此本阶段只报告 Measured 功能验收数据，不声称 CPU/GPU 性能改善。

## 未解决事项与下一阶段

- 尚未替换 Tauri 主应用常驻 Web Runtime；Phase 3 才接生产 Native Window 生命周期。
- 当前不包含物理键鼠输入、Hidden 停止工作、0 FPS Sleep、生产 FPS Scheduler 或完整 instrumentation。
- `dirty/animating` 为保守状态；持续 Expression 的 SDK 队列可能保持 animating，Sleep 语义留待 Phase 5。
- 本阶段不播放 Motion 元数据中的音频，不接 Physics/Pose/Breath 自动更新；验收覆盖计划要求的参数、Motion、Expression 和渲染。
- 仅 Windows x64 MSVC / OpenGL，纹理解码限定 PNG，且只支持一个已绑定 Runtime；不承诺多窗口/多 Context 并行。
- 未执行 Phase 7 的百次/千次压力或 30 分钟、2 小时长期测试，未测独立进程 GPU 利用率/显存。
- 实际桌面存在第三方 GPU/帧率叠加层，属于测试环境；不能将系统 GPU/功耗读数当作本进程性能结果。

当前 commit 仍为 `dfc3299cc6011acb9f4c285ae71cc2e5f1132483`，本阶段改动未自动提交。
