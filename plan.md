# plan.md

# Native-first 性能优化计划

## 0. 技术路线

本计划只覆盖 **Native-first 性能优化阶段**。

固定技术路线：

```text
Rust Application Core
        ↓
Native Window
        ↓
Rust ↔ C ABI ↔ C++
        ↓
CubismSdkForNative-5-r.5
        ↓
OpenGL
```

输入链路固定为：

```text
Input Producer
      ↓
Shared Input State
      ↓
Render Scheduler
      ↓
Cubism Update
      ↓
OpenGL Render
```

禁止：

```text
Input Event
  ↓
直接 Cubism Update
  ↓
直接 Render
```

目标：

- 降低 CPU 占用
- 降低 GPU 占用
- 降低后台功耗
- 降低线程 Wakeup
- 避免高 polling rate 输入放大 Runtime Update / Render
- Hidden 状态停止 Update / Render / Present
- Static 状态进入真正 Sleep
- 保证 Native / OpenGL / Cubism 资源正确释放
- 建立可重复 Benchmark
- 最终完成 Runtime Freeze

---

# 1. 阶段执行规则

## 1.1 一个 Phase 一个新窗口

每完成一个 Phase，必须新开一个对话窗口。

```text
窗口 1  → Phase 0
窗口 2  → Phase 1
窗口 3  → Phase 2
窗口 4  → Phase 3
窗口 5  → Phase 4
窗口 6  → Phase 5
窗口 7  → Phase 6
窗口 8  → Phase 7
窗口 9  → Phase 8
窗口 10 → Phase 9
窗口 11 → Phase 10
```

规则：

- 一个窗口只处理一个 Phase
- 不跨阶段顺手修改
- Phase 未达到 Exit Criteria，不得进入下一窗口
- 每个 Phase 完成后必须生成阶段报告
- 下一窗口必须基于上一阶段报告继续

## 1.2 每个新窗口必须提供

```text
当前 Phase：
上一阶段状态：
当前 Git branch：
当前 commit：
Cubism SDK：CubismSdkForNative-5-r.5
Renderer：OpenGL
```

并明确：

```text
本窗口只完成当前 Phase。
不要进入下一 Phase。
```

## 1.3 每个阶段完成后输出

至少包含：

- 修改了什么
- 为什么修改
- 修改文件
- Build 结果
- Runtime 验证结果
- Benchmark 结果
- Before / After
- 未解决问题
- Exit Criteria 是否全部通过
- 当前 commit
- 是否允许进入下一 Phase
- 同步PROJECT_MODIFICATION_CHECKLIST(2).md任务清单进度

建议阶段报告路径：

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

# 2. 总体阶段顺序

```text
Phase 0  可重复运行基线
 ↓
Phase 1  Native Runtime Skeleton
 ↓
Phase 2  Cubism Native + OpenGL
 ↓
Phase 3  Native Window Lifecycle
 ↓
Phase 4  Render Scheduler
 ↓
Phase 5  Dirty Rendering + 0 FPS Sleep
 ↓
Phase 6  Native Input State / 高频输入合并
 ↓
Phase 7  Native / OpenGL / Cubism 资源生命周期
 ↓
Phase 8  Instrumentation + Release 调优
 ↓
Phase 9  Before / After 性能回归
 ↓
Phase 10 Runtime Freeze
```

---

# 3. Phase 0 — 可重复运行基线

## 目标

固定当前 Runtime 作为 Before 基线。

## 任务

- [ ] 记录当前 Git branch
- [ ] 记录当前 commit SHA
- [ ] 检查 `package.json`
- [ ] 检查 `Cargo.toml`
- [ ] 确认当前构建命令
- [ ] 定位 Live2D 模型加载入口
- [ ] 定位当前 Render Loop
- [ ] 定位 FPS 控制
- [ ] 定位键盘输入路径
- [ ] 定位 MouseMove 路径
- [ ] 定位 Rust → WebView IPC/event
- [ ] 定位 Window Create/Show/Hide/Resize/Destroy
- [ ] `pnpm install`
- [ ] `pnpm build`
- [ ] `cargo check`
- [ ] `pnpm tauri dev`
- [ ] `pnpm tauri build`
- [ ] 验证稳定启动
- [ ] 验证稳定退出
- [ ] 验证现有模型
- [ ] 验证键盘输入
- [ ] 验证鼠标输入
- [ ] 记录 Windows 版本
- [ ] 记录 CPU / GPU / RAM
- [ ] 记录分辨率 / Refresh Rate / DPI
- [ ] 记录 Mouse Polling Rate
- [ ] 建立 Benchmark 模板

