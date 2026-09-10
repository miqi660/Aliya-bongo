# Phase 4 — Render Scheduler

日期：2026-09-10。起点：`master` / `b763bf3`（Phase 3 Native Window Lifecycle 已推送）。
本阶段只实现 Native Render Scheduler 的状态机、帧率控制和时间语义；不进入 Phase 5 的 Dirty Rendering/真正阻塞式 Sleep，也不进入 Phase 6 Input。

## 修改内容与范围

- `native/scheduler.h`、`native/scheduler.cpp`：新增可复用的 `RenderScheduler`，提供 ACTIVE、IDLE、DEEP_IDLE、SLEEP 状态，FPS 目标、超时、单调时钟、delta clamp、Wake 重置以及 transition/wakeup 计数器。
- `native/validation.cpp`：新增 `native_scheduler_run` 验收驱动；在确定性的 `steady_clock` 时间轴上验证 Update/Render/Present 作为同一个 Scheduler frame 提交。
- `native/build.rs`：将 Scheduler 编译进 Native 静态库。
- `src-tauri/examples/native_scheduler.rs`、`src-tauri/Cargo.toml`：新增独立 Scheduler 验收入口。
- `scripts/measure-native-phase4.ps1`：重复运行 Scheduler 场景并采样 Release 进程时长、Working Set 和 Private Bytes。
- `plan.md`、`PROJECT_MODIFICATION_CHECKLIST(2).md`：同步 Phase 4 完成项和 Phase 5 边界。

当前仓库仍未把 Native Runtime 接入 Tauri 主应用的生产 Render Loop；因此本阶段提供的是可复用 Native Scheduler policy 和独立验收宿主。Phase 5 再负责将 Dirty 状态、条件变量/事件等待和真正的 0 FPS 阻塞接入生产消费路径。

## Scheduler 语义

默认配置如下：

| 状态      |        目标 FPS | 进入条件                       |
| --------- | --------------: | ------------------------------ |
| ACTIVE    | 60（可切换 30） | 有动画或最近一次 Wake/Activity |
| IDLE      |              10 | 无 Activity 达 1 秒            |
| DEEP_IDLE |               5 | 无 Activity 达 2 秒            |
| SLEEP     |               0 | 无 Activity 达 3 秒            |

每次 `tick` 返回一个完整调度结果；只有 `due=true` 才允许同一调用路径执行 Update、Render、Present。SLEEP 返回 `due=false`，不会提交任何三者。状态变化后立即允许一帧新状态帧，之后按目标 FPS 排期。

时间使用 `std::chrono::steady_clock`。首帧和 Sleep → Wake 的第一帧 delta 为 0；普通帧 delta 是相邻已提交帧的单调时间差，并限制到 0.25 秒。Motion 使用实际 delta 累加，不使用目标 FPS 倒推时间，因此降低 FPS 不改变 Motion 的实际播放速度。

`wakeup_count` 记录显式 `wake`，`transition_count` 记录状态改变。计数器不代表 Phase 5 的线程阻塞唤醒率；真实线程 Wakeups/s 需要在接入条件变量/事件等待后再测量。

## 复现

在仓库根目录 PowerShell 执行：

```powershell
cargo build -p bongo-cat --example native_scheduler --features native-runtime --offline
cargo run -p bongo-cat --example native_scheduler --features native-runtime --offline -- 1
cargo build -p bongo-cat --release --example native_scheduler --features native-runtime --offline
target/release/examples/native_scheduler.exe 1
./scripts/measure-native-phase4.ps1
```

验收入口参数为场景重复次数，默认 1。采样脚本默认重复 100000 次，以保证进程持续足够长时间形成样本；结果保存在 `target/phase-04-release/`，不纳入 Git。

## 验证结果

状态：Phase 4 Scheduler policy Exit Criteria 通过；不进入 Phase 5 实现。

| 检查项                     | 结果 | 实测证据                                                                                                                       |
| -------------------------- | ---- | ------------------------------------------------------------------------------------------------------------------------------ |
| Debug Scheduler 构建       | PASS | `cargo build -p bongo-cat --example native_scheduler --features native-runtime --offline`                                      |
| Debug Scheduler 验收       | PASS | 四态、ACTIVE 60/30、IDLE 10、DEEP_IDLE 5；进程退出码 0                                                                         |
| Release Scheduler 构建     | PASS | `cargo build -p bongo-cat --release --example native_scheduler --features native-runtime --offline`                            |
| Release Scheduler 验收     | PASS | 状态帧 ACTIVE=59、IDLE=10、DEEP_IDLE=5、SLEEP=0；进程退出码 0                                                                  |
| Update/Render/Present 同步 | PASS | 状态场景三者均为 74，Scheduler 不允许拆分提交                                                                                  |
| 时间系统                   | PASS | `steady_clock` 编译期断言；首帧 delta=0；large delta clamp=0.250                                                               |
| Sleep → Wake               | PASS | Wake 后首帧 delta=0；transition=4、wakeup=1                                                                                    |
| Motion 时间                | PASS | 3 秒模拟中 motion60=2.992、motion30=2.992，差值小于 0.05 秒                                                                    |
| 全 Native examples 回归    | PASS | `cargo build -p bongo-cat --examples --features native-runtime --offline`；native_stress×100 与 Phase 3 lifecycle×100/100 通过 |
| Release 重复采样           | PASS | 100000 次、28 个样本、3.175 秒；Working Set 峰值/结束 6.06 MB，Private Bytes 峰值/结束 1.33 MB                                 |

## Benchmark / Before / After

本阶段使用确定性 1 ms 时间步进验证频率，不把模拟 tick 次数误写成真实线程 Wakeups/s：

```text
Test ID: phase4-release-scheduler-100000
Base: b763bf3（Phase 3 已推送；本阶段工作树未提交）
Build: cargo build -p bongo-cat --release --example native_scheduler --features native-runtime --offline
Scenario: ACTIVE/IDLE/DEEP_IDLE/SLEEP、ACTIVE 60/30 FPS、delta clamp、Wake、Motion
Repeats: 100000
Duration: 3.175 s（PowerShell 进程采样）
Samples: 28
Working Set: Start 2.73 MB；Peak/End 6.06 MB
Private Bytes: Start 0.48 MB；Peak/End 1.33 MB
```

Before/After：不可填写。Phase 0 基线和生产 Scheduler 尚未形成同场景对比；本阶段结果是 Scheduler policy 的功能与短时资源采样，不声称 CPU/GPU 性能提升。

## 未解决事项与下一阶段

- Phase 5 负责 dirty 来源、无变化不 Render、真正的 0 FPS 条件变量/事件阻塞，以及生产路径的 Sleep/Wake。
- Phase 6 负责 Keyboard、Mouse Button、MouseMove latest-state 和输入唤醒；本阶段没有直接接收输入。
- 当前 Native Scheduler 尚未接入 Tauri 主应用的生产窗口/Render Loop；独立验收宿主用于固定状态机证据。
- 未执行 30 分钟/2 小时长期稳定性、GPU 利用率/显存和真实线程 Wakeups/s 采样。
