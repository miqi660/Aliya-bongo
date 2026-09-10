# 项目修改清单

> 基于：`https://github.com/ayangweb/BongoCat`
>
> 当前路线：**Native-first Performance**
>
> Runtime：**CubismSdkForNative-5-r.5 + OpenGL**
>
> 配套文档：`agent.md`、`plan.md`

---

## 0. 使用方法

状态约定：

- `[ ]` 未开始
- `[x]` 已完成
- `[~]` 进行中
- `BLOCKED` 阻塞
- `DEFERRED` 延后

任务执行约定：

- 一个 Phase 使用一个独立对话窗口
- 当前 Phase 未通过 Exit Criteria，不进入下一 Phase
- 每个 Phase 完成后必须生成阶段报告
- 所有性能结论必须有 Before / After
- 最终性能结论必须基于 Release Build
- 不允许跳过 Baseline 直接声明优化有效

阶段报告建议目录：

```text
docs/performance/
├─ phase-00-baseline.md
├─ phase-01-native-runtime.md
├─ phase-02-cubism-opengl.md
├─ phase-03-window-lifecycle.md
├─ phase-04-render-scheduler.md
├─ phase-05-dirty-sleep.md
├─ phase-06-input-state.md
├─ phase-07-resource-lifecycle.md
├─ phase-08-instrumentation.md
├─ phase-09-performance-regression.md
└─ phase-10-runtime-freeze.md
```

---

# Phase 0 — 建立当前 Runtime 基线

## 0.1 仓库与代码基线

- [x] 记录当前 Git branch
- [x] 记录当前 commit SHA
- [x] 确认当前工作区 clean / dirty 状态
- [x] 记录上游仓库地址
- [x] 记录当前依赖版本
- [x] 检查 `package.json`
- [x] 检查 `Cargo.toml`
- [x] 检查当前 build scripts
- [x] 确认当前 Live2D 加载入口
- [x] 确认当前渲染循环
- [x] 确认当前 FPS 控制逻辑
- [x] 确认当前 MouseMove 数据流
- [x] 确认当前 Keyboard 数据流
- [x] 确认当前 Rust → WebView IPC/event 数据流
- [x] 确认当前 Window Create/Show/Hide/Resize/Destroy

## 0.2 当前项目可运行性

- [x] `pnpm install` 成功（以 `--frozen-lockfile` 执行）
- [x] `pnpm build` 成功
- [x] `cargo check` 成功
- BLOCKED：`pnpm tauri dev` 因当前执行环境无法监听 `::1:1420` / `127.0.0.1:1420`；已用 Release EXE 完成用户人工确认
- [~] `pnpm tauri build`：Release 编译和 NSIS 包生成成功，但默认更新器签名缺少 `TAURI_SIGNING_PRIVATE_KEY`；`pnpm tauri build --no-sign` 已通过
- [x] 应用可以稳定启动（用户确认）
- [x] 应用可以稳定退出（用户确认）
- [x] 当前测试模型正常显示（用户确认）
- [x] 当前 Keyboard 输入正常（用户确认）
- [x] 当前 Mouse 输入正常（用户确认）
- [x] 当前 Motion 正常（用户确认）
- [x] 当前 Expression 正常（用户确认）
- [x] 当前 Show/Hide 正常（用户确认）
- [x] 当前 Resize 正常（用户确认）

## 0.3 测试环境记录

- [x] Windows 版本
- [x] CPU 型号
- [x] GPU 型号
- [x] RAM
- [x] 显示器分辨率
- [x] Refresh Rate
- [~] DPI / Scale（已记录约 150% 推算值，需应用上下文复核）
- [ ] Mouse Polling Rate（尚未确认）
- [x] 当前 FPS 设置
- [x] 当前 Build Mode

## 0.4 Web Runtime Baseline

测试场景（已完成项按实际采样状态同步）：

- [x] 启动后静置（约 60 秒资源采样）
- [x] 持续 MouseMove（约 60 秒资源采样）
- [x] 高频 Keyboard（1500 对按压/释放资源采样）
- [ ] Motion（专用触发方式尚未确认；此前 `Control+1` 为显示/隐藏）
- [ ] Expression（专用触发方式尚未确认；此前 `Control+5` 为窗口置顶）
- [ ] Hidden
- [ ] 30 FPS
- [~] 60 FPS（配置值已记录为 60，实际 Render/Present 未测）
- [~] High DPI（当前环境约 150%，尚无独立场景采样）
- [ ] 30 分钟连续运行
- [ ] 2 小时连续运行