## Baseline 场景

- [ ] Idle
- [ ] 持续 MouseMove
- [ ] 高频 Keyboard
- [ ] Motion
- [ ] Expression
- [ ] Hidden
- [ ] 30 FPS
- [ ] 60 FPS
- [ ] High DPI
- [ ] 30 分钟
- [ ] 2 小时

记录：

```text
CPU Avg / Peak
GPU Avg / Peak
Memory Start / End
Update/s
Render/s
Present/s
MouseMove Raw/s
MouseMove Consumed/s
```

## Exit Criteria

- [ ] 当前版本可以稳定构建
- [ ] 可以稳定启动 / 运行 / 退出
- [ ] Baseline 数据完整
- [ ] 环境信息完整
- [ ] Before 数据可重复测量

完成后生成：

```text
docs/performance/phase-00-baseline.md
```

---

# 4. Phase 1 — Native Runtime Skeleton

进度（2026-09-09）：1.1–1.4 的构建、ABI、create/destroy 压力和短时内存趋势检查已完成；Debug/Release ×10、×100 及 Release ×20000 均通过，Phase 1 Skeleton Exit Criteria 已评估通过。详见 `docs/performance/phase-01-native-runtime.md`。

## 目标

跑通：

```text
Rust
 ↓
C ABI
 ↓
C++ Native Runtime
```

## 任务

- [x] 建立 `native/` Runtime 目录
- [x] 建立 C++ build
- [x] 接入 `CubismSdkForNative-5-r.5`
- [x] Rust `build.rs` 链接 Native library
- [x] 定义 opaque runtime handle
- [x] `runtime_create`
- [x] `runtime_destroy`
- [x] 定义统一错误码
- [x] 禁止 C++ exception 穿过 FFI
- [x] 明确 UTF-8 字符串
- [x] 明确指针所有权
- [x] create / destroy 成对
- [x] 连续 create / destroy 100 次
- [x] Debug Build
- [x] Release Build

## 推荐 FFI 最小 API

```text
runtime_create
runtime_destroy
model_load
model_unload
runtime_resize
parameter_set
parameter_add
motion_start
motion_stop
expression_set
runtime_update
runtime_render
runtime_is_dirty
runtime_is_animating
```

## Exit Criteria

- [x] Rust 可稳定创建 Runtime
- [x] Rust 可稳定销毁 Runtime
- [x] Debug / Release 均可链接
- [x] create/destroy 压力测试无异常
- [x] 无明显内存持续上涨

完成后生成：

```text
docs/performance/phase-01-native-runtime.md
```

---

# 5. Phase 2 — Cubism Native + OpenGL

## 目标

使用 `CubismSdkForNative-5-r.5 + OpenGL` 完成最小 Native Live2D 渲染。

## Cubism 任务

- [x] Cubism Framework Init
- [x] Allocator
- [x] Model Load
- [x] `.model3.json`
- [x] `.moc3`
- [x] Texture
- [x] Motion
- [x] Expression
- [x] Parameter Set/Add

## OpenGL 任务

- [x] 创建 OpenGL Context
- [x] 明确 Context Current Thread
- [x] 初始化 OpenGL functions
- [x] Cubism OpenGL Renderer
- [x] Shader
- [x] Texture Upload
- [x] Blend
- [x] 透明背景
- [x] Viewport
- [x] Model Matrix
- [x] Resize

## 性能约束

禁止：

- [x] 每帧 Shader Compile
- [x] 每帧 Program Link
- [x] 每帧 Texture Upload
- [x] 每帧 JSON Parse
- [x] 每帧 Framebuffer Recreate
- [x] 每帧 GPU Resource Create/Destroy
- [x] 正常路径长期 `glFinish`
- [x] 正常路径无必要 `glReadPixels`

## Exit Criteria

- [x] 模型正常显示
- [x] Texture 正常
- [x] Parameter 正常
- [x] Motion 正常
- [x] Expression 正常
- [x] Resize 正常
- [x] Unload 正常
- [x] Reload 正常
- [x] Destroy 正常

完成后生成：

```text
docs/performance/phase-02-cubism-opengl.md
```

状态：已生成并完成 Phase 2 Exit Criteria 验收；不进入 Phase 3。

---

# 6. Phase 3 — Native Window Lifecycle

## 目标

让 Native Window 生命周期直接驱动 OpenGL Runtime。

## 生命周期

