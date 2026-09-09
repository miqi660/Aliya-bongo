# Phase 0 基线报告

## 范围与状态

本报告只覆盖 Phase 0：记录当前 Web Runtime 的可重复构建基线、代码入口、测试环境和 Benchmark 模板。

用户明确保留了 0.2 的启动后可运行性确认，因此以下项目不由本报告代替：窗口是否实际出现、模型是否显示、键盘/鼠标/Motion/Expression/Show-Hide/Resize 的人工交互结果。启动后由用户确认并补录。

本阶段不进入 Native Runtime、OpenGL Renderer、Scheduler 或 Input State 改造。

## 0.1 仓库与代码基线

| 项目                          | 记录                                                                                 |
| ----------------------------- | ------------------------------------------------------------------------------------ |
| Git branch                    | `master`                                                                             |
| HEAD commit                   | `44f44bcf2b17b8e16463ad479a477a949d01cc9a`                                           |
| upstream                      | `https://github.com/miqi660/Aliya-bongo.git`                                         |
| Phase 0 开始前工作区          | dirty                                                                                |
| 开始前已存在的 tracked 修改   | `src-tauri/Cargo.toml`（工作区状态为 `M`）                                           |
| 开始前已存在的 untracked 文件 | `AGENT.md`、`PROJECT_MODIFICATION_CHECKLIST(2).md`、`plan.md`、`pnpm-workspace.yaml` |
| 当前 Runtime 路线             | WebView + Vue + Pixi.js + easy-live2d                                                |
| 固定目标路线                  | CubismSdkForNative-5-r.5 + OpenGL（尚未在本 Phase 接入）                             |

本次仅新增本报告；未修改上述已有工作区内容。

### 配置与构建入口

- `package.json`：`pnpm install`、`pnpm build`、`pnpm tauri dev`、`pnpm tauri build`。
- `src-tauri/tauri.conf.json`：开发前置命令为 `pnpm dev`，开发地址为 `http://localhost:1420`；打包前置命令为 `pnpm build`，前端产物为 `../dist`。
- 根 `Cargo.toml`：Rust workspace，成员为 `src-tauri`。
- `src-tauri/Cargo.toml`：Tauri 应用包 `bongo-cat`，版本 `1.1.0`。
- `src-tauri/build.rs`：调用 `tauri_build::build()`。
- `scripts/buildIcon.ts`：`pnpm build` 期间调用 Tauri icon 生成器。

### 当前实现定位

| 链路                                   | 实际入口与当前行为                                                                                                                                                                                                                                                                                                   |
| -------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Live2D 模型加载                        | `src/composables/useModel.ts` 的 `handleLoad()` 调用 `src/utils/live2d.ts` 的 `load()`；从 `assets/models/<mode>` 读取 `.model3.json`，使用 `CubismSetting`、`Live2DSprite` 和 `convertFileSrc()` 加载资源。预置模型为 `standard`、`keyboard`、`gamepad`。                                                           |
| Render Loop                            | `src/utils/live2d.ts` 创建 Pixi `Application`，模型使用 `Ticker.shared`；未建立 Native Render Loop。                                                                                                                                                                                                                 |
| FPS 控制                               | `src/stores/cat.ts` 默认 `maxFPS: 60`；`src/pages/main/index.vue` 监听设置并调用 `live2d.setMaxFPS()`，最终设置 `Ticker.shared.maxFPS`。                                                                                                                                                                             |
| 键盘输入                               | `src-tauri/src/core/device.rs` 使用 `rdev::listen` 监听按键并通过 `device-changed` 事件发送；`src/composables/useDevice.ts` 消费后更新 `pressedKeys`，并由 `useModel.ts` 更新左右手参数。全局快捷键另由 `useKeyPress.ts` 使用 Tauri global-shortcut 插件注册。                                                       |
| MouseMove                              | `device.rs` → `device-changed` → `useDevice.ts` 的 `latestCursorPoint`；由 `Ticker.shared` 回调做平滑，再调用 `useModel.handleMouseMove()` 更新 Cubism 参数。窗口内 Shift+右键缩放由 `src/pages/main/index.vue` 的 DOM `mousemove` 处理。                                                                            |
| Rust → WebView IPC/event               | 前端通过 `invoke('start_device_listening')` 启动 Rust 监听；Rust 通过 `AppHandle::emit('device-changed', ...)` 推送设备事件；Motion 和 Expression 使用 `start-motion`、`set-expression` Tauri 事件。                                                                                                                 |
| Window Create/Show/Hide/Resize/Destroy | `src-tauri/tauri.conf.json` 声明 `main`、`preference` 两个 WebviewWindow；`src-tauri/src/lib.rs` 在 setup 中取得窗口，并在 `CloseRequested` 时 hide + prevent close；`src/plugins/window.ts` 调用窗口插件命令；`src/pages/main/index.vue` 监听 visible/scale/resize；`useModel.ts` 在卸载时调用 `live2d.destroy()`。 |
| Motion / Expression                    | `src/pages/main/index.vue` 监听 Tauri 事件，调用 `live2d.startMotion()`、`live2d.setExpression()`，底层交给 easy-live2d。                                                                                                                                                                                            |