每个场景至少记录：

- [x] CPU Avg
- [x] CPU Peak
- [~] GPU Avg（目标进程 GPU Engine 无样本；错误触发阶段仅有系统总利用率旁证）
- [~] GPU Peak（目标进程 GPU Engine 无样本；错误触发阶段仅有系统总利用率旁证）
- [x] Memory Start
- [x] Memory End
- [ ] Update/s
- [ ] Render/s
- [ ] Present/s
- [ ] MouseMove Raw/s
- [ ] MouseMove Consumed/s
- [ ] Wakeups/s（如当前可测）

## Phase 0 Exit Criteria

- [x] 当前版本可重复构建
- [x] 当前版本可重复启动和退出（用户确认）
- [~] Baseline 环境信息完整（Mouse Polling Rate 和 DPI 应用上下文待补）
- [~] Baseline 场景数据部分完成（已完成 5 个资源短采样；长时和应用级计数待补）
- [~] Before 数据部分可重复测量（CPU/内存采样已执行；应用级计数和长时场景待补）
- [x] 已生成 `phase-00-baseline.md`

同步说明（2026-09-09）：Phase 0.1 代码/仓库基线和 Phase 0.2 功能可运行性已完成；Phase 0.4 已完成 Idle、MouseMove、高频 Keyboard 的资源采样。此前两段 `Control+1` / `Control+5` 采样经截图确认分别是显示/隐藏和窗口置顶，已撤回其 Motion/Expression 归类；Motion/Expression 专用触发方式仍待确认。Update/Render/Present 等应用级计数、Hidden、30 FPS、独立 High DPI、30 分钟和 2 小时场景仍未完成。详细证据见 `docs/performance/phase-00-baseline.md`。

---

# Phase 1 — Native Runtime Skeleton

## 1.1 SDK 与构建

固定 SDK：

```text
CubismSdkForNative-5-r.5
```

- [x] 确认 SDK 路径配置方式
- [x] SDK 路径不写死个人绝对路径
- [x] 建立 Native C++ 目录
- [x] 建立 C++ build 配置
- [x] 接入 Cubism Framework
- [x] 接入 Cubism Core
- [x] Rust `build.rs` 接入 Native library
- [x] Debug Build 可链接
- [x] Release Build 可链接

## 1.2 FFI / Bridge

推荐：

```text
Rust
 ↓
opaque handle
 ↓
C ABI
 ↓
C++ Runtime
```

- [x] 定义 opaque Runtime Handle
- [x] `runtime_create`
- [x] `runtime_destroy`
- [x] 定义错误码
- [x] C++ exception 不跨 FFI
- [x] 明确 UTF-8 字符串编码
- [x] 明确指针所有权
- [x] create / destroy 成对
- [x] 不在 FFI 中复制大型 Texture 数据
- [x] Rust 不直接管理复杂 Cubism C++ 对象树

## 1.3 最小 Runtime API

本节勾选表示 ABI 已定义、导出并完成链接；模型/渲染操作的真实实现与验收记录见 Phase 2。

- [x] `model_load`
- [x] `model_unload`
- [x] `runtime_resize`
- [x] `parameter_set`
- [x] `parameter_add`
- [x] `motion_start`
- [x] `motion_stop`
- [x] `expression_set`
- [x] `runtime_update`
- [x] `runtime_render`
- [x] `runtime_is_dirty`
- [x] `runtime_is_animating`

当前停止点（2026-09-09）：已完成 1.1–1.3 的 Skeleton 接口与构建；按用户要求在 1.4 开始前停止。详见 `docs/performance/phase-01-native-runtime.md`。

## 1.4 Skeleton 压力测试

- [x] create/destroy × 10
- [x] create/destroy × 100
- [x] 无 crash
- [x] 无 double free
- [x] 无明显持续内存上涨

## Phase 1 Exit Criteria

- [x] Rust 可稳定 create Native Runtime
- [x] Rust 可稳定 destroy Native Runtime
- [x] Debug Build 通过
- [x] Release Build 通过
- [x] FFI 所有权清晰
- [x] 已生成 `phase-01-native-runtime.md`

---

# Phase 2 — Cubism Native + OpenGL

## 2.1 Cubism Framework

