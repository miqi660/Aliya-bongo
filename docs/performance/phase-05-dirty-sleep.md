# Phase 5 — Dirty Rendering + 0 FPS Sleep

日期：2026-09-10。起点：`master` / `726be59`（Phase 4 Render Scheduler 已推送）。本阶段只完成 Native dirty/sleep policy、条件变量阻塞和独立线程验收；不实现 Phase 6 键盘、鼠标按键、MouseMove 输入，也不把 Native Runtime 接入 Tauri 主应用生产 Render Loop。

## 完成内容

- `native/dirty_sleep.h`、`native/dirty_sleep.cpp`：新增 `DirtySleepController`。
  - `ParameterChanged`、`MotionAdvanced`、`ExpressionChanged`、`Resize`、`ModelLoad`、`VisibilityChanged` 均可设置 dirty 并唤醒 Scheduler。
  - `framePresented()` 清除 dirty；只有 dirty 或 animating 时返回 Frame。
  - 静态且可见时使用 `std::condition_variable` 阻塞；不使用 1ms/5ms/10ms polling sleep。
  - 隐藏时等待重新显示或 Shutdown，不因隐藏状态的 dirty 谓词形成忙循环。
  - 提供 sleep enter/exit、wakeup、dirty event、frame 和最近一次 wake latency 计数。
- `native/scheduler.h`、`native/scheduler.cpp`：增加 `nextDeadline()`，让阻塞等待同时覆盖 FPS 截止时间和 Idle/Deep Idle/Sleep 状态边界。
- `native/validation.cpp`：新增 `native_dirty_sleep_run`，用真实线程和 `promise/future` 验证静态阻塞、所有非输入 dirty 来源、隐藏/显示、animating 和 Shutdown。
- `native/build.rs`、`src-tauri/examples/native_dirty_sleep.rs`、`src-tauri/Cargo.toml`：接入独立验收 example。
- `scripts/measure-native-phase5.ps1`：Release 重复运行与 Working Set/Private Bytes 采样。
- `plan.md`、`PROJECT_MODIFICATION_CHECKLIST(2).md`：同步 Phase 5 进度和 Phase 6 边界。

## 线程与状态语义

```text
dirty / animating / visible
            ↓
RenderScheduler::tick + nextDeadline
            ↓
condition_variable wait / Frame
            ↓
Update → Render → Present
            ↓
framePresented() → dirty = false
```

静态条件为 `dirty == false && !animating && visible == true`。进入 SLEEP 后，Render 消费线程没有周期性唤醒；dirty、显示、动画和 Shutdown 通过条件变量改变等待结果。键盘、鼠标按键与 MouseMove 的生产输入源保留到 Phase 6。

Native Runtime 已在 `runtime.cpp` 中对 Model Load/Unload、Resize、Parameter、Motion、Expression、Update 设置 dirty，并在 Render 成功后清除 dirty；`runtime_is_dirty` 与 `runtime_is_animating` 已存在。本阶段控制器是可复用 Native policy 和独立验收宿主，尚未连接到 Tauri 生产 Render Loop。

## 复现命令

```powershell
cargo build -p bongo-cat --example native_dirty_sleep --features native-runtime --offline
cargo run -p bongo-cat --example native_dirty_sleep --features native-runtime --offline -- 10
cargo build -p bongo-cat --release --example native_dirty_sleep --features native-runtime --offline
target/release/examples/native_dirty_sleep.exe 1
./scripts/measure-native-phase5.ps1
```

采样脚本默认重复 50 次，结果写入 `target/phase-05-release/`，不纳入 Git。

## 验证结果

| 检查项                   | 结果 | 实测证据                                                                                                       |
| ------------------------ | ---- | -------------------------------------------------------------------------------------------------------------- |
| Debug Dirty Sleep 构建   | PASS | `cargo build -p bongo-cat --example native_dirty_sleep --features native-runtime --offline`                    |
| Debug 验收               | PASS | 重复 1 次、10 次均退出码 0；静态区间 `before=1`，没有额外 Frame                                                |
| Release Dirty Sleep 构建 | PASS | `cargo build -p bongo-cat --release --example native_dirty_sleep --features native-runtime --offline`          |
| Release 验收             | PASS | 重复 1 次退出码 0；`after=8`、`enter=1`、`exit=1`、`wakeups=8`、`dirty_events=7`                               |
| 真正阻塞等待             | PASS | `sleeping()` 进入条件变量等待；等待 40ms 未产生 Frame                                                          |
| Dirty 来源               | PASS | Parameter、Motion、Expression、Resize、Model Load 各唤醒一帧；Visibility 通过 Show/Hide 路径验证               |
| 隐藏/显示                | PASS | Hidden 等待期间不返回 Frame；Show 唤醒且只提交一帧                                                             |
| Animating / Shutdown     | PASS | Animating 唤醒 Frame；Shutdown 唤醒阻塞线程并返回 `Shutdown`                                                   |
| Wake latency             | PASS | Release 单次运行观测 `10 us`；该值为短时验收样本，不是长期分位数                                               |
| Release 重复采样         | PASS | 50 次、86 个采样点、9.719 s；Working Set 2.73→峰值/结束 6.15 MB；Private Bytes 0.47→峰值 1.39 MB、结束 1.36 MB |

## Benchmark / Before / After

本阶段没有生产 Tauri Render Loop 的同场景 CPU/GPU Before/After，因此不宣称已降低整机 CPU/GPU。可确认的阶段性差异是：

| 项目                   | Phase 4 前/本阶段前                                 | Phase 5 后                                                            |
| ---------------------- | --------------------------------------------------- | --------------------------------------------------------------------- |
| Static scheduler       | Scheduler policy 可判定 SLEEP，但没有线程阻塞控制器 | 条件变量真正阻塞，静态验收区间无新增 Frame                            |
| Dirty semantics        | Runtime dirty flag 已有，缺少统一消费等待           | `DirtySleepController` 统一 dirty、animating、visible 与 Wake         |
| Wake evidence          | 只有 Scheduler policy wake 计数                     | 真实 waiter 线程由 dirty/Show/Animating/Shutdown 唤醒，并记录 latency |
| Production integration | Native example 独立运行                             | 仍未接入 Tauri 主应用，留作后续接线任务                               |

## Exit Criteria

- [x] Static 状态无 Update/Render/Present 帧提交（独立 controller 验收中 `frame_count` 保持 1）。
- [x] Scheduler 使用条件变量阻塞等待。
- [x] 非输入 dirty 来源、Show、Animating、Shutdown 可唤醒并返回正确状态。
- [x] Render 后 clear dirty 语义已通过 `framePresented()` 验证；Runtime Render 也已有清除逻辑。
- [x] Sleep enter/exit 与 wake latency 有实测输出。
- [ ] 键盘、鼠标按键、MouseMove Wake：明确留给 Phase 6，当前不作为本阶段已完成项。
- [ ] Tauri 生产 Render Loop 接线：当前 Native-first 主应用尚未接入，不能作为本阶段已验证项。

结论：Phase 5 的独立 Native dirty/sleep policy 与阻塞验收通过；允许下一窗口进入 Phase 6 输入状态，但不得把当前 example 验收误写成生产 Tauri Runtime 已完成接线。

## 未解决事项

- 未执行 30 分钟/2 小时长期稳定性、GPU 利用率/显存和真实生产线程 Wakeups/s 采样。
- 采样脚本只覆盖 controller example 的短时生命周期；不替代 Phase 9 的同场景 Before/After。