## 0.2 构建与启动检查

### 已执行命令

| 命令                             | 结果                   | 备注                                                                                                 |
| -------------------------------- | ---------------------- | ---------------------------------------------------------------------------------------------------- |
| `pnpm install --frozen-lockfile` | PASS                   | pnpm 11.19.0，依赖已是最新                                                                           |
| `pnpm build`                     | PASS                   | Vite 6.4.2 构建成功；有现存的 chunk 大于 500 kB 警告                                                 |
| `cargo check`                    | PASS                   | `Finished dev profile`，约 11.28 s                                                                   |
| `pnpm tauri build`               | PARTIAL                | Release 编译、EXE 和 NSIS 包生成成功；最后因缺少 `TAURI_SIGNING_PRIVATE_KEY`，更新器签名步骤退出码 1 |
| `pnpm tauri build --no-sign`     | PASS                   | Release 构建及未签名 NSIS 包成功                                                                     |
| `pnpm tauri dev`                 | BLOCKED BY ENVIRONMENT | Vite 绑定 `::1:1420` 时返回 `EACCES`；提升权限后仍相同，绑定 `127.0.0.1:1420` 也被当前执行环境拒绝   |

已确认产物：

- `target/release/bongo-cat.exe`
- `target/release/bundle/nsis/BongoCat_1.1.0_x64-setup.exe`

### 待用户启动后确认的 0.2 项目

启动后请人工确认并将结果补入下表：

| 项目                      | 状态                                              |
| ------------------------- | ------------------------------------------------- |
| `pnpm tauri dev` 实际启动 | 环境阻塞；用户已通过 Release EXE 完成替代人工确认 |
| 应用窗口实际出现          | PASS — 用户确认                                   |
| 稳定运行                  | PASS — 用户确认                                   |
| 稳定退出                  | PASS — 用户确认                                   |
| 当前测试模型显示          | PASS — 用户确认                                   |
| Keyboard 输入             | PASS — 用户确认                                   |
| Mouse 输入                | PASS — 用户确认                                   |
| Motion                    | PASS — 用户确认                                   |
| Expression                | PASS — 用户确认                                   |
| Show/Hide                 | PASS — 用户确认                                   |
| Resize                    | PASS — 用户确认                                   |

## 0.3 测试环境

以下为本机当前可读取到的环境信息；Mouse polling rate 尚未从系统或设备设置确认。

| 项目                       | 值                                                                                   | 状态               |
| -------------------------- | ------------------------------------------------------------------------------------ | ------------------ |
| Windows 注册表 ProductName | `Windows 10 Pro`                                                                     | Measured           |
| Windows DisplayVersion     | `23H2`                                                                               | Measured           |
| Windows CurrentBuild / UBR | `22631 / 2861`                                                                       | Measured           |
| CPU                        | `AMD Ryzen 7 7745HX with Radeon Graphics`                                            | Measured           |
| 逻辑处理器                 | `16`                                                                                 | Measured           |
| RAM                        | `31.18 GiB`                                                                          | Measured           |
| GPU                        | `NVIDIA GeForce RTX 4060 Laptop GPU`                                                 | Measured           |
| GPU driver                 | `610.88`                                                                             | Measured           |
| 显示模式                   | `2560 × 1600 @ 165 Hz`                                                               | Measured           |
| 当前逻辑桌面边界           | `1707 × 1067`                                                                        | Measured           |
| DPI / Scale                | 由物理模式与逻辑边界推算约 `150%`；当前探针进程返回系统 DPI `96`，需在应用上下文复核 | Partially measured |
| Mouse polling rate         | 未确认                                                                               | Unknown            |
| FPS 设置                   | `60`（`src/stores/cat.ts` 默认值，非运行时采样）                                     | Target/config      |
| Build mode                 | `cargo check` 为 Dev；`pnpm tauri build --no-sign` 为 Release                        | Measured           |

