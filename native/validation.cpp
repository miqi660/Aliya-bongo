// Phase 2 专用验收驱动：只在 example 调用，生产 Runtime 不调用读回或测试窗口。
#include "runtime.h"
#include "scheduler.h"
#include "dirty_sleep.h"
#include "input_state.h"
#include <GL/glew.h>
#include <windows.h>
#include <dwmapi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <thread>
#include <future>
#include <limits>
#include <cmath>
#include <cstdio>
namespace {
constexpr wchar_t kCloseProperty[] = L"AliyaPhase2ValidationClosed";
void ok(AliyaStatus status) {
    if (status) throw std::runtime_error("Native 状态码=" + std::to_string(status));
}
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    // 验收脚本独占销毁顺序，关闭仅隐藏，循环随即结束。
    if (message == WM_CLOSE) {
        SetPropW(window, kCloseProperty, reinterpret_cast<HANDLE>(1));
        ShowWindow(window, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
struct Session {
    HWND window = nullptr;
    AliyaRuntime* runtime = nullptr;
    ~Session() { if (runtime) runtime_destroy(&runtime); if (window) DestroyWindow(window); }
};
struct DpiScope {
    DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ~DpiScope() { if (previous) SetThreadDpiAwarenessContext(previous); }
};
void pump() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}
void verifySurface(int w, int h) {
    // 检测 Viewport 超出实际 WGL 画布的 DPI 错配；模型有像素并不能证明画布完整。
    glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    for (int y : {0, h - 1}) for (int x : {0, w - 1}) {
        unsigned char pixel[4]{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        check(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255,
            "WGL 画布没有覆盖全部 Client 像素，请检查 DPI 初始化");
    }
    check(glGetError() == GL_NO_ERROR, "WGL 画布边界读回失败");
}
void capture(const std::filesystem::path& path, int w, int h) {
    std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
    // 仅验收读回，正常渲染 API 不含 GPU→CPU 同步。
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    check(glGetError() == GL_NO_ERROR, "像素读回失败");
    size_t visible = 0, transparent = 0;
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int y = h - 1; y >= 0; --y) for (int x = 0; x < w; ++x) {
        auto p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
        visible += p[3] > 20; transparent += p[3] == 0;
        unsigned char bg = ((x / 16 + y / 16) % 2) ? 210 : 245;
        // Cubism 输出为预乘 alpha，合成棋盘用于肉眼检查边缘。
        for (int c = 0; c < 3; ++c) f.put(static_cast<char>(std::min(255, p[c] + bg * (255 - p[3]) / 255)));
    }
    check(f.good() && visible > 100 && transparent > 100, "空白模型或透明背景失败");
    printf("像素验收 %s: 可见=%zu 透明=%zu\n", path.filename().string().c_str(), visible, transparent);
}
float value(AliyaRuntime* r, const char* id) { float result; ok(parameter_get(r, id, &result)); return result; }
}
extern "C" int native_validation_run(const char* model, const char* output, unsigned seconds) noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        // WGL 驱动还会读取进程级 DPI；仅设置线程上下文可能造成 150% 下画布裁切。
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            check(GetLastError() == ERROR_ACCESS_DENIED, "进程 DPI 初始化失败");
        DpiScope dpi;
        auto dir = std::filesystem::u8path(output); std::filesystem::create_directories(dir);
        WNDCLASSW wc{}; wc.style = CS_OWNDC; wc.lpfnWndProc = procedure;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AliyaPhase2Validation";
        if (!RegisterClassW(&wc)) check(GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "窗口注册失败");
        for (int cycle = 0; cycle < 3; ++cycle) {
            Session s;
            s.window = CreateWindowExW(0, wc.lpszClassName, L"Aliya Native · Phase 2 验证", WS_OVERLAPPEDWINDOW,
                100, 100, 660, 680, nullptr, nullptr, wc.hInstance, nullptr);
            check(s.window != nullptr, "窗口创建失败");
            // 桌面合成透明由 DWM 完成；alpha 本身额外通过读回断言。
            DWM_BLURBEHIND blur{}; blur.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
            blur.fEnable = TRUE; blur.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
            auto dwm = DwmEnableBlurBehindWindow(s.window, &blur); DeleteObject(blur.hRgnBlur);
            check(SUCCEEDED(dwm), "DWM 透明初始化失败");
            ShowWindow(s.window, SW_SHOW);
            // Start-Process -WindowStyle Hidden 会覆盖第一次 ShowWindow 的 nCmdShow。
            // SWP_SHOWWINDOW 明确显示验收窗口，同时保持控制台可隐藏采样。
            SetWindowPos(s.window, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
            check(IsWindowVisible(s.window), "验收窗口未显示");
            UpdateWindow(s.window); pump();
            puts("创建 Runtime / WGL"); ok(runtime_create(&s.runtime)); ok(runtime_attach_window(s.runtime, s.window));
            printf("OpenGL: %s / %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
            AliyaRuntime* second = nullptr; ok(runtime_create(&second));
            check(runtime_attach_window(second, s.window) == ALIYA_BUSY, "并发上下文约束失败");
            ok(runtime_destroy(&second));
            RECT rect; GetClientRect(s.window, &rect); int w = rect.right, h = rect.bottom;
            printf("Client 像素尺寸: %dx%d, DPI=%u\n", w, h, GetDpiForWindow(s.window));
            ok(runtime_resize(s.runtime, w, h));
            verifySurface(w, h);
            AliyaStatus threadStatus = ALIYA_OK;
            std::thread wrongThread([&] { threadStatus = runtime_render(s.runtime); }); wrongThread.join();
            check(threadStatus == ALIYA_WRONG_THREAD, "跨线程调用未拒绝");
            check(model_load(s.runtime, "missing.model3.json") == ALIYA_IO_ERROR, "缺失模型未返回 I/O 错误");
            puts("加载模型"); ok(model_load(s.runtime, model));
            auto invalidJson = dir / "invalid.model3.json";
            { std::ofstream f(invalidJson); f << "{"; }
            check(model_load(s.runtime, invalidJson.u8string().c_str()) == ALIYA_ASSET_ERROR, "非法 JSON 未拒绝");
            // 部分纹理已创建后触发失败，旧模型仍应能渲染。
            auto partialJson = dir / "partial.model3.json";
            auto modelDir = std::filesystem::absolute(std::filesystem::u8path(model)).parent_path();
            {
                std::ofstream f(partialJson);
                f << "{\"Version\":3,\"FileReferences\":{\"Moc\":\""
                  << (modelDir / "demomodel.moc3").generic_u8string()
                  << "\",\"Textures\":[\"" << (modelDir / "demomodel.1024/texture_00.png").generic_u8string()
                  << "\",\"missing.png\"]}}";
            }
            check(model_load(s.runtime, partialJson.u8string().c_str()) == ALIYA_IO_ERROR, "部分加载失败未返回错误");
            auto invalidExpression = dir / "invalid-expression.model3.json";
            {
                std::ofstream f(invalidExpression);
                f << "{\"Version\":3,\"FileReferences\":{\"Moc\":\""
                  << (modelDir / "demomodel.moc3").generic_u8string()
                  << "\",\"Textures\":[\"" << (modelDir / "demomodel.1024/texture_00.png").generic_u8string()
                  << "\"],\"Expressions\":[{\"Name\":\"invalid\",\"File\":\""
                  << std::filesystem::absolute(invalidJson).generic_u8string() << "\"}]}}";
            }
            check(model_load(s.runtime, invalidExpression.u8string().c_str()) == ALIYA_ASSET_ERROR, "损坏表情被当作加载成功");
            check(runtime_update(s.runtime, std::numeric_limits<float>::quiet_NaN()) == ALIYA_INVALID_ARGUMENT, "NaN 时间未拒绝");
            check(parameter_set(s.runtime, "missing", 1) == ALIYA_INVALID_ARGUMENT, "未知参数未拒绝");
            check(runtime_resize(s.runtime, 0, h) == ALIYA_INVALID_ARGUMENT, "零尺寸未拒绝");
            ok(runtime_update(s.runtime, 0)); ok(runtime_render(s.runtime));
            if (!cycle) capture(dir / "baseline.ppm", w, h);
            ok(runtime_present(s.runtime));
            ok(parameter_set(s.runtime, "Param3", 0.25f)); ok(parameter_add(s.runtime, "Param3", 0.25f));
            check(std::abs(value(s.runtime, "Param3") - 0.5f) < 0.001f, "参数 Set/Add 失败");
            ok(runtime_render(s.runtime)); if (!cycle) capture(dir / "parameter.ppm", w, h);
            ok(runtime_present(s.runtime));
            ok(motion_start(s.runtime, "CAT_motion", 0));
            float minimum = 100, maximum = -100;
            for (int frame = 0; frame < 45; ++frame) {
                ok(runtime_update(s.runtime, 1.f / 30));
                float v = value(s.runtime, "Param"); minimum = std::min(minimum, v); maximum = std::max(maximum, v);
                ok(runtime_render(s.runtime)); ok(runtime_present(s.runtime)); pump();
            }
            check(maximum - minimum > 0.1f, "动作没有推进参数");
            printf("动作参数范围: %.3f..%.3f\n", minimum, maximum);
            ok(motion_stop(s.runtime));
            // 动作中的 Param 控制闪电全屏遮罩，停止后归零再检查透明表情。
            ok(parameter_set(s.runtime, "Param", 0));
            uint8_t animating = 1; ok(runtime_is_animating(s.runtime, &animating)); check(!animating, "Motion Stop 失败");
            ok(expression_set(s.runtime, "live2d_expression1.exp3.json"));
            for (int i = 0; i < 35; ++i) ok(runtime_update(s.runtime, 1.f / 30));
            check(value(s.runtime, "Param4") > 0.9f, "表情没有作用于参数");
            ok(runtime_render(s.runtime)); if (!cycle) capture(dir / "expression.ppm", w, h);
            ok(runtime_present(s.runtime));
            SetWindowPos(s.window, nullptr, 0, 0, 840, 520, SWP_NOMOVE | SWP_NOZORDER); pump();
            GetClientRect(s.window, &rect); w = rect.right; h = rect.bottom;
            ok(runtime_resize(s.runtime, w, h)); verifySurface(w, h); ok(runtime_render(s.runtime));
            if (!cycle) capture(dir / "resize.ppm", w, h);
            ok(runtime_present(s.runtime));
            for (int i = 0; i < 3; ++i) {
                ok(model_unload(s.runtime)); ok(model_unload(s.runtime)); ok(runtime_render(s.runtime));
                check(runtime_update(s.runtime, 0) == ALIYA_INVALID_STATE, "Unload 后 Update 未拒绝");
                puts("加载模型"); ok(model_load(s.runtime, model)); ok(runtime_update(s.runtime, 0)); ok(runtime_render(s.runtime));
                ok(runtime_present(s.runtime));
            }
            if (!cycle && seconds) {
                // 有限验收播放，阻塞消息等待控制 30 FPS；不作为生产 Scheduler。
                ok(motion_start(s.runtime, "CAT_motion", 1));
                auto started = GetTickCount64();
                auto end = started + seconds * 1000ull;
                auto previous = started;
                unsigned frames = 0;
                while (GetTickCount64() < end && IsWindow(s.window)
                    && GetPropW(s.window, kCloseProperty) == nullptr) {
                    MsgWaitForMultipleObjectsEx(0, nullptr, 33, QS_ALLINPUT, MWMO_INPUTAVAILABLE); pump();
                    auto now = GetTickCount64(); if (now - previous < 33) continue;
                    ok(runtime_update(s.runtime, (now - previous) / 1000.f)); previous = now;
                    ok(runtime_render(s.runtime)); ok(runtime_present(s.runtime));
                    ++frames;
                }
                double elapsed = (GetTickCount64() - started) / 1000.0;
                check(frames > 0, "有限播放未执行任何帧");
                printf("有限播放: %.3f 秒, Update/Render/Present 各 %u 次, %.2f 次/秒\n", elapsed, frames, frames / elapsed);
            }
            ok(runtime_destroy(&s.runtime)); ok(runtime_destroy(&s.runtime));
            check(wglGetCurrentContext() == nullptr, "销毁后仍有 current Context");
            printf("生命周期 %d/3 通过\n", cycle + 1);
        }
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        puts("Phase 2 验收通过"); return 0;
    } catch (const std::exception& error) { fprintf(stderr, "验收失败: %s\n", error.what()); return 1; }
    catch (...) { fputs("验收失败: 未知异常\n", stderr); return 2; }
}

namespace {
using SchedulerClock = aliya::RenderScheduler::Clock;
using SchedulerState = aliya::SchedulerState;

struct SchedulerFrameCounts {
    uint64_t update = 0;
    uint64_t render = 0;
    uint64_t present = 0;
    uint64_t active = 0;
    uint64_t idle = 0;
    uint64_t deepIdle = 0;
    uint64_t sleep = 0;
    double delta = 0;
};

uint64_t& stateCount(SchedulerFrameCounts& counts, SchedulerState state) {
    switch (state) {
    case SchedulerState::Active: return counts.active;
    case SchedulerState::Idle: return counts.idle;
    case SchedulerState::DeepIdle: return counts.deepIdle;
    case SchedulerState::Sleep: return counts.sleep;
    }
    throw std::runtime_error("未知 Scheduler 状态");
}

void expectSchedulerFps(const aliya::SchedulerTick& tick, double activeFps) {
    const double expected = tick.state == SchedulerState::Active ? activeFps
        : tick.state == SchedulerState::Idle ? 10.0
        : tick.state == SchedulerState::DeepIdle ? 5.0 : 0.0;
    check(std::abs(tick.target_fps - expected) < 0.001, "Scheduler FPS 目标错误");
}

void driveScheduler(aliya::RenderScheduler& scheduler, SchedulerClock::time_point begin,
    SchedulerClock::time_point end, bool animating, double activeFps, SchedulerFrameCounts& counts) {
    for (auto now = begin; now <= end; now += std::chrono::milliseconds(1)) {
        const auto tick = scheduler.tick(now, animating);
        if (!tick.due) continue;
        expectSchedulerFps(tick, activeFps);
        ++counts.update; ++counts.render; ++counts.present;
        ++stateCount(counts, tick.state);
        counts.delta += tick.delta_seconds;
    }
}

void checkFrameTriplet(const SchedulerFrameCounts& counts) {
    check(counts.update == counts.render && counts.render == counts.present,
        "Update/Render/Present 未由同一 Scheduler frame 驱动");
}

void checkRange(uint64_t value, uint64_t minimum, uint64_t maximum, const char* message) {
    check(value >= minimum && value <= maximum, message);
}

struct SchedulerReport {
    SchedulerFrameCounts active60;
    SchedulerFrameCounts active30;
    SchedulerFrameCounts states;
    double clampedDelta = 0;
    double motionDelta60 = 0;
    double motionDelta30 = 0;
    aliya::SchedulerCounters counters{};
};

double simulateMotion(double activeFps) {
    aliya::SchedulerConfig config;
    config.active_fps = activeFps;
    aliya::RenderScheduler scheduler(config);
    const auto start = SchedulerClock::time_point{};
    scheduler.reset(start);
    SchedulerFrameCounts counts;
    driveScheduler(scheduler, start, start + std::chrono::seconds(3), true, activeFps, counts);
    checkFrameTriplet(counts);
    check(counts.delta > 2.90 && counts.delta <= 3.01, "Motion delta 未保持实际时间");
    return counts.delta;
}

SchedulerReport runSchedulerScenario() {
    static_assert(SchedulerClock::is_steady, "Scheduler 必须使用 monotonic clock");
    const auto start = SchedulerClock::time_point{};
    aliya::SchedulerConfig config;
    aliya::RenderScheduler scheduler(config);

    SchedulerReport report;
    scheduler.reset(start);
    driveScheduler(scheduler, start, start + std::chrono::seconds(1), true, 60, report.active60);
    checkFrameTriplet(report.active60);
    checkRange(report.active60.active, 59, 61, "ACTIVE 60 FPS 未受控");
    check(report.active60.idle == 0 && report.active60.deepIdle == 0
        && report.active60.sleep == 0, "ACTIVE 意外进入低频状态");

    scheduler.setActiveFps(30);
    scheduler.reset(start);
    driveScheduler(scheduler, start, start + std::chrono::seconds(1), true, 30, report.active30);
    checkFrameTriplet(report.active30);
    checkRange(report.active30.active, 29, 31, "ACTIVE 30 FPS 未受控");

    scheduler = aliya::RenderScheduler(config);
    scheduler.reset(start);
    driveScheduler(scheduler, start, start + std::chrono::milliseconds(3500), false, 60, report.states);
    checkFrameTriplet(report.states);
    checkRange(report.states.active, 59, 61, "ACTIVE 状态计数错误");
    checkRange(report.states.idle, 9, 11, "IDLE 10 FPS 未受控");
    checkRange(report.states.deepIdle, 4, 6, "DEEP_IDLE 5 FPS 未受控");
    check(report.states.sleep == 0 && scheduler.state() == SchedulerState::Sleep,
        "SLEEP 未停止提交");
    check(scheduler.counters().transition_count == 3, "状态 transition 计数错误");
    check(scheduler.counters().wakeup_count == 0, "未唤醒时 wakeup 计数错误");

    const auto wakeAt = start + std::chrono::milliseconds(3500);
    scheduler.wake(wakeAt);
    const auto wakeTick = scheduler.tick(wakeAt, false);
    check(wakeTick.due && wakeTick.delta_seconds == 0 && wakeTick.state == SchedulerState::Active,
        "Sleep → Wake 未重置 delta");
    check(scheduler.counters().transition_count == 4
        && scheduler.counters().wakeup_count == 1, "Wake 计数错误");
    report.counters = scheduler.counters();

    aliya::RenderScheduler clampScheduler(config);
    clampScheduler.reset(start);
    check(clampScheduler.tick(start, true).delta_seconds == 0, "首帧 delta 未归零");
    report.clampedDelta = clampScheduler.tick(start + std::chrono::seconds(1), true).delta_seconds;
    check(std::abs(report.clampedDelta - config.max_delta_seconds) < 0.001,
        "large delta 未 clamp");

    report.motionDelta60 = simulateMotion(60);
    report.motionDelta30 = simulateMotion(30);
    check(std::abs(report.motionDelta60 - report.motionDelta30) < 0.05,
        "FPS 变化改变 Motion 实际时间");
    return report;
}
}

extern "C" int native_scheduler_run(unsigned repeats) noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        check(repeats > 0, "Scheduler 重复次数必须大于零");
        SchedulerReport report;
        for (unsigned i = 0; i < repeats; ++i) report = runSchedulerScenario();
        printf("Phase 4 状态帧: ACTIVE=%llu, IDLE=%llu, DEEP_IDLE=%llu, SLEEP=%llu\n",
            static_cast<unsigned long long>(report.states.active),
            static_cast<unsigned long long>(report.states.idle),
            static_cast<unsigned long long>(report.states.deepIdle),
            static_cast<unsigned long long>(report.states.sleep));
        printf("Phase 4 频率: ACTIVE60=%llu, ACTIVE30=%llu, Update=%llu, Render=%llu, Present=%llu\n",
            static_cast<unsigned long long>(report.active60.active),
            static_cast<unsigned long long>(report.active30.active),
            static_cast<unsigned long long>(report.states.update),
            static_cast<unsigned long long>(report.states.render),
            static_cast<unsigned long long>(report.states.present));
        printf("Phase 4 时间: clamp=%.3f, motion60=%.3f, motion30=%.3f, transitions=%llu, wakeups=%llu\n",
            report.clampedDelta, report.motionDelta60, report.motionDelta30,
            static_cast<unsigned long long>(report.counters.transition_count),
            static_cast<unsigned long long>(report.counters.wakeup_count));
        printf("Phase 4 Scheduler 验收重复=%u\n", repeats);
        puts("Phase 4 Render Scheduler 验收通过");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "验收失败: %s\n", error.what()); return 1; }
    catch (...) { fputs("验收失败: 未知异常\n", stderr); return 2; }
}