- [x] Cubism Framework Init
- [x] Allocator 正常
- [x] Model 加载框架建立
- [x] `.model3.json` 正常
- [x] `.moc3` 正常
- [x] Texture metadata 正常
- [x] Motion metadata 正常
- [x] Expression metadata 正常

## 2.2 OpenGL Context

- [x] 创建 OpenGL Context
- [x] 明确 Context 创建线程
- [x] 明确 Context Current 线程
- [x] 明确 Render 线程
- [x] 明确 Swap/Present 线程
- [x] OpenGL functions 初始化正常
- [x] Context Destroy 顺序明确

## 2.3 Cubism OpenGL Renderer

- [x] 创建 Cubism OpenGL Renderer
- [x] Shader 正常
- [x] Texture Upload 正常
- [x] Blend 正常
- [x] 透明背景正常
- [x] Viewport 正常
- [x] Model Matrix 正常
- [x] Scale 正常
- [x] Resize 正常

## 2.4 Runtime 功能

- [x] Parameter Set
- [x] Parameter Add
- [x] Motion Start
- [x] Motion Stop
- [x] Expression
- [x] Model Unload
- [x] Model Reload
- [x] Runtime Destroy

## 2.5 OpenGL 性能约束

确认不存在：

- [x] 每帧 Shader Compile
- [x] 每帧 Program Link
- [x] 每帧 Texture Upload
- [x] 每帧 Texture Decode
- [x] 每帧 JSON Parse
- [x] 每帧 Framebuffer Recreate
- [x] 每帧 GPU Resource Create
- [x] 每帧 GPU Resource Destroy
- [x] 正常 Render Path 长期 `glFinish`
- [x] 正常 Render Path 无必要 `glReadPixels`

## Phase 2 Exit Criteria

- [x] 测试模型正常显示
- [x] 透明背景正确
- [x] Parameter 正常
- [x] Motion 正常
- [x] Expression 正常
- [x] Resize 正常
- [x] Unload/Reload 正常
- [x] Destroy 正常
- [x] 已生成 `phase-02-cubism-opengl.md`

---

# Phase 3 — Native Window Lifecycle

## 3.1 Window Lifecycle

- [x] Create
- [x] Show
- [x] Hide
- [x] Move
- [x] Resize
- [x] Destroy

## 3.2 Window → Runtime 联动

- [x] Show → Wake Runtime
- [x] Resize → Update Viewport
- [x] Resize → `dirty = true`
- [x] Hide → Stop Update
- [x] Hide → Stop Render
- [x] Hide → Stop Present
- [ ] Destroy → Stop Scheduler
- [x] Destroy → Stop Render
- [x] Destroy → Release Cubism Renderer
- [x] Destroy → Release OpenGL Resources
- [x] Destroy → Release Context

## 3.3 Resize 压力测试

`CubismSdkForNative-5-r.5` 路线中 Resize 属于重点回归项。

- [x] Resize × 100
- [x] Resize × 500
- [x] 记录 Memory Before
- [x] 记录 Memory Peak
- [x] 记录 Memory After
- [ ] 检查 GPU Resource 是否持续增长
- [ ] 检查 Framebuffer 是否持续重建泄漏

## 3.4 Show/Hide 压力测试

- [x] Show/Hide × 100
- [x] Hidden 后确认 Render 停止
- [x] Hidden 后确认 Present 停止
- [x] Re-show 后恢复正常

## Phase 3 Exit Criteria

- [x] Create/Show/Hide/Resize/Destroy 稳定
- [x] Resize 无明显持续内存增长
- [x] Hidden 不继续 Native Render
- [x] 已生成 `phase-03-window-lifecycle.md`

---

# Phase 4 — Render Scheduler

## 4.1 Scheduler 状态机

目标：

```text
ACTIVE
30 / 60 FPS
   ↓
IDLE
10 FPS
   ↓
DEEP_IDLE
5 FPS
   ↓
SLEEP
0 FPS
```

- [x] 建立 `RenderScheduler`
- [x] ACTIVE
- [x] IDLE
- [x] DEEP_IDLE
- [x] SLEEP
- [x] 状态 transition 明确

## 4.2 FPS 控制

- [x] ACTIVE 60 FPS
- [x] ACTIVE 30 FPS
- [x] IDLE 10 FPS
- [x] DEEP_IDLE 5 FPS
- [x] Render Rate 受控
- [x] Present Rate 受控
- [x] Update Rate 受控

禁止：