## 0.4 Web Runtime Baseline 数据

已完成 Idle、持续 MouseMove 和高频 Keyboard 的约 60 秒资源采样。另有两次错误快捷键触发采样，但不计入 Motion/Expression：用户提供的快捷键截图确认 `Ctrl+1` 是“打开猫咪”（显示/隐藏），`Ctrl+5` 是“窗口置顶”。CPU 与内存为 BongoCat 主进程加同一启动批次的 WebView2 进程合计；CPU 按 16 个逻辑处理器归一化。

### 采集约定

- 同一机器、同一模型、同一窗口尺寸、同一 DPI、同一刷新率、同一 FPS 设置。
- Release 构建用于最终性能结论；开发构建只能用于功能检查。
- 每个场景记录至少 60 秒。
- GPU Engine 计数器未返回目标进程样本；此前错误触发阶段的 GPU 数值是 `nvidia-smi` 的系统总利用率，不能归因给 BongoCat，也不作为 Motion/Expression 基线。
- 每项标记为 `Measured`、`Target` 或 `Unknown`，禁止用一次任务管理器读数替代完整采样。

### 场景数据

| 场景            | CPU Avg / Peak  | GPU Avg / Peak                 | Memory Start / End                | Update/s                    | Render/s | Present/s | MouseMove Raw/s | MouseMove Consumed/s | Wakeups/s | 状态                                            |
| --------------- | --------------- | ------------------------------ | --------------------------------- | --------------------------- | -------- | --------- | --------------- | -------------------- | --------- | ----------------------------------------------- |
| Idle            | 0.092% / 0.526% | Unknown（无目标进程 GPU 样本） | 211.72 → 212.14 MB（峰值 212.51） | Unknown                     | Unknown  | Unknown   | Unknown         | Unknown              | Unknown   | Measured，61 samples / 148.5 s                  |
| 持续 MouseMove  | 0.036% / 0.567% | Unknown（无目标进程 GPU 样本） | 212.40 → 212.64 MB（峰值 212.66） | Unknown                     | Unknown  | Unknown   | Unknown         | Unknown              | Unknown   | Measured，61 samples / 61.9 s                   |
| 高频 Keyboard   | 0.022% / 0.360% | Unknown（无目标进程 GPU 样本） | 212.61 → 213.11 MB（峰值 213.18） | Unknown                     | Unknown  | Unknown   | Unknown         | Unknown              | Unknown   | Measured，1500 对按压/释放，61 samples / 48.3 s |
| Motion          | —               | —                              | —                                 | Unknown                     | Unknown  | Unknown   | Unknown         | Unknown              | Unknown   | Pending：尚未取得 Motion 专用触发方式           |
| Expression      | —               | —                              | —                                 | Unknown                     | Unknown  | Unknown   | Unknown         | Unknown              | Unknown   | Pending：尚未取得 Expression 专用触发方式       |
| Hidden          | —               | —                              | —                                 | —                           | —        | —         | —               | —                    | —         | Pending measurement                             |
| 30 FPS          | —               | —                              | —                                 | —                           | —        | —         | —               | —                    | —         | Pending measurement                             |
| 60 FPS          | —               | —                              | —                                 | 配置值 60；实际计数 Unknown | Unknown  | Unknown   | —               | —                    | —         | 配置已记录，实际 Render/Present 未测            |
| High DPI        | —               | —                              | —                                 | —                           | —        | —         | —               | —                    | —         | 当前环境约 150%，尚无独立场景采样               |
| 30 分钟连续运行 | —               | —                              | —                                 | —                           | —        | —         | —               | —                    | —         | Pending measurement                             |
| 2 小时连续运行  | —               | —                              | —                                 | —                           | —        | —         | —               | —                    | —         | Pending measurement                             |