namespace {
struct LifecycleCounters {
    unsigned update = 0;
    unsigned render = 0;
    unsigned present = 0;
};

void lifecycleShow(HWND window) {
    ShowWindow(window, SW_SHOW);
    // 隐藏控制台启动时第一次 ShowWindow 可能受启动参数影响，显式要求显示。
    check(SetWindowPos(window, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW) != FALSE,
        "显示窗口失败");
    UpdateWindow(window);
    pump();
    check(IsWindowVisible(window), "窗口未进入可见状态");
}

void lifecycleHide(HWND window) {
    ShowWindow(window, SW_HIDE);
    pump();
    check(!IsWindowVisible(window), "窗口未进入隐藏状态");
}

void lifecycleTick(AliyaRuntime* runtime, LifecycleCounters& counters) {
    ok(runtime_update(runtime, 1.f / 60)); ++counters.update;
    ok(runtime_render(runtime)); ++counters.render;
    ok(runtime_present(runtime)); ++counters.present;
}

void lifecycleResize(HWND window, AliyaRuntime* runtime, int width, int height,
    LifecycleCounters& counters) {
    check(SetWindowPos(window, nullptr, 0, 0, width, height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "调整窗口尺寸失败");
    pump();
    RECT client{};
    check(GetClientRect(window, &client) && client.right > 0 && client.bottom > 0,
        "读取 Client 尺寸失败");
    ok(runtime_resize(runtime, static_cast<uint32_t>(client.right), static_cast<uint32_t>(client.bottom)));
    uint8_t dirty = 0;
    ok(runtime_is_dirty(runtime, &dirty));
    check(dirty != 0, "Resize 未设置 dirty");
    lifecycleTick(runtime, counters);
}
}

extern "C" int native_window_lifecycle_run(const char* model,
    unsigned resize_cycles, unsigned visibility_cycles) noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        check(model && *model, "模型路径为空");
        check(resize_cycles > 0 && visibility_cycles > 0, "压力次数必须大于零");
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            check(GetLastError() == ERROR_ACCESS_DENIED, "进程 DPI 初始化失败");
        DpiScope dpi;
        WNDCLASSW wc{}; wc.style = CS_OWNDC; wc.lpfnWndProc = procedure;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AliyaPhase3Lifecycle";
        if (!RegisterClassW(&wc)) check(GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "窗口注册失败");