```text
Update = 10 FPS
Present = 60 FPS
```

## 4.3 时间系统

- [x] monotonic clock
- [x] delta time 正确
- [x] abnormal large delta clamp
- [x] Sleep/Wake delta reset
- [x] 降低 FPS 不改变 Motion 实际速度

## 4.4 Scheduler Instrumentation

- [x] scheduler_wakeup_count
- [x] state_transition_count
- [ ] ACTIVE wakeups/s
- [ ] IDLE wakeups/s
- [ ] DEEP_IDLE wakeups/s

## Phase 4 Exit Criteria

- [x] ACTIVE 正常
- [x] IDLE 正常
- [x] DEEP_IDLE 正常
- [x] FPS cap 正常
- [x] Present Rate 正常
- [x] Motion 时间正常
- [x] 已生成 `phase-04-render-scheduler.md`

---

# Phase 5 — Dirty Rendering + 0 FPS Sleep

进度（2026-09-10）：已完成 Native DirtySleepController、条件变量阻塞、dirty/visibility/animation/shutdown 验收。键盘、鼠标按键和 MouseMove 仍由 Phase 6 实现；未宣称已接入 Tauri 生产 Render Loop。详见 `docs/performance/phase-05-dirty-sleep.md`。

## 5.1 Dirty State

Dirty 来源：

- [x] Parameter changed
- [x] Motion advanced
- [x] Expression changed
- [x] Resize
- [x] Model Load
- [x] Visibility changed

逻辑：

```text
dirty = true
 ↓
Render
 ↓
dirty = false
```

- [x] `runtime_is_dirty`
- [x] `runtime_is_animating`
- [x] Render 后 clear dirty
- [x] `dirty == false && !animating` 时可降级状态

## 5.2 SLEEP

目标：

```text
Update  = 0
Render  = 0
Present = 0
```

- [x] condition_variable / event / wait handle
- [x] 禁止 1ms polling
- [x] 禁止 5ms polling
- [x] 禁止 10ms polling
- [x] Render Thread 真正阻塞

## 5.3 Wake Sources

- [ ] Keyboard
- [ ] Mouse Button
- [ ] Mouse Move
- [x] Motion
- [x] Expression
- [x] Resize
- [x] Show
- [x] Shutdown

## 5.4 Sleep Instrumentation

- [x] sleep_enter_count
- [x] sleep_exit_count
- [x] Wake latency
- [x] Static wakeups/s

## Phase 5 Exit Criteria

Static 状态：

- [x] Update = 0
- [x] Render = 0
- [x] Present = 0
- [x] Scheduler Blocking Wait
- [x] 非输入 dirty 来源可以 Wake；键鼠输入留 Phase 6
- [x] Wake 后状态正确
- [x] 已生成 `phase-05-dirty-sleep.md`

---

# Phase 6 — Native Input State / 高频输入合并

## 6.1 Shared Input State

目标：

```text
Raw Input
    ↓
Input Thread
    ↓
Shared Input State
    ↓
Scheduler
    ↓
Runtime Consume
```

- [ ] 建立 Shared Input State
- [ ] latest_mouse_x
- [ ] latest_mouse_y
- [ ] Keyboard state
- [ ] Mouse button state

## 6.2 MouseMove

- [ ] MouseMove 只更新 latest state
- [ ] MouseMove 不执行 Cubism Update
- [ ] MouseMove 不调用 OpenGL
- [ ] MouseMove 不直接 Render
- [ ] Scheduler tick 消费最新坐标
- [ ] 中间 MouseMove 可丢弃

## 6.3 离散输入

- [ ] Keyboard Press 不丢
- [ ] Keyboard Release 不丢
- [ ] Mouse Left Press 不丢
- [ ] Mouse Left Release 不丢
- [ ] Mouse Right Press 不丢
- [ ] Mouse Right Release 不丢
- [ ] 多键状态正确
- [ ] 快速 Press/Release 正确
- [ ] Modifier 正确
- [ ] 不出现 stuck state

## 6.4 Input Thread 约束

- [ ] 不执行 OpenGL
- [ ] 不执行 Cubism Update
- [ ] 不做昂贵计算
- [ ] 不长时间持锁
- [ ] 不高频 IPC
- [ ] 不复制大型数据

## 6.5 高频输入 Benchmark

- [ ] 500 Hz
- [ ] 1000 Hz
- [ ] 2000 Hz（设备支持时）
- [ ] 4000 Hz（设备支持时）
- [ ] 8000 Hz（设备支持时）

