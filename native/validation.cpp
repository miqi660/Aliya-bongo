// Phase 2 专用验收驱动：只在 example 调用，生产 Runtime 不调用读回或测试窗口。
#include "runtime.h"
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