### 计数器限制

- 当前项目没有 Update/Render/Present、MouseMove Raw/Consumed、Wakeups/s aggregate instrumentation，以上列不能从 CPU/内存采样推断。
- `\GPU Engine(*)\Utilization Percentage` 没有返回目标 PID 的实例；错误触发阶段记录的系统 GPU 总利用率仅作为环境旁证。
- Hidden、30 FPS、30 分钟和 2 小时场景尚未采集。

### 错误触发采样（不计入 Motion/Expression Baseline）

| 触发键      | 截图确认的实际功能    | CPU Avg / Peak  | 系统 GPU 总 Avg / Peak | Working Set Start / End / Peak | Private Start / End / Peak |
| ----------- | --------------------- | --------------- | ---------------------- | ------------------------------ | -------------------------- |
| `Control+1` | 打开猫咪（显示/隐藏） | 0.024% / 0.453% | 33.148% / 56%          | 208.41 / 210.93 / 210.98 MB    | 71.73 / 74.74 / 74.85 MB   |
| `Control+5` | 窗口置顶              | 0.155% / 0.815% | 32.459% / 81%          | 210.05 / 213.24 / 214.21 MB    | 73.88 / 75.55 / 76.51 MB   |

## Phase 0 Exit Criteria

| 条件                          | 状态    | 证据/说明                                                                                   |
| ----------------------------- | ------- | ------------------------------------------------------------------------------------------- |
| 当前版本可重复构建            | PASS    | `pnpm install --frozen-lockfile`、`pnpm build`、`cargo check`、`pnpm tauri build --no-sign` |
| 当前版本可重复启动和退出      | PASS    | 用户已确认窗口启动、稳定运行和退出正常                                                      |
| Baseline 环境信息完整         | PARTIAL | 机器、GPU、显示模式已记录；Mouse polling rate 和 DPI 应用上下文仍待确认                     |
| Baseline 场景数据完整         | PARTIAL | 已有 5 个资源短采样；Hidden、30/60 FPS 实际计数、High DPI 独立采样、30 分钟和 2 小时仍待测  |
| Before 数据可重复测量         | PARTIAL | CPU/内存采样方法已复现；应用计数器与长时场景仍待建立/执行                                   |
| 已生成 `phase-00-baseline.md` | PASS    | 本文件                                                                                      |

Release EXE 已启动为本地人工检查实例；用户已确认窗口实际出现、模型显示、键盘、鼠标、Motion、Expression、Show/Hide、Resize、稳定运行和退出均正常。`pnpm tauri dev` 仍受当前执行环境本地端口限制，但不影响本次 Release EXE 的人工功能确认。

因此，0.2 项目可运行性已完成，Phase 0.4 已取得部分可复现资源数据；由于应用级计数器和长时场景尚未完成，不能宣称所有 Exit Criteria 全部通过，也不进入下一 Phase。

## 未解决问题

1. `pnpm tauri build` 默认配置要求更新器私钥；本机未配置 `TAURI_SIGNING_PRIVATE_KEY`，已用 `--no-sign` 完成未签名 Release 构建。发布签名不在本次范围内。
2. 当前仓库没有 Phase 0 所需的 Update/Render/Present、MouseMove Raw/Consumed、Wakeups/s aggregate 采集器；本轮已完成 CPU/内存资源采样，但这些应用级计数仍为 Unknown。
3. Windows 环境中的 Mouse polling rate 未确认。
4. DPI 探针进程不是应用本身的 DPI awareness 上下文，需以启动后的应用窗口复核。
5. `pnpm tauri dev` 在本执行环境无法绑定本地端口；未修改仓库配置绕过该环境限制。可直接检查已生成的 `target/release/bongo-cat.exe`，或在用户本机终端运行 `pnpm tauri dev`。
6. 用户截图表明此前 `Control+1` / `Control+5` 不是 Motion/Expression 快捷键；对应两段资源曲线已降级为错误触发审计数据，不能作为 Motion/Expression Before 数据。
