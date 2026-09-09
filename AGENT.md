# agent.md

## 0. 文档目标

本文件只约束 **Native-first 性能优化路线**。

当前唯一目标：

> 将常驻 Live2D 渲染运行时迁移为 **Cubism SDK for Native + OpenGL**，建立低 CPU、低 GPU、低后台功耗、低输入放大、可长期稳定运行的桌面 Runtime。

所有 Coding Agent 的修改必须直接服务于以下目标：

- Native Runtime
- OpenGL Renderer
- Rust / C++ Bridge
- Native Window Lifecycle
- Render Scheduler
- Input State
- 高频输入合并
- Dirty Rendering
- Sleep / Wakeup
- GPU / CPU 资源生命周期
- Benchmark
- 长时间稳定性
- Runtime Freeze

如果一个改动不能推进以上目标，则当前阶段不应实施。

---

# 1. 总体架构

目标运行链路：

```text
Rust Application Core
        │
        ├─ Native Window
        ├─ Input State
        ├─ Render Scheduler
        ├─ Runtime Lifecycle
        │
        └─ Native Bridge
                │
                ▼
        Cubism SDK for Native
                │
                ▼
              OpenGL
```

常驻 Live2D Runtime 的核心原则：

```text
Input
  ↓
State
  ↓
Scheduler
  ↓
Update
  ↓
Render
```

禁止：

```text
Input Event
  ↓
直接 Live2D Update
  ↓
直接 Render
```

---

# 2. 核心技术决策

## 2.1 Cubism SDK for Native

Live2D Runtime 使用：

```text
Cubism SDK for Native
```

要求：

- 使用官方 Cubism Native Runtime
- 不自行实现 Cubism Core
- 不修改 `.moc3` 二进制格式
- 不在业务层复制 Cubism 内部对象模型
- Native Runtime 封装保持薄
- 所有资源生命周期显式管理
- Runtime 行为必须可测量

---

## 2.2 OpenGL

Native Runtime 统一使用：

```text
OpenGL
```

当前性能阶段只维护一套 Renderer。

优先保证：

- 单一 OpenGL 初始化路径
- 单一 Shader 路径
- 单一 Texture 生命周期
- 单一 Framebuffer 生命周期
- 单一 Cubism Renderer 生命周期
- 单一 Present / Swap 策略
- 单一性能测量模型

不得在没有 benchmark 证据的情况下扩展额外 Renderer Backend。

---

## 2.3 Native-first

常驻桌宠渲染不得继续依赖：

```text
WebView
Vue
Pixi.js
easy-live2d
WebGL
```

作为最终 Runtime。

目标是：

```text
Native Window
  ↓
Cubism Native
  ↓
OpenGL
```

---

# 3. 固定开发顺序

性能开发固定按以下顺序进行：

```text
P0  可重复运行基线
 ↓
P1  性能测量基线
 ↓
P2  Native Runtime Skeleton
 ↓
P3  Cubism Native + OpenGL
 ↓
P4  Native Window Lifecycle
 ↓
P5  Render Scheduler
 ↓
P6  Input State / 高频输入合并
 ↓
P7  Dirty Rendering / Sleep
 ↓
P8  资源生命周期
 ↓
P9  性能回归与长时间稳定性
 ↓
P10 Runtime Freeze
```

不得跳过 Baseline 直接声称性能优化有效。

---

# 4. P0：可重复运行基线

任何架构修改前，必须先读取当前工作区实际实现。

至少确认：

- 当前 branch
- 当前 commit
- `package.json`
- `Cargo.toml`
- 当前构建脚本
- 当前 Live2D 加载路径
- 当前渲染循环
- 当前输入监听路径
- 当前窗口生命周期
- 当前 IPC / event 路径
- 当前 FPS 控制方式

禁止根据旧文档或文件名猜逻辑。

至少执行当前工程对应的：

```bash
pnpm install
pnpm build
cargo check
pnpm tauri dev
pnpm tauri build
```

如果仓库实际命令不同，以当前配置为准。

必须记录：

```text
baseline commit
Windows version
CPU
GPU
RAM
display resolution
display refresh rate
DPI / scale
mouse polling rate（如可确认）
FPS setting
build mode
```

