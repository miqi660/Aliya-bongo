# Phase 6 — Native Input State / 高频输入合并

日期：2026-09-10。起点：`master` / `f62a7a8`（Phase 5 Dirty Sleep 已推送）。本阶段只完成 Native shared input state、MouseMove latest-state、离散键鼠事件保序和 Wake 合并；不进入 Phase 7 资源生命周期。

现有 Rust 输入链路仍是 `rdev` 事件逐条发送到 WebView，Tauri 主应用也尚未接入 Native Render Loop。因此本阶段提供 Native policy 和独立验收宿主，不声称已经替换生产 WebView 输入路径或完成真实硬件 polling rate 接线。

## 完成内容

- `native/input_state.h`、`native/input_state.cpp`：新增线程安全 `InputState`。
  - MouseMove 只覆盖 `latest_mouse_x/latest_mouse_y`，每个 pending 批次只保留一个待消费状态。
  - Keyboard 和 Mouse Button Press/Release 进入 FIFO 离散事件队列，并同步维护键盘 bitset、鼠标按键 bitmask。
  - 同一批 pending 输入只返回一次 `needsWake`；Render 消费后下一批输入才重新请求 Wake。
  - 记录 raw MouseMove、consumed Mouse state、Keyboard、Mouse Button 和 Wake request 计数。
- `native/dirty_sleep.h`：增加 MouseMove、Keyboard、MouseButton dirty source，供 Input State policy 连接 Phase 5 Scheduler Wake。
- `native/validation.cpp`：新增 `native_input_state_run`。
  - 真实 producer thread 发布 MouseMove 和离散事件。
  - 验证 500/1000/2000/4000/8000 事件压力下 latest-state、Wake 合并、事件顺序和状态清理。
  - 验证 Input producer 不调用 OpenGL、Cubism Update 或 Render；它只发布 shared state 并请求 Scheduler dirty。
- `native/build.rs`、`src-tauri/examples/native_input_state.rs`、`src-tauri/Cargo.toml`：接入独立 Rust 验收入口。
- `scripts/measure-native-phase6.ps1`：Release 重复压力与进程资源采样。
- `plan.md`、`PROJECT_MODIFICATION_CHECKLIST(2).md`：同步 Phase 6 进度和生产接线边界。

## 输入语义

```text
MouseMove ───────→ latest position ───────┐
Keyboard/Button ─→ FIFO discrete queue ───┼→ one pending Wake
                                           ↓
                                     Render consume
                                           ↓
                                  next pending batch
```

连续 MouseMove 中间值允许丢弃，最终消费值必须是该 pending 批次的最后坐标。离散 Press/Release 不丢失，sequence 保持跨键盘与鼠标事件的发布顺序；快速 Press/Release 后，状态必须回到未按下，避免 stuck state。

InputState 的 producer 临界区只更新固定大小坐标、bitset、bitmask 和单个小事件，未执行 OpenGL、Cubism、JSON、IPC 或大块复制。离散队列由消费线程一次取走，允许短暂积累以保留事件。

## 复现命令

```powershell
cargo fmt --all -- --check
cargo build -p bongo-cat --example native_input_state --features native-runtime --offline
cargo run -p bongo-cat --example native_input_state --features native-runtime --offline -- 10
cargo build -p bongo-cat --release --example native_input_state --features native-runtime --offline
target/release/examples/native_input_state.exe 1
./scripts/measure-native-phase6.ps1
```

采样脚本默认重复 100 次，结果写入 `target/phase-06-release/`，不纳入 Git。

## 验证结果

| 检查项                   | 结果 | 实测证据                                                                                                                   |
| ------------------------ | ---- | -------------------------------------------------------------------------------------------------------------------------- |
| Debug 格式检查           | PASS | `cargo fmt --all -- --check`                                                                                               |
| Debug Input State 构建   | PASS | `cargo build -p bongo-cat --example native_input_state --features native-runtime --offline`                                |
| Debug 验收               | PASS | 重复 1 次、10 次均退出码 0                                                                                                 |
| Release Input State 构建 | PASS | `cargo build -p bongo-cat --release --example native_input_state --features native-runtime --offline`                      |
| Release 验收             | PASS | 500/1000/2000/4000/8000 压力均通过，退出码 0                                                                               |
| MouseMove latest-state   | PASS | 每个批次 raw=500..8000，consumed=2；最终坐标精确匹配最后发布值                                                             |
| Wake 合并                | PASS | 每个场景 `wake_requests=3`、Scheduler `frames=4`，不随 MouseMove 数量线性增长                                              |
| 离散事件完整性           | PASS | 6 个键盘 + 4 个鼠标事件全部保留，sequence 连续为 0..9                                                                      |
| 多键/修饰键/stuck state  | PASS | A、B、Shift 与 Left/Right 按键状态均正确回落到 0                                                                           |
| Release 重复采样         | PASS | 100 次、614 个采样点、69.312 s；Working Set 2.73→峰值 6.25 MB、结束 6.23 MB；Private Bytes 0.47→峰值 1.47 MB、结束 1.45 MB |

## Benchmark / Before / After

本阶段压力值是独立 Native example 的事件批次规模，不是连接真实设备的墙钟 500/1000/2000/4000/8000 Hz 采样；由于生产 Tauri Native Render Loop 尚未接入，也不宣称整机 Runtime Update/Render/Present 已完成真实硬件对比。

| 项目                   | Phase 5 前/本阶段前              | Phase 6 后                                                  |
| ---------------------- | -------------------------------- | ----------------------------------------------------------- |
| MouseMove storage      | 没有 Native shared input state   | 只保留每批次最新坐标，500→8000 raw 仍只消费 2 次 state      |
| Discrete input         | Native policy 未定义             | FIFO 保留 Press/Release 顺序，10 个事件全部消费             |
| Wake behavior          | Dirty Sleep 只覆盖非输入来源     | 输入 pending 批次合并为 3 次 Wake，Scheduler frame 固定为 4 |
| Production integration | Rust `rdev` → WebView 逐事件链路 | 生产链路保持不变，Native 接线留待后续集成任务               |

## Exit Criteria

- [x] MouseMove latest-state 生效。
- [x] Keyboard/Mouse Button Press/Release 不丢失。
- [x] 500/1000/2000/4000/8000 事件压力不放大 Native Scheduler frame/Wake。
- [x] 多键、修饰键、快速 Press/Release 不产生 stuck state。
- [x] Input producer 不调用 OpenGL、Cubism Update 或 Render。
- [ ] 真实设备 polling rate 与生产 Runtime Update/Render/Present 对比：当前生产 Native Render Loop 尚未接入，不能作为本阶段已验证项。

结论：Phase 6 Native Input State policy 与独立线程验收通过；允许下一窗口进入 Phase 7，但必须保留真实生产接线和硬件 polling benchmark 的未完成标记。

## 未解决事项

- 现有 `src-tauri/src/core/device.rs` 仍逐事件 emit 到 WebView，尚未改为 Native InputState producer。
- 未执行真实 500 Hz/1000 Hz 设备墙钟采样，也未收集生产 Runtime 的 Update/Render/Present/s。
- 未执行 30 分钟/2 小时长期稳定性、GPU 利用率/显存和 Phase 9 Before/After 回归。
