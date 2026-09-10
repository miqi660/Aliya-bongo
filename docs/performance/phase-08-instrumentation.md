# Phase 8 — Instrumentation + Release 调优

日期：2026-09-10。起点：`master` / `ac2066a`（Phase 7 Native Resource Lifecycle 已推送）。
本阶段只完成可复用的 Native instrumentation、独立 Scheduler/DirtySleep/InputState 聚合验证和 Release 调优数据；当前 Tauri 生产 Native Render Loop 尚未接入，因此报告不把独立 example 计数表述为生产应用计数。

## 完成内容

- `native/instrumentation.h/.cpp`
  - 新增 `RenderInstrumentation`，统一记录 `Update`、`Render`、`Present`、Input、MouseMove raw/consumed、Scheduler Wake、Sleep enter/exit。
  - 计数使用 `std::atomic<uint64_t>` 和 `memory_order_relaxed`；热路径不打印日志。
  - `setEnabled(false)` 可关闭计数，关闭状态仍保留同一调用接口，供 Release 开关和开销对比使用。
- `native/validation.cpp`
  - 新增 `native_instrumentation_run`。
  - 以单调时间驱动 ACTIVE 60/30、IDLE 10、DEEP_IDLE 5、SLEEP 0 矩阵，验证 Update/Render/Present 对齐、Sleep 不提交帧、Wake 后立即恢复一帧。
  - 每个模拟 1 秒输出一次 aggregate summary；没有逐事件、逐帧日志。
  - 汇总 Phase 5 DirtySleep 和 Phase 6 InputState 的 Wake/Sleep/Input 计数，并执行 enabled/disabled 热路径开销对比。
- `src-tauri/examples/native_instrumentation.rs`、`src-tauri/Cargo.toml`
  - 注册独立 Rust 验收入口。
- `scripts/measure-native-phase8.ps1`
  - 对 Release example 采样 CPU、Working Set、Private Bytes，并保存 `target/phase-08-release/metrics.json`。

## Instrumentation 语义

| 计数                                              | 语义                                                          |
| ------------------------------------------------- | ------------------------------------------------------------- |
| `update_count` / `render_count` / `present_count` | Scheduler due frame 的三段计数；每个 due frame 必须各增加一次 |
| `input_count`                                     | 本例中 raw MouseMove、Keyboard、Mouse Button 发布事件总数     |
| `mouse_move_raw_count`                            | InputState 发布的 MouseMove 次数                              |
| `mouse_move_consume_count`                        | Render/Update 消费到的最新 MouseMove state 批次数             |
| `scheduler_wakeup_count`                          | Scheduler/DirtySleep/Input 触发的 Wake 合计                   |
| `sleep_enter_count` / `sleep_exit_count`          | DirtySleep 阻塞进入/退出次数                                  |

Instrumentation 本身只负责计数和快照；周期输出由验收驱动负责，每秒一次，避免高频 I/O 污染测量。

## Release 调优矩阵

```text
ACTIVE 60 FPS → ACTIVE 30 FPS → IDLE 10 FPS → DEEP_IDLE 5 FPS → SLEEP 0 FPS
                                                              ↓
                                                        Wake → 一帧 ACTIVE
```

本阶段使用现有 Scheduler 默认策略：`idle_after=1000 ms`、`deep_idle_after=2000 ms`、`sleep_after=3000 ms`；不改变 Phase 4 已验证的策略，只增加可观测性和同场景 Release 数据。

## 复现命令

在仓库根目录 PowerShell 执行：

```powershell
cargo fmt --all -- --check
cargo check -p bongo-cat --features native-runtime --offline
cargo build -p bongo-cat --example native_instrumentation --features native-runtime --offline
cargo run -p bongo-cat --example native_instrumentation --features native-runtime --offline -- 1
cargo build -p bongo-cat --release --example native_instrumentation --features native-runtime --offline
./scripts/measure-native-phase8.ps1 -Repeats 100
```

Phase 8 example 参数为 instrumentation 热路径重复倍数，范围 `1..10000`；脚本默认 `100`，每次进程退出前保留 500 ms 采样窗口。