        Session session;
        session.window = CreateWindowExW(0, wc.lpszClassName, L"Aliya Native · Phase 3 Lifecycle",
            WS_OVERLAPPEDWINDOW, 120, 120, 660, 680, nullptr, nullptr, wc.hInstance, nullptr);
        check(session.window != nullptr, "窗口创建失败");
        lifecycleShow(session.window);
        puts("创建 Native Window / Runtime");
        ok(runtime_create(&session.runtime));
        ok(runtime_attach_window(session.runtime, session.window));
        RECT client{};
        check(GetClientRect(session.window, &client), "读取初始 Client 尺寸失败");
        ok(runtime_resize(session.runtime, static_cast<uint32_t>(client.right), static_cast<uint32_t>(client.bottom)));
        ok(model_load(session.runtime, model));

        LifecycleCounters counters;
        lifecycleTick(session.runtime, counters);
        const auto framesBeforeHide = counters.render;
        lifecycleHide(session.window);
        for (int i = 0; i < 10; ++i) { pump(); check(!IsWindowVisible(session.window), "隐藏窗口重新变为可见"); }
        check(counters.update == framesBeforeHide && counters.render == framesBeforeHide
            && counters.present == framesBeforeHide, "Hidden 仍提交 Native 帧");
        lifecycleShow(session.window);
        lifecycleTick(session.runtime, counters);
        check(counters.render > framesBeforeHide, "Show 未恢复 Native 渲染");