记录：

- [ ] Raw MouseMove/s
- [ ] Consumed Mouse State/s
- [ ] Runtime Update/s
- [ ] Render/s
- [ ] Present/s

必须确认：

- [ ] Raw Input Rate 增长不线性放大 Update
- [ ] Raw Input Rate 增长不线性放大 Render
- [ ] Raw Input Rate 增长不线性放大 Present

## Phase 6 Exit Criteria

- [ ] MouseMove latest-state 生效
- [ ] Keyboard/Mouse Button 不丢事件
- [ ] 高频输入不放大 Runtime
- [ ] 高频输入不产生 stuck state
- [ ] 已生成 `phase-06-input-state.md`

---

# Phase 7 — Native / OpenGL / Cubism 资源生命周期

## 7.1 Resource Owner

必须明确：

- [ ] Window Owner
- [ ] Context Owner
- [ ] Cubism Runtime Owner
- [ ] Cubism Renderer Owner
- [ ] Texture Owner
- [ ] Shader Owner
- [ ] FBO Owner
- [ ] Scheduler Owner
- [ ] Input State Owner

## 7.2 RAII / Lifetime

- [ ] Model RAII
- [ ] Texture RAII
- [ ] Shader RAII
- [ ] FBO RAII
- [ ] Cubism Renderer RAII
- [ ] OpenGL Context RAII
- [ ] Runtime RAII
- [ ] create/destroy 成对
- [ ] load/unload 成对
- [ ] double-free 防护
- [ ] 异常路径资源释放

## 7.3 Destroy 顺序

建议：

```text
Stop Scheduler
 ↓
Stop Input Consumption
 ↓
Stop Render
 ↓
Release Model
 ↓
Release Cubism Renderer
 ↓
Release OpenGL Resources
 ↓
Release Context
 ↓
Destroy Window
 ↓
Destroy Runtime
```

- [ ] 不在 Context Destroy 后释放 GPU Resource

## 7.4 Stress Test

- [ ] Load/Unload × 100
- [ ] Show/Hide × 500
- [ ] Resize × 500
- [ ] Motion Start/Stop × 1000
- [ ] 记录 Memory Start
- [ ] 记录 Memory Peak
- [ ] 记录 Memory End

## Phase 7 Exit Criteria

- [ ] 无明显持续内存单向上涨
- [ ] GPU Resource 正确释放
- [ ] Context 生命周期正确
- [ ] FFI allocation 正确释放
- [ ] 已生成 `phase-07-resource-lifecycle.md`

---

# Phase 8 — Instrumentation + Release 调优

## 8.1 Runtime Instrumentation

允许统计：

- [ ] update_count
- [ ] render_count
- [ ] present_count
- [ ] input_count
- [ ] mouse_move_raw_count
- [ ] mouse_move_consume_count
- [ ] scheduler_wakeup_count
- [ ] sleep_enter_count
- [ ] sleep_exit_count

## 8.2 Logging 约束

禁止：

- [ ] MouseMove 每事件 log
- [ ] Update 每帧 log
- [ ] Render 每帧 log
- [ ] Present 每帧 log

建议：

- [ ] 每 1 秒 Aggregate Summary
- [ ] Release 可关闭或降低采样

## 8.3 Release 调优

测试：

- [ ] ACTIVE 60
- [ ] ACTIVE 30
- [ ] IDLE 10
- [ ] DEEP_IDLE 5
- [ ] SLEEP 0

调优：

- [ ] Idle Timeout
- [ ] Deep Idle Timeout
- [ ] FPS Limiter
- [ ] Wake Strategy
- [ ] Dirty Strategy
- [ ] Lock Contention
- [ ] Present Frequency
- [ ] Texture Lifetime

## Phase 8 Exit Criteria

- [ ] Release Build 性能稳定
- [ ] Instrumentation 开销可接受
- [ ] Scheduler 参数有实测依据
- [ ] 已生成 `phase-08-instrumentation.md`

---

# Phase 9 — Before / After 性能回归

## 9.1 环境一致性

必须尽量保持：

- [ ] 同一机器
- [ ] 同一 Windows 电源模式
- [ ] 同一测试模型
- [ ] 同一窗口尺寸
- [ ] 同一 DPI
- [ ] 同一 Refresh Rate
- [ ] 同一 FPS
- [ ] 同一测试时长
- [ ] Release Build