## 验证结果

| 检查项                    | 结果 | 实测证据                                                                        |
| ------------------------- | ---- | ------------------------------------------------------------------------------- |
| Rust 格式                 | PASS | `cargo fmt --all -- --check`                                                    |
| Cargo 检查                | PASS | `cargo check -p bongo-cat --features native-runtime --offline`                  |
| Debug 构建与验收          | PASS | `native_instrumentation 1` 退出码 0                                             |
| 既有 Scheduler 回归       | PASS | Phase 4 `native_scheduler 1` 通过，ACTIVE=59、IDLE=10、DEEP_IDLE=5、SLEEP=0     |
| 既有 DirtySleep 回归      | PASS | Phase 5 `native_dirty_sleep 1` 通过，Sleep enter/exit=1/1、Wake=8               |
| 既有 InputState 回归      | PASS | Phase 6 `native_input_state 1` 的 500/1000/2000/4000/8000 压力均通过            |
| Release 构建              | PASS | `native_instrumentation` Release 构建成功                                       |
| FPS / Sleep 矩阵          | PASS | ACTIVE60=59、ACTIVE30=30、IDLE10=10、DEEP_IDLE5=5、SLEEP=0                      |
| 三段帧计数一致            | PASS | Update=Render=Present=164                                                       |
| Input / Wake / Sleep 聚合 | PASS | Input=511、MouseRaw=501、MouseConsumed=2、SchedulerWake=12、SleepEnter/Exit=1/1 |
| 1 秒 Aggregate Summary    | PASS | 输出 t=1s 至 t=5s 五次汇总，无逐事件/逐帧日志                                   |
| instrumentation 开关      | PASS | enabled 计数正常；disabled 三段计数均为 0                                       |

## Release 采样与开销

环境为 Windows x64 MSVC、16 logical processors；独立 Native policy example、Release 构建、PowerShell 100 ms 采样。

三次 `-Repeats 100`：

| Run | Duration | CPU Avg | Working Set Start → End | Private Bytes Start → End | Enabled / Disabled 热路径 |
| --- | -------: | ------: | ----------------------: | ------------------------: | ------------------------: |
| 1   |  1.130 s |  0.432% |          2.73 → 6.19 MB |            0.47 → 1.37 MB |        2.6688 / 1.8120 ms |
| 2   |  1.136 s |  0.086% |          2.73 → 6.18 MB |            0.48 → 1.37 MB |        2.5575 / 1.8332 ms |
| 3   |  1.129 s |  0.000% |          2.73 → 6.19 MB |            0.47 → 1.37 MB |        2.5589 / 1.8142 ms |

热路径每次运行 500,000 个 Update/Render/Present 三元组；enabled 模式只做 relaxed atomic 计数，不做输出。该短时基准显示绝对开销处于毫秒级，且禁用开关有效；CPU 百分比因进程很短而不作为性能结论。

## Phase 8 Exit Criteria

- [x] 九类 Runtime/Scheduler/Input/Sleep instrumentation 已统一并可快照。
- [x] 不在高频路径逐事件打印；每秒 Aggregate Summary。
- [x] Release 可关闭 instrumentation，disabled 路径计数为零。
- [x] ACTIVE 60/30、IDLE 10、DEEP_IDLE 5、SLEEP 0 和 Wake 有实测依据。
- [x] Release 构建、重复采样和既有 Phase 4/5/6 回归通过。
- [x] 短时 instrumentation 开销可接受；未把 Debug 数据当作 Release 性能结论。

## 未解决事项与下一阶段

- 当前 instrumentation 通过独立 Native 验收驱动使用；Tauri 生产 Render Loop、Rust `rdev` 输入链路尚未接入该聚合器。
- Lock contention 未做独立 ETW/采样器剖析；本阶段只复用 Phase 6 的生产者线程验收和 Release 热路径开关基准。
- Texture lifetime 沿用 Phase 7 的生命周期证据，本阶段没有重复上传或改变资源策略。
- 尚未执行 Phase 9 的同环境 Before/After、30 分钟/2 小时长时和 GPU 指标回归。