        RECT beforeMove{}, afterMove{};
        check(GetWindowRect(session.window, &beforeMove), "读取移动前位置失败");
        check(SetWindowPos(session.window, nullptr, beforeMove.left + 24, beforeMove.top + 18,
            0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "移动窗口失败");
        pump();
        check(GetWindowRect(session.window, &afterMove)
            && (afterMove.left != beforeMove.left || afterMove.top != beforeMove.top),
            "窗口位置未改变");

        for (unsigned i = 0; i < resize_cycles; ++i) {
            const int width = 640 + static_cast<int>(i % 9) * 17;
            const int height = 520 + static_cast<int>(i % 7) * 13;
            lifecycleResize(session.window, session.runtime, width, height, counters);
        }
        for (unsigned i = 0; i < visibility_cycles; ++i) {
            lifecycleShow(session.window);
            lifecycleTick(session.runtime, counters);
            const auto beforeHidden = counters.render;
            lifecycleHide(session.window);
            pump(); pump();
            check(counters.update == beforeHidden && counters.render == beforeHidden
                && counters.present == beforeHidden, "Show/Hide 隐藏段提交了 Native 帧");
        }

        printf("Phase 3 计数: resize=%u, show/hide=%u, update=%u, render=%u, present=%u\n",
            resize_cycles, visibility_cycles, counters.update, counters.render, counters.present);
        ok(model_unload(session.runtime));
        ok(runtime_destroy(&session.runtime));
        ok(runtime_destroy(&session.runtime));
        check(wglGetCurrentContext() == nullptr, "Runtime 销毁后仍保留 current Context");
        const HWND destroyedWindow = session.window;
        check(DestroyWindow(destroyedWindow) != FALSE, "窗口销毁失败");
        session.window = nullptr;
        check(!IsWindow(destroyedWindow), "窗口句柄仍有效");
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        puts("Phase 3 Native Window Lifecycle 验收通过");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "验收失败: %s\n", error.what()); return 1; }
    catch (...) { fputs("验收失败: 未知异常\n", stderr); return 2; }
}