P0 完成条件：

> 当前版本可以稳定构建、启动、运行和退出，并可重复进行性能测量。

---

# 5. P1：性能测量基线

任何性能修改前必须先测量。

至少建立以下 benchmark：

```text
A  启动后静置
B  持续鼠标移动
C  高频键盘输入
D  Motion 播放
E  Expression 播放
F  窗口隐藏
G  30 FPS
H  60 FPS
I  高 DPI
J  30 分钟连续运行
K  2 小时连续运行
```

至少记录：

- CPU 占用
- GPU 占用
- Working Set / RSS
- 实际 render FPS
- update 次数
- render 次数
- present / swap 次数
- MouseMove 原始输入频率
- MouseMove 实际消费频率
- Scheduler wakeups / second
- Hidden 状态 render 次数
- Static 状态 update 次数
- Static 状态 render 次数
- 内存长期趋势

所有数据必须明确标记：

```text
Measured
```

或：

```text
Target
```

禁止把目标数值写成实测结果。

---

# 6. P2：Native Runtime Skeleton

推荐边界：

```text
Rust
 ↓
C ABI / Stable Bridge
 ↓
C++ Cubism Runtime
 ↓
OpenGL
```

Rust 不应直接散落调用大量 C++ Cubism 类。

Native Bridge 必须：

- 薄
- 稳定
- 易于测试
- 不包含高层业务逻辑
- 不隐藏资源所有权
- 不隐藏线程约束

建议最小 API：

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

实际命名可调整，但职责必须保持清晰。

---

# 7. FFI / Bridge 规则

禁止：

```text
Rust
  ↓
直接管理复杂 Cubism C++ 对象树
```

正确：

```text
Rust
  ↓
opaque handle
  ↓
C ABI
  ↓
C++ Runtime
```

要求：

- Rust 只持有 opaque handle
- Bridge 返回明确错误码
- 不跨 FFI 抛 C++ exception
- 字符串编码明确
- 指针所有权明确
- create / destroy 成对
- load / unload 成对
- 不跨线程传递不安全对象
- 不在 FFI 边界复制大型纹理数据

---

# 8. P3：Cubism Native + OpenGL

第一目标：

> 使用现有测试模型完整跑通 Native Cubism + OpenGL 渲染。

至少验证：

- `.model3.json` 正常加载
- `.moc3` 正常加载
- texture 正常加载
- 透明背景
- model matrix
- scale
- viewport resize
- parameter write
- Motion
- Expression
- unload
- reload
- destroy

必须明确以下生命周期：

```text
Window
OpenGL Context
OpenGL Surface
Shader / Program
Texture
Framebuffer
Cubism Renderer
Cubism Model
Runtime
```

禁止依赖进程退出自动清理资源。

---

# 9. OpenGL Context

必须明确：

- Context 创建线程
- Context current 线程
- Render 调用线程
- Swap / Present 所在线程
- Resize 同步方式
- Destroy 顺序

禁止多个线程无控制地同时操作同一 OpenGL Context。

如果需要跨线程：

- 明确所有权
- 明确同步机制
- 明确 Context 切换成本

默认优先单一 Render Thread。

---

# 10. OpenGL 性能原则

优先减少：

- 无意义 Draw
- 无意义 Swap
- 重复 Texture Upload
- 每帧 Shader Compile
- 每帧 Program Link
- 每帧 Framebuffer Recreate
- 每帧 GPU Resource Create / Destroy
- GPU / CPU 强制同步
- 频繁状态查询
- 不必要的 Pipeline Flush

正常渲染路径禁止长期保留：

```text
glFinish
glReadPixels
```

除非 benchmark 或调试明确需要。

---

# 11. Texture 生命周期

Texture 必须：

- Load 时创建
- Runtime 中复用
- Unload 时释放
- Reload 时不泄漏
- Context Destroy 前正确释放

必须测试：

```text
load
unload
load
unload
load
unload
```

并记录：

```text
CPU memory
GPU memory
process memory
```

是否持续增长。

---

# 12. Native Window Lifecycle

Native Window 至少必须覆盖：

```text
Create
Show
Hide
Move
Resize
Destroy
```

窗口状态必须能够驱动 Scheduler。