```text
Create → OpenGL Context → Cubism Runtime
Show   → Wake
Resize → Viewport → Dirty
Hide   → Stop Render
Destroy → Stop Scheduler → Release Cubism → Release OpenGL → Release Context → Destroy Window
```

## 任务

- [x] Window Create
- [x] Show
- [x] Hide
- [x] Move
- [x] Resize
- [x] Destroy
- [x] Show → Wake
- [x] Resize → dirty
- [x] Resize → viewport update
- [x] Hide → stop Update
- [x] Hide → stop Render
- [x] Hide → stop Present
- [ ] Destroy → stop Scheduler
- [x] Destroy → release Renderer
- [x] Destroy → release OpenGL Resources
- [x] Destroy → release Context

## Stress Test

- [x] Show/Hide × 100
- [x] Resize × 100
- [x] Resize × 500
- [x] 记录 Memory Before/After

## Exit Criteria

- [x] Show/Hide 稳定
- [x] Resize 稳定
- [x] Destroy 稳定
- [x] Resize 无明显持续内存增长
- [x] Hidden 不继续 Native Render

完成后生成：

```text
docs/performance/phase-03-window-lifecycle.md
```

---

# 7. Phase 4 — Render Scheduler

## 目标

彻底取消无条件 Render Loop。

## 状态机

```text
ACTIVE       30 / 60 FPS
   ↓
IDLE         10 FPS
   ↓
DEEP_IDLE     5 FPS
   ↓
SLEEP         0 FPS
```

## 任务

- [ ] 建立 `RenderScheduler`
- [ ] ACTIVE
- [ ] IDLE
- [ ] DEEP_IDLE
- [ ] SLEEP
- [ ] ACTIVE FPS limiter
- [ ] Idle timeout
- [ ] Deep Idle timeout
- [ ] Sleep condition
- [ ] monotonic clock
- [ ] delta time
- [ ] large delta clamp
- [ ] Sleep → Wake delta reset
- [ ] Motion speed 不受 FPS 变化影响
- [ ] Scheduler transition counter
- [ ] Scheduler wakeup counter

## 必须确认

```text
Update Rate
Render Rate
Present Rate
```

均受 Scheduler 约束。

禁止：

```text
Update = 10 FPS
Present = 60 FPS
```

## Exit Criteria

- [ ] ACTIVE 正常
- [ ] IDLE 正常
- [ ] DEEP_IDLE 正常
- [ ] Render FPS 受控
- [ ] Present FPS 受控
- [ ] Motion 时间正确
- [ ] Wakeup/s 可测量

完成后生成：

```text
docs/performance/phase-04-render-scheduler.md
```

---

# 8. Phase 5 — Dirty Rendering + 0 FPS Sleep

## 目标

没有变化就不 Render，完全静止时真正阻塞等待。

## Dirty 来源

- [ ] Parameter changed
- [ ] Motion advanced
- [ ] Expression changed
- [ ] Resize
- [ ] Model Load
- [ ] Visibility changed

逻辑：

```text
dirty = true
 ↓
Render
 ↓
dirty = false
```

如果：

```text
dirty == false && !animating
```

则允许进入 Sleep。

## Sleep 目标

```text
Update  = 0
Render  = 0
Present = 0
```

## 任务

- [ ] dirty flag
- [ ] `runtime_is_dirty`
- [ ] `runtime_is_animating`
- [ ] Render 后 clear dirty
- [ ] condition_variable / event / wait handle
- [ ] Keyboard Wake
- [ ] Mouse Button Wake
- [ ] Mouse Move Wake
- [ ] Motion Wake
- [ ] Resize Wake
- [ ] Show Wake
- [ ] Shutdown Wake
- [ ] 删除 polling sleep loop
- [ ] sleep_enter_count
- [ ] sleep_exit_count
- [ ] Wake latency 测试

## Exit Criteria

Static 状态：

```text
Update  = 0
Render  = 0
Present = 0
```

- [ ] Scheduler 真正阻塞等待
- [ ] 输入可正常 Wake
- [ ] Wake 后状态正确

完成后生成：

```text
docs/performance/phase-05-dirty-sleep.md
```

---

# 9. Phase 6 — Native Input State / 高频输入合并

## 目标

高 polling rate 输入只更新状态，不直接放大 Runtime Update / Render。

## 架构

```text
Raw Input
    ↓
Input Thread
    ↓
Shared Input State
    ↓
Scheduler Wake
    ↓
Render Thread Consume
```

## MouseMove

只保存：

```text
latest_x
latest_y
```

## 任务