namespace {
struct ResourceStressCounters {
    unsigned load_unload = 0;
    unsigned show_hide = 0;
    unsigned resize = 0;
    unsigned motion = 0;
    uint64_t update = 0;
    uint64_t render = 0;
    uint64_t present = 0;
};

void resourceFrame(AliyaRuntime* runtime, ResourceStressCounters& counters) {
    ok(runtime_update(runtime, 1.f / 60));
    ok(runtime_render(runtime));
    ok(runtime_present(runtime));
    ++counters.update;
    ++counters.render;
    ++counters.present;
}

void resourceResize(HWND window, AliyaRuntime* runtime, int width, int height,
    ResourceStressCounters& counters) {
    check(SetWindowPos(window, nullptr, 0, 0, width, height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "调整窗口尺寸失败");
    pump();
    RECT rect{};
    check(GetClientRect(window, &rect) && rect.right > 0 && rect.bottom > 0,
        "读取 Resize 后 Client 尺寸失败");
    ok(runtime_resize(runtime, static_cast<uint32_t>(rect.right), static_cast<uint32_t>(rect.bottom)));
    uint8_t dirty = 0;
    ok(runtime_is_dirty(runtime, &dirty));
    check(dirty != 0, "Resize 未设置 dirty");
    resourceFrame(runtime, counters);
    ++counters.resize;
}
}

extern "C" int native_resource_lifecycle_run(const char* model,
    unsigned load_cycles, unsigned show_hide_cycles, unsigned resize_cycles,
    unsigned motion_cycles) noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        check(model && *model, "模型路径为空");
        check(load_cycles > 0 && show_hide_cycles > 0 && resize_cycles > 0 && motion_cycles > 0,
            "资源压力次数必须大于零");
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            check(GetLastError() == ERROR_ACCESS_DENIED, "进程 DPI 初始化失败");
        DpiScope dpi;
        WNDCLASSW wc{}; wc.style = CS_OWNDC; wc.lpfnWndProc = procedure;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"AliyaPhase7ResourceLifecycle";
        if (!RegisterClassW(&wc)) check(GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "窗口注册失败");