例如：

```text
Show
 ↓
wake scheduler

Hide
 ↓
stop render

Resize
 ↓
dirty = true
 ↓
wake scheduler

Destroy
 ↓
stop scheduler
 ↓
release renderer
 ↓
release context
```

禁止：

```text
Window Hidden
但 Render Loop 继续执行
```

---

# 13. P5：Render Scheduler

## 13.1 原则

禁止无条件：

```cpp
while (running) {
    update();
    render();
}
```

Scheduler 必须根据以下状态决定是否工作：

- Input State
- Motion
- Expression
- dirty state
- visibility
- resize
- timer
- runtime lifecycle

---

# 14. Scheduler 状态机

至少实现：

```text
ACTIVE
  ↓
IDLE
  ↓
DEEP_IDLE
  ↓
SLEEP
```

初始目标：

```text
ACTIVE
30 / 60 FPS

IDLE
10 FPS

DEEP_IDLE
5 FPS

SLEEP
0 FPS
```

这些只是初始调优参数。

最终值必须根据 benchmark 调整。

---

# 15. ACTIVE

进入条件示例：

- 键盘状态变化
- 鼠标按键变化
- 鼠标追踪需要更新
- Motion 正在推进
- Expression 动态变化
- Resize
- Show
- Runtime 状态变化

要求：

- 输入响应稳定
- render FPS 不超过上限
- update delta 正确
- GPU 不出现额外无意义 Present

---

# 16. IDLE

短时间无高频交互后进入。

目标：

- 降低 Update Rate
- 降低 Render Rate
- 降低 CPU Wakeup
- 保留必要动画
- 保持快速唤醒

---

# 17. DEEP_IDLE

长时间无交互且没有高频动画需求时进入。

目标：

- 进一步降低 Render Rate
- 进一步降低 Update Rate
- 进一步降低线程唤醒
- 为 SLEEP 做准备

---

# 18. SLEEP

满足以下条件时应考虑进入：

- 没有需要推进的动画
- 没有 dirty state
- 没有 resize
- 没有需要立即呈现的参数变化
- Window 状态允许休眠

SLEEP 目标：

```text
render = 0
update = 0
```

直到明确事件唤醒。

---

# 19. SLEEP 实现要求

SLEEP 必须使用真正阻塞等待。

优先：

```text
condition_variable
event
wait handle
native message wakeup
```

禁止：

```text
sleep(1ms) → check
sleep(5ms) → check
sleep(10ms) → check
```

作为长期轮询机制。

必须测量：

```text
wakeups / second
```

Static 状态下应显著低于 ACTIVE。

---

# 20. Hidden

Hidden 状态必须显式停止：

- Cubism update
- OpenGL draw
- swap / present
- render timer

性能验收必须满足：

```text
Hidden:
render submissions = 0
```

直到明确事件唤醒。

---

# 21. Dirty Rendering

Runtime 必须维护 dirty state。

示例 dirty 来源：

```text
parameter changed
motion advanced
expression changed
resize
model loaded
visibility changed
texture changed
```

发生变化：

```text
dirty = true
```

完成有效 Render：

```text
dirty = false
```

如果：

```text
dirty = false
and
not animating
```

则 Scheduler 应优先降级状态。

禁止持续重复绘制完全相同 frame。

---

# 22. Update 与 Render 解耦

允许：

```text
Update Rate != Render Rate
```

但必须保证：

- 动画基于真实 delta time
- 降低 FPS 不改变 Motion 实际速度
- 恢复 ACTIVE 后不出现巨大时间跳变
- Sleep / Resume 后 delta 被合理处理
- Render 不因为 Update 低频而偷偷保持高频 Present

---

# 23. 时间系统

统一使用：

```text
monotonic clock
```

禁止使用可被系统时间调整影响的 wall clock 计算动画 delta。

必须处理：

- debugger pause
- machine suspend
- runtime sleep
- scheduler wakeup
- abnormal large delta

异常大 delta 应合理 clamp 或重置。

---

# 24. P6：Input State

输入架构固定为：

```text
Input Producer
      ↓
Shared Input State
      ↓
Scheduler
      ↓
Model Update
```