## 9.2 Native Runtime Benchmark

- [ ] Idle
- [ ] MouseMove
- [ ] 高频 Keyboard
- [ ] Motion
- [ ] Expression
- [ ] Hidden
- [ ] 30 FPS
- [ ] 60 FPS
- [ ] High DPI
- [ ] 500 Hz Mouse
- [ ] 1000 Hz Mouse
- [ ] 30 分钟
- [ ] 2 小时

## 9.3 对比指标

- [ ] CPU Before / After
- [ ] GPU Before / After
- [ ] Memory Before / After
- [ ] Update Rate Before / After
- [ ] Render Rate Before / After
- [ ] Present Rate Before / After
- [ ] Wakeups/s Before / After
- [ ] 高频输入 Before / After
- [ ] 30 分钟 Memory Trend
- [ ] 2 小时 Memory Trend

## 9.4 Performance Report

建议输出：

```text
performance/
├─ baseline/
│  └─ web-runtime.md
├─ native/
│  └─ cubism-5-r5-opengl.md
└─ report/
   ├─ comparison.md
   ├─ cpu.md
   ├─ gpu.md
   ├─ memory.md
   ├─ input.md
   └─ long-run.md
```

## Phase 9 Exit Criteria

- [ ] Before 数据完整
- [ ] After 数据完整
- [ ] 环境一致
- [ ] 比较结果完整
- [ ] Known Issues 完整
- [ ] 已生成 `phase-09-performance-regression.md`

---

# Phase 10 — Runtime Freeze

## 10.1 Build Gate

- [ ] `cargo check`
- [ ] Native C++ compile
- [ ] Debug Build
- [ ] Release Build
- [ ] Release 实机稳定启动

## 10.2 Cubism Gate

- [ ] Runtime Init
- [ ] Model Load
- [ ] Texture
- [ ] Parameter
- [ ] Motion
- [ ] Expression
- [ ] Resize
- [ ] Unload
- [ ] Reload
- [ ] Destroy

## 10.3 OpenGL Gate

- [ ] Context Lifecycle
- [ ] Texture Lifecycle
- [ ] Shader Lifecycle
- [ ] FBO Lifecycle
- [ ] Resize 无持续泄漏
- [ ] 正常路径无长期 `glFinish`
- [ ] 无无意义 GPU/CPU Sync

## 10.4 Scheduler Gate

- [ ] ACTIVE
- [ ] IDLE
- [ ] DEEP_IDLE
- [ ] SLEEP
- [ ] Wake
- [ ] FPS Cap
- [ ] Static 无 Busy Loop

## 10.5 Input Gate

- [ ] Keyboard Press/Release
- [ ] Mouse Button
- [ ] MouseMove Latest State
- [ ] 500 Hz
- [ ] 1000 Hz
- [ ] Polling Rate 不线性放大 Update
- [ ] Polling Rate 不线性放大 Render
- [ ] Polling Rate 不线性放大 Present

## 10.6 Hidden Gate

必须达到：

```text
Update  = 0
Render  = 0
Present = 0
```

- [ ] PASS

## 10.7 Static / Sleep Gate

必须达到：

```text
Update  = 0
Render  = 0
Present = 0
Scheduler = blocking wait
```

- [ ] Input Wake PASS
- [ ] Wake Latency 可接受

## 10.8 Stability Gate

- [ ] 多次启动 / 退出
- [ ] Show / Hide
- [ ] Load / Unload
- [ ] Resize
- [ ] 高频 Input
- [ ] High DPI
- [ ] 30 分钟
- [ ] 2 小时

## 10.9 Performance Report Gate

- [ ] Web Runtime Baseline 完整
- [ ] Native Runtime Baseline 完整
- [ ] CPU Comparison
- [ ] GPU Comparison
- [ ] Memory Comparison
- [ ] Idle Comparison
- [ ] Hidden Comparison
- [ ] Input Comparison
- [ ] 30 min Trend
- [ ] 2 h Trend
- [ ] Environment
- [ ] Known Issues

全部通过后：

```text
CubismSdkForNative-5-r.5
          +
        OpenGL
          +
  Native Scheduler
          +
 Native Input State
          ↓
      RUNTIME FREEZE
```

- [ ] 标记 Runtime Freeze
- [ ] 生成 `phase-10-runtime-freeze.md`

---

# 修改范围快速表