        Session session;
        session.window = CreateWindowExW(0, wc.lpszClassName, L"Aliya Native · Phase 7 Resource Lifecycle",
            WS_OVERLAPPEDWINDOW, 120, 120, 660, 680, nullptr, nullptr, wc.hInstance, nullptr);
        check(session.window != nullptr, "窗口创建失败");
        lifecycleShow(session.window);
        puts("创建 Native Window / Runtime");
        ok(runtime_create(&session.runtime));
        ok(runtime_attach_window(session.runtime, session.window));
        RECT rect{};
        check(GetClientRect(session.window, &rect) && rect.right > 0 && rect.bottom > 0,
            "读取 Client 尺寸失败");
        ok(runtime_resize(session.runtime, static_cast<uint32_t>(rect.right), static_cast<uint32_t>(rect.bottom)));
        ok(model_load(session.runtime, model));

        ResourceStressCounters counters;
        resourceFrame(session.runtime, counters);

        // 临时 Model 成功后才替换旧 Model；循环覆盖 Model、Renderer、Texture、Motion 和 Expression 释放。
        for (unsigned i = 0; i < load_cycles; ++i) {
            ok(model_unload(session.runtime));
            ok(model_load(session.runtime, model));
            resourceFrame(session.runtime, counters);
            ++counters.load_unload;
        }

        lifecycleShow(session.window);
        for (unsigned i = 0; i < resize_cycles; ++i) {
            const int width = 640 + static_cast<int>(i % 11) * 19;
            const int height = 520 + static_cast<int>(i % 13) * 11;
            resourceResize(session.window, session.runtime, width, height, counters);
        }

        lifecycleShow(session.window);
        for (unsigned i = 0; i < motion_cycles; ++i) {
            ok(motion_start(session.runtime, "CAT_motion", 0));
            uint8_t animating = 0;
            ok(runtime_is_animating(session.runtime, &animating));
            check(animating != 0, "Motion Start 后未进入 animating");
            resourceFrame(session.runtime, counters);
            ok(motion_stop(session.runtime));
            ok(runtime_is_animating(session.runtime, &animating));
            check(animating == 0, "Motion Stop 后仍处于 animating");
            ++counters.motion;
        }

        for (unsigned i = 0; i < show_hide_cycles; ++i) {
            lifecycleShow(session.window);
            resourceFrame(session.runtime, counters);
            const auto rendered = counters.render;
            lifecycleHide(session.window);
            pump(); pump();
            check(!IsWindowVisible(session.window), "隐藏循环中窗口重新可见");
            check(counters.render == rendered && counters.present == rendered,
                "Show/Hide 隐藏段提交了额外 Native 帧");
            ++counters.show_hide;
        }

        // 显式 double-unload、Render-after-unload 和 double-destroy，验证释放 API 的幂等边界。
        ok(model_unload(session.runtime));
        ok(model_unload(session.runtime));
        check(runtime_update(session.runtime, 0) == ALIYA_INVALID_STATE,
            "最终 Unload 后 Update 未拒绝");
        ok(runtime_render(session.runtime));
        ok(runtime_present(session.runtime));
        ok(runtime_destroy(&session.runtime));
        ok(runtime_destroy(&session.runtime));
        check(wglGetCurrentContext() == nullptr, "Runtime Destroy 后仍保留 current Context");
        const HWND destroyedWindow = session.window;
        check(DestroyWindow(destroyedWindow) != FALSE, "窗口销毁失败");
        session.window = nullptr;
        check(!IsWindow(destroyedWindow), "窗口句柄仍有效");
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        printf("Phase 7 压力: load/unload=%u, show/hide=%u, resize=%u, motion=%u\n",
            counters.load_unload, counters.show_hide, counters.resize, counters.motion);
        printf("Phase 7 帧: Update=%llu, Render=%llu, Present=%llu\n",
            static_cast<unsigned long long>(counters.update),
            static_cast<unsigned long long>(counters.render),
            static_cast<unsigned long long>(counters.present));
        puts("Phase 7 Native Resource Lifecycle 验收通过");
        // 给外部采样器一个 post-destroy 窗口，避免进程退出太快而错过最终内存值。
        Sleep(500);
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "验收失败: %s\n", error.what()); return 1; }
    catch (...) { fputs("验收失败: 未知异常\n", stderr); return 2; }
}