- [ ] 建立 Shared Input State
- [ ] latest mouse X
- [ ] latest mouse Y
- [ ] MouseMove 只覆盖最新状态
- [ ] MouseMove 不执行 Cubism Update
- [ ] MouseMove 不调用 OpenGL
- [ ] MouseMove 不直接 Render
- [ ] Render/Update tick 消费最新状态
- [ ] raw mouse counter
- [ ] consumed mouse counter
- [ ] Keyboard Press
- [ ] Keyboard Release
- [ ] Mouse Left Press/Release
- [ ] Mouse Right Press/Release
- [ ] 多键并发
- [ ] 快速 Press/Release
- [ ] 修饰键
- [ ] stuck state 检查

## Benchmark

- [ ] 500 Hz
- [ ] 1000 Hz
- [ ] 更高 polling rate（设备支持时）

必须确认：

```text
Raw Input Rate ↑
```

不会导致：

```text
Update Rate
Render Rate
Present Rate
```

线性上升。

## Exit Criteria

- [ ] MouseMove latest-state 生效
- [ ] 离散按键事件不丢
- [ ] 高频输入不放大 Runtime
- [ ] 高频输入不导致 stuck state

完成后生成：

```text
docs/performance/phase-06-input-state.md
```

---

# 10. Phase 7 — Resource Lifecycle

## 目标

消除 Native / OpenGL / Cubism 长时间资源泄漏。

## Owner 必须明确

- [ ] Window Owner
- [ ] Context Owner
- [ ] Cubism Runtime Owner
- [ ] Renderer Owner
- [ ] Texture Owner
- [ ] Scheduler Owner
- [ ] Input State Owner

## 推荐释放顺序

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
Release OpenGL Context
 ↓
Destroy Window
 ↓