输入事件不得直接驱动完整 Render Pipeline。

---

# 25. MouseMove 合并

高 polling rate MouseMove 只保留：

```text
latest_x
latest_y
```

例如：

```text
8000 MouseMove / sec
        ↓
latest state
        ↓
30 FPS scheduler
        ↓
约 30 次有效消费
```

原则：

> Live2D Update 频率不得与鼠标硬件 polling rate 线性绑定。

必须记录：

```text
raw MouseMove / sec
consumed Mouse State / sec
runtime update / sec
render / sec
```

---

# 26. 键盘与鼠标按键

Press / Release 属于离散状态变化。

必须保证：

- Press 不丢
- Release 不丢
- 多键状态正确
- 快速输入不锁死
- 修饰键状态正确
- Mouse Left 正确
- Mouse Right 正确

允许：

```text
event
 ↓
update state
 ↓
wake scheduler
```

禁止：

```text
event
 ↓
立即完整 GPU render
```

---

# 27. Input Thread

Input Thread 必须尽量：

- 不执行 OpenGL
- 不执行 Cubism Update
- 不执行昂贵计算
- 不执行长时间锁
- 不执行高频 IPC
- 不复制大型数据

高频状态可以考虑：

```text
atomic
small mutex
double buffer
lock-free state
```

优先简单、可验证的实现。

不为了无锁而引入不可维护复杂度。

---

# 28. Scheduler Wakeup

Scheduler Wakeup 来源必须明确。

至少包括：

- Keyboard state changed
- Mouse button changed
- Mouse position changed
- Motion started
- Expression changed
- Window shown
- Window resized
- Runtime shutdown

Wakeup 后只执行必要工作。

禁止：

```text
wake
 ↓
无条件重建全部资源
```

---

# 29. 线程模型

必须在文档或代码注释中明确：

```text
Main Thread
Input Thread
Render Thread
Runtime Thread（如独立）
```

以及每类资源的 Owner。

至少明确：

- Window owner
- OpenGL Context owner
- Cubism Runtime owner
- Input State owner
- Scheduler owner

禁止隐式多线程共享。

---

# 30. 锁与同步

优先：

- 小临界区
- 明确所有权
- 避免 Render Thread 等待长任务
- 避免 Input Thread 等待 Render Thread

禁止：

```text
Input Thread
  ↓
拿全局大锁
  ↓
等待 Render
```

也禁止：

```text
Render Thread
  ↓
每帧等待磁盘 / IPC
```

---

# 31. 文件 I/O

模型与 Texture 加载不得长期阻塞高频 Render Loop。

至少保证：

- 正常 Render Frame 不进行重复磁盘读取
- Texture 不每帧重新 decode
- JSON 不每帧重新 parse
- Shader 不每帧重新读取文件

---

# 32. 内存原则

禁止用“程序还没崩”判断内存正常。

必须关注：

```text
CPU Heap
Working Set
GPU Resource
Texture Memory
Model Resource
FFI allocation
```

重点测试：

```text
load/unload
show/hide
resize
motion loop
long run
```

---

# 33. 资源释放顺序

Destroy 必须有明确顺序。

建议：

```text
Stop Scheduler
 ↓
Stop New Input Consumption
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

实际顺序根据 Context 依赖调整。

关键原则：

> 不得在 OpenGL Context 已销毁后再释放需要 Context 的 GPU Resource。

---

# 34. P9：性能回归

每个关键性能改动都必须给出：

```text
Before
After
Environment
Scenario
Measurement Method
Result
```

禁止只写：

```text
应该更省电
理论上更快
```

---

# 35. Benchmark 统一格式

建议记录：

```text
Test ID:
Commit:
Build:
Windows:
CPU:
GPU:
RAM:
Display:
Refresh Rate:
DPI:
Mouse Polling Rate:

Scenario:
Duration:

CPU Avg:
CPU Peak:

GPU Avg:
GPU Peak:

Memory Start:
Memory End:

Update Rate:
Render Rate:
Present Rate:
Wakeups/s:

Notes:
```

---

# 36. Benchmark 场景稳定性

同一对比必须尽量保持：

- 同一机器
- 同一 Windows 电源模式
- 同一模型
- 同一窗口尺寸
- 同一 DPI
- 同一刷新率
- 同一 FPS 设置
- 同一输入方式
- 同一 build mode
- 同一测试时长

禁止拿不同环境的数据直接比较。

---

# 37. CPU 验收原则

重点不是单一百分比，而是：

- Static 是否显著下降
- Hidden 是否接近无工作
- 高频鼠标是否不线性放大 CPU
- Sleep 是否没有 busy loop
- 长时间运行是否稳定

---

# 38. GPU 验收原则

重点确认：

- Render Rate 受 Scheduler 控制
- Hidden 无 Draw
- Hidden 无 Present
- Static 不持续提交重复 Frame
- Texture 不重复 Upload
- Resize 不重复重建资源
- GPU 使用不因 Mouse Polling Rate 线性增加

---

# 39. Memory 验收原则

重点确认：

```text
长时间运行
```

不存在明显持续单向增长。

必须测试：

```text
30 minutes
2 hours
```

以及重复：

```text
load/unload
show/hide
resize
```

---

# 40. 高频输入验收

至少测试：

```text
500 Hz
1000 Hz
```

如设备支持，可测试更高 polling rate。

必须确认：

```text
raw input rate ↑
```

不会导致：

```text
runtime update rate
render rate
GPU present rate
```

同步线性上升。

---

# 41. Hidden 验收

必须能够通过 instrumentation 证明：

```text
Hidden:
Update = 0
Render = 0
Present = 0
```

如果存在必须后台推进的逻辑，必须单独说明原因和频率。

---

# 42. SLEEP 验收

必须证明：

- Scheduler 已进入 Sleep
- Render Thread 未持续循环
- GPU 无周期性 Present
- Input 可以正常唤醒
- Wake 后模型状态正确
- Wake latency 可接受

---

# 43. Instrumentation

允许增加仅用于性能调试的统计：

```text
update_count
render_count
present_count
input_count
mouse_move_raw_count
mouse_move_consume_count
scheduler_wakeup_count
sleep_enter_count
sleep_exit_count
```

必须避免统计本身成为高频性能负担。

Release 可关闭或降低采样频率。

---

# 44. Logging

禁止在高频路径逐事件打印日志。

尤其禁止：

```text
MouseMove 每事件 log
Render 每帧 log
Update 每帧 log
```

Debug 时应采用：

- sampling
- aggregate counters
- periodic summary

例如：

```text
每 1 秒打印一次统计
```

---

# 45. 错误处理

Native Runtime 错误必须：

- 有明确错误码或 Result
- 不静默吞掉资源错误
- 不跨 FFI 抛异常
- 不因为单次模型错误导致未释放 OpenGL Resource
- Destroy 必须可重复安全执行或明确禁止重复

---

# 46. Build 要求

涉及 Native Runtime 修改后至少执行：

```text
cargo check
Native C++ compile
Debug build
Release build
```

如果仍有现有前端构建依赖：

```text
pnpm build
```

也必须保持通过。

---

# 47. Release 性能测试

最终性能结论必须来自：

```text
Release Build
```

Debug Build 只能用于：

- 功能验证
- 日志
- instrumentation
- 崩溃定位

不得用 Debug 性能代表最终 Runtime。

---

# 48. Agent 工作流程

每次代码任务必须执行：

## Step 1

读取当前实现。

## Step 2

明确任务属于：

```text
P0 ~ P10
```

中的哪一项。

## Step 3

只做最小可验证修改。

## Step 4

Build / Run / Benchmark。

## Step 5

报告：

- 修改内容
- 修改原因
- 修改文件
- 性能假设
- 验证方式
- Before / After
- 未验证项
- 对 Runtime Freeze 的推进情况

---

# 49. 提交纪律

优先：

```text
小步
可回滚
可 benchmark
可定位
```

建议 commit：

```text
perf(render):
perf(runtime):
perf(input):
perf(scheduler):
fix(native):
fix(opengl):
test(perf):
docs(perf):
```

禁止把多个无关性能主题放在一次提交。

---

# 50. 禁止事项

当前性能阶段禁止：

- 没有 Baseline 就优化
- 没有数据就宣称性能提升
- 用 busy loop 换低延迟
- 高频 MouseMove 直接驱动 Render
- 隐藏窗口后继续无意义 Draw
- Static 状态持续 Present 同一 Frame
- 每帧创建 / 销毁 GPU Resource
- 每帧上传不变 Texture
- 每帧编译 Shader
- 每帧读取模型 JSON
- OpenGL Context 跨线程无控制共享
- FFI 中隐藏不明确资源所有权
- Release 资源依赖进程退出清理
- 只测一次 Task Manager 就得出结论
- 用 Debug Build 做最终性能结论
- 为“架构漂亮”进行无性能收益的大重构
- 同时引入多套 Renderer Backend

---

# 51. Runtime Freeze Gate

只有以下条件全部满足，性能阶段才完成。

## Build

- [ ] `cargo check` 通过
- [ ] Native C++ 编译通过
- [ ] Debug Build 通过
- [ ] Release Build 通过
- [ ] Release 实机可稳定启动

## Native Runtime

- [ ] Cubism Runtime 初始化正常
- [ ] OpenGL Context 初始化正常
- [ ] Model Load 正常
- [ ] Texture 正常
- [ ] Parameter 正常
- [ ] Motion 正常
- [ ] Expression 正常
- [ ] Resize 正常
- [ ] Unload 正常
- [ ] Reload 正常
- [ ] Destroy 正常

## Scheduler

- [ ] ACTIVE 正常
- [ ] IDLE 正常
- [ ] DEEP_IDLE 正常
- [ ] SLEEP 正常
- [ ] SLEEP 可正常唤醒
- [ ] Render Rate 受 Scheduler 控制
- [ ] Static 不存在高频轮询
- [ ] Hidden 不继续 Render

## Input

- [ ] Keyboard Press / Release 正常
- [ ] Mouse Button 正常
- [ ] MouseMove 使用 latest state
- [ ] 高频 MouseMove 不线性放大 Update
- [ ] 高频 MouseMove 不线性放大 Render
- [ ] 高频输入不导致状态锁死

## OpenGL

- [ ] Texture 生命周期正确
- [ ] Shader / Program 生命周期正确
- [ ] Framebuffer 生命周期正确
- [ ] Context 生命周期正确
- [ ] Resize 不持续泄漏资源
- [ ] Render Path 无长期 `glFinish`
- [ ] Render Path 无无意义同步

## Hidden

- [ ] Update = 0
- [ ] Render = 0
- [ ] Present = 0

## Static / Sleep

- [ ] Update 接近 0 或完全停止
- [ ] Render = 0
- [ ] Present = 0
- [ ] Scheduler 使用阻塞等待
- [ ] 输入唤醒正常

## Stability

- [ ] 多次启动 / 退出正常
- [ ] 多次 Show / Hide 正常
- [ ] 多次 Load / Unload 正常
- [ ] 30 分钟运行无明显异常
- [ ] 2 小时运行无明显持续内存增长
- [ ] 高频输入测试稳定
- [ ] 高 DPI 测试稳定

## Performance Report

- [ ] Baseline 数据完整
- [ ] Native Runtime 数据完整
- [ ] CPU Before / After
- [ ] GPU Before / After
- [ ] Memory Before / After
- [ ] Idle Before / After
- [ ] Hidden Before / After
- [ ] 高频输入 Before / After
- [ ] 30 分钟趋势
- [ ] 2 小时趋势
- [ ] 测试环境完整
- [ ] 未解决问题完整记录

全部满足后：

> **标记 Runtime Freeze。**

---

# 52. 最终原则

所有技术决策优先回答：

1. 是否减少无意义 Update？
2. 是否减少无意义 Render？
3. 是否减少无意义 Present？
4. 是否降低线程 Wakeup？
5. 是否避免输入频率放大为渲染频率？
6. 是否能在 Static 状态真正进入 Sleep？
7. 是否能在 Hidden 状态完全停止 GPU 提交？
8. 是否能通过可重复 Benchmark 证明？
9. 是否能长期稳定运行？
10. 是否能安全释放所有 Native / OpenGL / Cubism 资源？

最终目标：

> **完成一个可测量、可验证、可冻结的 Cubism SDK for Native + OpenGL 低功耗 Runtime。**