namespace {
struct DirtySleepWaiter {
    std::future<aliya::WaitResult> result;
    std::thread thread;
};

DirtySleepWaiter beginDirtySleepWait(aliya::DirtySleepController& controller) {
    std::promise<aliya::WaitResult> promise;
    auto result = promise.get_future();
    std::thread thread([&controller, promise = std::move(promise)]() mutable {
        promise.set_value(controller.waitForFrame());
    });
    return {std::move(result), std::move(thread)};
}

void finishFrame(DirtySleepWaiter& waiter, const char* message) {
    check(waiter.result.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready,
        message);
    check(waiter.result.get() == aliya::WaitResult::Frame, "Dirty 唤醒未返回 Frame");
    waiter.thread.join();
}

struct DirtySleepReport {
    aliya::DirtySleepCounters counters{};
    uint64_t presented_frames = 0;
    uint64_t static_frames = 0;
};

DirtySleepReport runDirtySleepScenario() {
    aliya::SchedulerConfig config;
    config.active_fps = 60;
    config.idle_fps = 10;
    config.deep_idle_fps = 5;
    // 验收缩短状态超时，避免为进入 SLEEP 引入数秒等待；生产默认值仍由 SchedulerConfig 提供。
    config.idle_after = std::chrono::milliseconds(20);
    config.deep_idle_after = std::chrono::milliseconds(40);
    config.sleep_after = std::chrono::milliseconds(70);
    aliya::DirtySleepController controller(config);

    check(controller.waitForFrame() == aliya::WaitResult::Frame, "初始 dirty 未提交 Frame");
    controller.framePresented();
    check(!controller.dirty(), "Frame Presented 后 dirty 未清除");
    const auto initial = controller.counters().frame_count;

    auto staticWait = beginDirtySleepWait(controller);
    for (int i = 0; i < 150 && !controller.sleeping(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    check(controller.sleeping(), "静态状态未进入条件变量 Sleep");
    check(staticWait.result.wait_for(std::chrono::milliseconds(40)) == std::future_status::timeout,
        "静态 Sleep 仍提交 Update/Render/Present");
    const auto staticFrames = controller.counters().frame_count;
    check(staticFrames == initial, "静态 Sleep 增加了 frame_count");

    const std::pair<const char*, aliya::DirtySource> sources[] = {
        {"Parameter changed", aliya::DirtySource::ParameterChanged},
        {"Motion advanced", aliya::DirtySource::MotionAdvanced},
        {"Expression changed", aliya::DirtySource::ExpressionChanged},
        {"Resize", aliya::DirtySource::Resize},
        {"Model Load", aliya::DirtySource::ModelLoad},
    };
    for (const auto& [name, source] : sources) {
        controller.markDirty(source);
        finishFrame(staticWait, (std::string("Dirty source 未唤醒: ") + name).c_str());
        controller.framePresented();
        check(!controller.dirty(), "Dirty source Frame 后 dirty 未清除");
        staticWait = beginDirtySleepWait(controller);
    }

    controller.setVisible(false);
    check(controller.dirty(), "Visibility changed 未设置 dirty");
    auto hiddenWait = std::move(staticWait);
    check(hiddenWait.result.wait_for(std::chrono::milliseconds(40)) == std::future_status::timeout,
        "隐藏状态错误地提交了 Frame");
    const auto hiddenFrames = controller.counters().frame_count;
    controller.setVisible(true);
    finishFrame(hiddenWait, "Show 未唤醒 Dirty Sleep");
    controller.framePresented();
    check(controller.counters().frame_count == hiddenFrames + 1, "Show 未只提交一帧");

    controller.setAnimating(true);
    auto animationWait = beginDirtySleepWait(controller);
    finishFrame(animationWait, "Animating 未唤醒 Render Scheduler");
    controller.framePresented();
    controller.setAnimating(false);

    auto shutdownWait = beginDirtySleepWait(controller);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    controller.shutdown();
    check(shutdownWait.result.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready,
        "Shutdown 未唤醒阻塞线程");
    check(shutdownWait.result.get() == aliya::WaitResult::Shutdown, "Shutdown 返回值错误");
    shutdownWait.thread.join();

    DirtySleepReport report;
    report.counters = controller.counters();
    report.presented_frames = report.counters.frame_count;
    report.static_frames = staticFrames;
    check(report.counters.dirty_event_count >= 7, "Dirty 来源计数不完整");
    check(report.counters.sleep_enter_count >= 1 && report.counters.sleep_exit_count >= 1,
        "Sleep enter/exit 未记录");
    check(report.counters.wakeup_count >= 8, "Wake source 未触发 Scheduler wake");
    return report;
}
}

extern "C" int native_dirty_sleep_run(unsigned repeats) noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        check(repeats > 0, "Dirty Sleep 重复次数必须大于零");
        DirtySleepReport report;
        for (unsigned i = 0; i < repeats; ++i) report = runDirtySleepScenario();
        printf("Phase 5 静态帧: before=%llu, after=%llu；Wake latency=%llu us\n",
            static_cast<unsigned long long>(report.static_frames),
            static_cast<unsigned long long>(report.presented_frames),
            static_cast<unsigned long long>(report.counters.last_wake_latency_us));
        printf("Phase 5 Sleep: enter=%llu, exit=%llu, wakeups=%llu, dirty_events=%llu\n",
            static_cast<unsigned long long>(report.counters.sleep_enter_count),
            static_cast<unsigned long long>(report.counters.sleep_exit_count),
            static_cast<unsigned long long>(report.counters.wakeup_count),
            static_cast<unsigned long long>(report.counters.dirty_event_count));
        puts("Phase 5 Dirty Rendering + 0 FPS Sleep 验收通过");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "验收失败: %s\n", error.what()); return 1; }
    catch (...) { fputs("验收失败: 未知异常\n", stderr); return 2; }
}