Destroy Runtime
```

## 任务

- [ ] Model RAII
- [ ] Texture RAII
- [ ] Shader RAII
- [ ] FBO RAII
- [ ] Cubism Renderer RAII
- [ ] Context RAII
- [ ] Runtime RAII
- [ ] create/destroy 成对
- [ ] load/unload 成对
- [ ] double free 防护
- [ ] destroy-after-context 防护
- [ ] 异常路径释放

## Stress Test

- [ ] Load/Unload × 100
- [ ] Show/Hide × 500
- [ ] Resize × 500
- [ ] Motion Start/Stop × 1000

记录：

```text
Memory Start
Memory Peak
Memory End
```

## Exit Criteria

- [ ] 无明显持续内存单向上涨
- [ ] Context 销毁顺序正确
- [ ] GPU Resource 正确释放
- [ ] FFI allocation 正确释放

完成后生成：

```text
docs/performance/phase-07-resource-lifecycle.md
```

---

# 11. Phase 8 — Instrumentation + Release 调优

## 目标

建立低开销性能观测，并在 Release Build 中调优。

## Instrumentation

- [ ] update_count
- [ ] render_count
- [ ] present_count
- [ ] input_count
- [ ] mouse_move_raw_count
- [ ] mouse_move_consume_count
- [ ] scheduler_wakeup_count
- [ ] sleep_enter_count
- [ ] sleep_exit_count

禁止：

```text
MouseMove 每事件 log
Render 每帧 log
Update 每帧 log
```

建议：

```text
每 1 秒输出一次 aggregate summary
```

## Release 调优

测试：

- [ ] ACTIVE 60 FPS
- [ ] ACTIVE 30 FPS
- [ ] IDLE 10 FPS
- [ ] DEEP_IDLE 5 FPS
- [ ] SLEEP 0 FPS

调优：

- [ ] idle timeout
- [ ] deep idle timeout
- [ ] FPS limiter
- [ ] Wake strategy
- [ ] Dirty strategy
- [ ] Lock contention
- [ ] Present frequency
- [ ] Texture lifetime

## Exit Criteria

- [ ] Release Build 性能数据稳定
- [ ] Instrumentation 本身开销可忽略
- [ ] Scheduler 参数有实测依据

完成后生成：

```text
docs/performance/phase-08-instrumentation.md
```

---

# 12. Phase 9 — Before / After 性能回归

## 目标

使用与 Phase 0 相同环境重新执行完整 Benchmark。

## 必须保持一致

- [ ] 同一机器
- [ ] 同一 Windows 电源模式
- [ ] 同一模型
- [ ] 同一窗口尺寸
- [ ] 同一 DPI
- [ ] 同一 Refresh Rate
- [ ] 同一 FPS
- [ ] 同一测试时长
- [ ] Release Build

## Native 测试

- [ ] Idle
- [ ] MouseMove
- [ ] 高频 Keyboard
- [ ] Motion
- [ ] Expression
- [ ] Hidden
- [ ] 30 FPS
- [ ] 60 FPS
- [ ] High DPI
- [ ] 500 Hz
- [ ] 1000 Hz
- [ ] 30 分钟
- [ ] 2 小时

## 对比指标

- [ ] CPU Before / After
- [ ] GPU Before / After
- [ ] Memory Before / After
- [ ] Update Rate Before / After
- [ ] Render Rate Before / After
- [ ] Present Rate Before / After
- [ ] Wakeups/s Before / After
- [ ] 高频输入 Before / After
- [ ] 30 min Memory Trend
- [ ] 2 h Memory Trend

## Exit Criteria

- [ ] Before 数据完整
- [ ] After 数据完整
- [ ] 环境一致
- [ ] 对比报告完整
- [ ] Known Issues 完整

完成后生成：

```text
docs/performance/phase-09-performance-regression.md
```

---

# 13. Phase 10 — Runtime Freeze

## 目标

完成 Native-first 性能阶段最终验收。

## Build Gate

- [ ] `cargo check`
- [ ] Native C++ compile
- [ ] Debug Build
- [ ] Release Build
- [ ] Release 实机稳定启动

## Cubism Native Gate

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

## OpenGL Gate

- [ ] Context 生命周期正确
- [ ] Texture 生命周期正确
- [ ] Shader 生命周期正确
- [ ] FBO 生命周期正确
- [ ] Resize 无持续泄漏
- [ ] 无长期 `glFinish`
- [ ] 无无意义 GPU/CPU sync

## Scheduler Gate

- [ ] ACTIVE
- [ ] IDLE
- [ ] DEEP_IDLE
- [ ] SLEEP
- [ ] Wake
- [ ] FPS cap
- [ ] Static 无 busy loop

## Input Gate

- [ ] Keyboard Press/Release
- [ ] Mouse Button
- [ ] MouseMove latest-state
- [ ] 500 Hz
- [ ] 1000 Hz
- [ ] polling rate 不线性放大 Update
- [ ] polling rate 不线性放大 Render

## Hidden Gate

必须达到：

```text
Update  = 0
Render  = 0
Present = 0
```

- [ ] PASS

## Static / Sleep Gate

必须达到：

```text
Update  = 0
Render  = 0
Present = 0
Scheduler = blocking wait
```

- [ ] Input Wake PASS

## Stability Gate

- [ ] 多次启动/退出
- [ ] 多次 Show/Hide
- [ ] 多次 Load/Unload
- [ ] 多次 Resize
- [ ] 高频输入
- [ ] 30 分钟
- [ ] 2 小时
- [ ] High DPI

## Performance Report Gate

- [ ] Web Runtime Baseline
- [ ] Native Runtime Baseline
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

全部通过：

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

完成后生成：

```text
docs/performance/phase-10-runtime-freeze.md
```

---

# 14. 每个新窗口的固定开场模板

```text
当前进入 Phase X：<Phase 名称>。

请严格遵守仓库中的 agent.md 和 plan.md。

当前状态：
- 上一阶段：Phase X-1 已完成
- 当前 branch：<branch>
- 当前 commit：<commit>
- Cubism SDK：CubismSdkForNative-5-r.5
- Renderer：OpenGL

本窗口只完成 Phase X。
不要进入下一 Phase。

请先读取当前实现与上一阶段报告，再开始修改。

完成后必须输出：
1. 修改内容
2. 修改文件
3. Build 结果
4. Runtime 验证结果
5. Benchmark Before / After
6. 未解决问题
7. Exit Criteria 检查
8. 当前 commit
9. 是否允许进入下一 Phase
```

---

# 15. 最终执行原则

每个 Phase 必须做到：

```text
小步
可编译
可运行
可回滚
可 Benchmark
可验收
```

不得：

- 跨 Phase 修改
- 未完成 Exit Criteria 就进入下一阶段
- 没有 Baseline 就声称性能提升
- 用 Debug Build 得出最终性能结论
- 用一次 Task Manager 截图代替 Benchmark
- 用 busy loop 换取低延迟
- 让 MouseMove polling rate 直接决定 Render Rate
- Hidden 后继续 Draw / Present
- Static 后继续周期性 Render
- 依赖进程退出清理 Native / OpenGL / Cubism Resource

最终完成标准：

> **CubismSdkForNative-5-r.5 + OpenGL Runtime 通过完整 Benchmark、稳定性测试和 Runtime Freeze Gate。**