| 模块                        | 当前策略                           |   优先级 |
| --------------------------- | ---------------------------------- | -------: |
| CubismSdkForNative-5-r.5    | 核心 Runtime                       |       P0 |
| OpenGL Renderer             | 核心 Renderer                      |       P0 |
| Rust ↔ C ABI ↔ C++ Bridge   | 核心修改                           |       P0 |
| Native Window Lifecycle     | 核心修改                           |       P0 |
| Render Scheduler            | 核心修改                           |       P0 |
| Dirty Rendering             | 核心修改                           |       P0 |
| Sleep / Wakeup              | 核心修改                           |       P0 |
| Shared Input State          | 核心修改                           |       P0 |
| MouseMove Coalescing        | 核心修改                           |       P0 |
| OpenGL Resource Lifecycle   | 核心修改                           |       P0 |
| Benchmark / Instrumentation | 必须                               |       P0 |
| Web Runtime                 | Before 基线 / 逐步退出常驻渲染路径 |       P1 |
| Build Scripts               | 按 Native Runtime 需要修改         |       P1 |
| Tauri Application Core      | 按 Native Window 接入需要修改      |       P1 |
| 与性能无关代码              | 当前不修改                         | DEFERRED |

---

# 每个 PR / Agent Task 的完成定义

每项修改完成时至少回答：

- [ ] 当前属于哪个 Phase？
- [ ] 这次修改解决了什么性能问题？
- [ ] 修改了哪些文件？
- [ ] 是否修改 FFI？
- [ ] 是否修改 OpenGL Context / Resource Lifecycle？
- [ ] 是否修改 Scheduler？
- [ ] 是否修改 Input State？
- [ ] Build 是否通过？
- [ ] Debug Runtime 是否验证？
- [ ] Release Runtime 是否验证？
- [ ] 是否有 Before / After？
- [ ] 测试环境是否一致？
- [ ] CPU 是否有变化？
- [ ] GPU 是否有变化？
- [ ] Memory 是否有变化？
- [ ] Update/Render/Present Rate 是否有变化？
- [ ] Wakeups/s 是否有变化？
- [ ] 是否新增资源泄漏风险？
- [ ] 是否新增线程同步风险？
- [ ] 当前 Phase Exit Criteria 是否全部通过？
- [ ] 是否更新阶段报告？
- [ ] 是否允许进入下一 Phase？

---

# 每个 Phase 的窗口规则

每个新窗口第一条消息建议：

```text
当前进入 Phase X：<Phase 名称>。

请严格遵守仓库中的 agent.md、plan.md 和 PROJECT_MODIFICATION_CHECKLIST.md。

当前状态：
- 上一阶段：Phase X-1 已完成
- 当前 branch：<branch>
- 当前 commit：<commit>
- Cubism SDK：CubismSdkForNative-5-r.5
- Renderer：OpenGL

本窗口只完成 Phase X。
不要进入下一 Phase。

请先读取：
1. agent.md
2. plan.md
3. PROJECT_MODIFICATION_CHECKLIST.md
4. 上一阶段 phase report
5. 当前实现

完成后必须输出：
- 修改内容
- 修改文件
- Build 结果
- Runtime 验证
- Benchmark Before / After
- 未解决问题
- Exit Criteria
- 当前 commit
- 是否允许进入下一 Phase
```

---

# Native-first 性能阶段 Done

只有以下全部完成，才标记整个 Native-first 性能阶段完成：

- [ ] **Web Runtime Baseline 已固定**
- [ ] **CubismSdkForNative-5-r.5 Runtime 已跑通**
- [ ] **OpenGL Renderer 已稳定**
- [ ] **Native Window Lifecycle 已稳定**
- [ ] **ACTIVE / IDLE / DEEP_IDLE / SLEEP Scheduler 已完成**
- [ ] **Dirty Rendering 已完成**
- [ ] **Static 0 FPS Sleep 已完成**
- [ ] **Hidden Update/Render/Present = 0**
- [ ] **MouseMove Latest-State 已完成**
- [ ] **高 polling rate 输入不会线性放大 Runtime**
- [ ] **Native/OpenGL/Cubism Resource Lifecycle 已验证**
- [ ] **30 分钟稳定性通过**
- [ ] **2 小时稳定性通过**
- [ ] **Release Before / After 性能报告完成**
- [ ] **Runtime Freeze Gate 全部通过**

最终状态：

> **Native-first Runtime Freeze**
