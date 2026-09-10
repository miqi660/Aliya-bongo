# Phase 9 — Before / After 性能回归

日期：2026-09-10。起点：`master` / `b218bd7`（Phase 8 Instrumentation 已推送）。

本阶段已整理 Phase 0 Before 与当前 Release After 证据，但当前仓库仍没有接入 Tauri 生产 Native Render Loop。因此两组数据不能作为同场景 Before/After 性能提升结论；本报告明确记录可比边界和剩余阻塞，不用独立 Native policy 数据替代生产应用回归。

## 1. 环境一致性

以下机器信息来自 Phase 0，Phase 9 仍在同一工作环境执行：

| 项目               | 值                                                              | 状态                   |
| ------------------ | --------------------------------------------------------------- | ---------------------- |
| Windows            | Windows 10 Pro 23H2，Build 22631 / UBR 2861                     | 已记录                 |
| CPU                | AMD Ryzen 7 7745HX with Radeon Graphics，16 logical processors  | 已记录                 |
| RAM                | 31.18 GiB                                                       | 已记录                 |
| GPU                | NVIDIA GeForce RTX 4060 Laptop GPU，driver 610.88               | 已记录                 |
| Display            | 2560 × 1600 @ 165 Hz                                            | 已记录                 |
| DPI                | 约 150%，应用上下文仍需复核                                     | 部分记录               |
| Mouse polling rate | 未确认                                                          | 未知                   |
| Power mode         | 未记录                                                          | 未知                   |
| Model              | Phase 0 Web Runtime 预置 standard；After 使用 `cat.model3.json` | 需在生产回归中再次确认 |

## 2. Before：Phase 0 Web Runtime

Before 来自真实 WebView + Vue + Pixi.js + easy-live2d 应用采样；CPU/内存为主进程与同批次 WebView2 进程合计，CPU 按 16 logical processors 归一化。Phase 0 的 Update/Render/Present、GPU target utilization 和 Wakeups/s 没有 instrumentation，因此不能从资源曲线推导。

| 场景                                | Duration |  CPU Avg / Peak | Working Set Start → End（Peak） | Update / Render / Present | 状态                       |
| ----------------------------------- | -------: | --------------: | ------------------------------: | ------------------------- | -------------------------- |
| Idle                                |  148.5 s | 0.092% / 0.526% |    211.72 → 212.14 MB（212.51） | Unknown                   | Before measured            |
| 持续 MouseMove                      |   61.9 s | 0.036% / 0.567% |    212.40 → 212.64 MB（212.66） | Unknown                   | Before measured            |
| 高频 Keyboard                       |   48.3 s | 0.022% / 0.360% |    212.61 → 213.11 MB（213.18） | Unknown                   | Before measured            |
| Motion / Expression                 |        — |               — |                               — | Unknown                   | Phase 0 未取得专用触发数据 |
| Hidden / 30 FPS / 60 FPS / High DPI |        — |               — |                               — | Unknown                   | Phase 0 未完成             |

## 3. After：当前 Release Native policy

After 使用 Phase 8 的 `native_instrumentation` Release example 和 `measure-native-phase8.ps1 -Repeats 100`。它是独立 Native policy 进程，不包含 Tauri WebView、真实生产窗口事件链或 Cubism 生产 Render Loop。

```text
Test ID: phase9-after-native-policy-instrumentation
Base: b218bd7
Build: cargo build -p bongo-cat --release --example native_instrumentation --features native-runtime --offline
Scenario: ACTIVE 60/30、IDLE 10、DEEP_IDLE 5、SLEEP 0、Input/Sleep/Wake aggregate
Duration: 1.094 s
Samples: 10
Logical processors: 16
Working Set: Start 2.73 MB；Peak 6.19 MB；End 6.19 MB
Private Bytes: Start 0.48 MB；Peak 1.39 MB；End 1.38 MB
```

当前 After 计数：

| 指标                             |            After |
| -------------------------------- | ---------------: |
| ACTIVE 60 / 30                   | 59 / 30 frames/s |
| IDLE 10 / DEEP_IDLE 5            |  10 / 5 frames/s |
| SLEEP                            |          0 frame |
| Update / Render / Present        |  164 / 164 / 164 |
| Input / MouseMove raw / consumed |    511 / 501 / 2 |
| Scheduler Wake                   |               12 |
| Sleep enter / exit               |            1 / 1 |

三次 Phase 8 Release 重复采样的 Duration 为 1.129–1.136 s，Working Set End 为 6.18–6.19 MB，Private Bytes End 为 1.37 MB；这证明独立 policy example 的短时数据稳定，但不等同于 WebView 应用或生产 Native Runtime 的 After。

## 4. Before / After 可比性结论

| 指标                      | 状态    | 原因                                                                              |
| ------------------------- | ------- | --------------------------------------------------------------------------------- |
| CPU Before / After        | PARTIAL | Before 是 WebView2+主进程真实应用；After 是独立 Native policy，进程拓扑与时长不同 |
| GPU Before / After        | UNKNOWN | Before 无目标 GPU 样本；After 未采集 GPU engine/显存                              |
| Memory Before / After     | PARTIAL | 数值已记录，但 Working Set 的进程组成和场景不同，禁止计算提升百分比               |
| Update / Render / Present | PARTIAL | After 有 instrumentation；Before 没有同类计数器                                   |
| Wakeups/s                 | PARTIAL | After 有 policy aggregate；Before 未记录                                          |
| 高频输入                  | PARTIAL | After 有 500–8000 独立事件压力；Before 是 WebView 采样，未形成同口径计数          |
| 30 分钟 / 2 小时趋势      | NOT RUN | 生产 Native Render Loop 尚未接入，当前长时 policy 结果不能代表目标应用            |

结论：Phase 9 已完成数据边界审计和当前 After 可复现采样，但 Before/After 性能回归 Exit Criteria 不能宣称全部通过，也不能进入 Phase 10 Runtime Freeze。

## 5. 可复现命令

```powershell
cargo check -p bongo-cat --features native-runtime --offline
cargo build -p bongo-cat --release --example native_instrumentation --features native-runtime --offline
./scripts/measure-native-phase8.ps1 -Repeats 100
```

Phase 0 Before 原始口径见 [`phase-00-baseline.md`](phase-00-baseline.md)，After instrumentation 口径见 [`phase-08-instrumentation.md`](phase-08-instrumentation.md)。

## 6. 未解决事项与阻塞

- 需要先把 Native Render Loop、Window、InputState 和 Scheduler 接入 Tauri 生产链路，才能在同一应用拓扑中重新采集 Before/After。
- 需要确认 Power mode、应用实际 DPI、窗口 Client 尺寸、Mouse polling rate，并固定模型、FPS、时长。
- 需要补采真实生产应用的 Idle、MouseMove、Keyboard、Motion、Expression、Hidden、30/60 FPS、High DPI、500/1000 Hz、30 分钟和 2 小时场景。
- GPU engine、显存和长时 memory trend 仍未知；Phase 9 不修改代码绕过这些缺口。