namespace {
struct InputReport {
    unsigned mouse_moves = 0;
    aliya::InputCounters input{};
    aliya::DirtySleepCounters scheduler{};
    uint64_t frame_count = 0;
    uint64_t discrete_event_count = 0;
};

InputReport runInputScenario(unsigned mouseMoves) {
    check(mouseMoves >= 500, "MouseMove 压力次数过低");
    aliya::InputState input;
    aliya::DirtySleepController controller;

    check(controller.waitForFrame() == aliya::WaitResult::Frame, "输入验收初始 Frame 失败");
    controller.framePresented();

    auto mouseWait = beginDirtySleepWait(controller);
    std::thread mouseProducer([&] {
        for (unsigned i = 0; i < mouseMoves; ++i) {
            if (input.publishMouseMove(static_cast<double>(i) + 0.25, -static_cast<double>(i) * 0.5))
                controller.markDirty(aliya::DirtySource::MouseMove);
        }
    });
    finishFrame(mouseWait, "MouseMove 未唤醒 Scheduler");
    mouseProducer.join();
    auto mouseSnapshot = input.consume();
    check(mouseSnapshot.has_mouse_move, "MouseMove latest state 未消费");
    check(mouseSnapshot.mouse_x == static_cast<double>(mouseMoves - 1) + 0.25
        && mouseSnapshot.mouse_y == -static_cast<double>(mouseMoves - 1) * 0.5,
        "MouseMove 未保留最后坐标");
    check(mouseSnapshot.discrete_events.empty(), "MouseMove 错误进入离散事件队列");
    controller.framePresented();

    check(!input.hasPending(), "MouseMove 消费后仍有 pending");
    check(input.publishMouseMove(123.5, -456.25), "下一批 MouseMove 未重新请求 Wake");
    controller.markDirty(aliya::DirtySource::MouseMove);
    auto nextMouseWait = beginDirtySleepWait(controller);
    finishFrame(nextMouseWait, "第二批 MouseMove 未唤醒 Scheduler");
    const auto nextMouseSnapshot = input.consume();
    check(nextMouseSnapshot.has_mouse_move && nextMouseSnapshot.mouse_x == 123.5
        && nextMouseSnapshot.mouse_y == -456.25, "第二批 MouseMove 坐标错误");
    controller.framePresented();

    auto discreteWait = beginDirtySleepWait(controller);
    std::thread pressProducer([&] {
        if (input.publishKeyboard(0x41, true)) controller.markDirty(aliya::DirtySource::Keyboard);
        input.publishKeyboard(0x42, true);
        input.publishKeyboard(0x10, true);
        input.publishMouseButton(aliya::MouseButton::Left, true);
        input.publishMouseButton(aliya::MouseButton::Right, true);
    });
    pressProducer.join();
    check(input.keyDown(0x41) && input.keyDown(0x42) && input.keyDown(0x10),
        "多键 Press 状态丢失");
    check((input.mouseButtons() & 0x3) == 0x3, "多鼠标按键状态丢失");
    std::thread releaseProducer([&] {
        input.publishKeyboard(0x41, false);
        input.publishKeyboard(0x42, false);
        input.publishKeyboard(0x10, false);
        input.publishMouseButton(aliya::MouseButton::Left, false);
        input.publishMouseButton(aliya::MouseButton::Right, false);
    });
    releaseProducer.join();
    finishFrame(discreteWait, "离散输入未唤醒 Scheduler");
    const auto discreteSnapshot = input.consume();
    check(discreteSnapshot.discrete_events.size() == 10, "Keyboard/Mouse Press Release 事件丢失");
    for (size_t i = 0; i < discreteSnapshot.discrete_events.size(); ++i)
        check(discreteSnapshot.discrete_events[i].sequence == i, "离散输入事件顺序错误");
    check(!input.keyDown(0x41) && !input.keyDown(0x42) && !input.keyDown(0x10),
        "快速 Press/Release 产生 stuck key");
    check(input.mouseButtons() == 0, "快速 Press/Release 产生 stuck mouse button");
    controller.framePresented();

    const auto inputCounters = input.counters();
    const auto schedulerCounters = controller.counters();
    check(inputCounters.raw_mouse_move_count == static_cast<uint64_t>(mouseMoves) + 1,
        "Raw MouseMove 计数错误");
    check(inputCounters.consumed_mouse_state_count == 2, "Mouse state 消费次数错误");
    check(inputCounters.keyboard_event_count == 6 && inputCounters.mouse_button_event_count == 4,
        "离散输入计数错误");
    check(inputCounters.wake_request_count == 3 && schedulerCounters.wakeup_count == 3,
        "输入 Wake 未合并");
    check(schedulerCounters.dirty_event_count == 3 && schedulerCounters.frame_count == 4,
        "输入导致 Scheduler frame 线性增长");
    check(!input.hasPending(), "全部输入消费后仍有 pending");

    return {mouseMoves, inputCounters, schedulerCounters,
        schedulerCounters.frame_count, static_cast<uint64_t>(discreteSnapshot.discrete_events.size())};
}
}

extern "C" int native_input_state_run(unsigned repeats) noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        check(repeats > 0, "Input State 重复次数必须大于零");
        InputReport report;
        for (unsigned repeat = 0; repeat < repeats; ++repeat) {
            for (const unsigned mouseMoves : {500u, 1000u, 2000u, 4000u, 8000u}) {
                report = runInputScenario(mouseMoves);
                printf("Phase 6 压力: raw=%u, consumed=%llu, frames=%llu, wake_requests=%llu, discrete=%llu\n",
                    report.mouse_moves,
                    static_cast<unsigned long long>(report.input.consumed_mouse_state_count),
                    static_cast<unsigned long long>(report.frame_count),
                    static_cast<unsigned long long>(report.input.wake_request_count),
                    static_cast<unsigned long long>(report.discrete_event_count));
            }
        }
        puts("Phase 6 Native Input State 验收通过");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "验收失败: %s\n", error.what()); return 1; }
    catch (...) { fputs("验收失败: 未知异常\n", stderr); return 2; }
}
