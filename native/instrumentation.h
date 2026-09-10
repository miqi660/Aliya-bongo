#pragma once

#include <atomic>
#include <cstdint>

namespace aliya {

struct InstrumentationSnapshot {
    uint64_t update_count = 0;
    uint64_t render_count = 0;
    uint64_t present_count = 0;
    uint64_t input_count = 0;
    uint64_t mouse_move_raw_count = 0;
    uint64_t mouse_move_consume_count = 0;
    uint64_t scheduler_wakeup_count = 0;
    uint64_t sleep_enter_count = 0;
    uint64_t sleep_exit_count = 0;
};

class RenderInstrumentation final {
public:
    explicit RenderInstrumentation(bool enabled = true) noexcept : enabled_(enabled) {}

    RenderInstrumentation(const RenderInstrumentation&) = delete;
    RenderInstrumentation& operator=(const RenderInstrumentation&) = delete;

    void setEnabled(bool enabled) noexcept { enabled_.store(enabled, std::memory_order_relaxed); }
    bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }
    void reset() noexcept;

    void recordUpdate(uint64_t count = 1) noexcept;
    void recordRender(uint64_t count = 1) noexcept;
    void recordPresent(uint64_t count = 1) noexcept;
    void recordInput(uint64_t count = 1) noexcept;
    void recordMouseMoveRaw(uint64_t count = 1) noexcept;
    void recordMouseMoveConsumed(uint64_t count = 1) noexcept;
    void recordSchedulerWakeup(uint64_t count = 1) noexcept;
    void recordSleepEnter(uint64_t count = 1) noexcept;
    void recordSleepExit(uint64_t count = 1) noexcept;

    InstrumentationSnapshot snapshot() const noexcept;

private:
    template <typename Counter>
    void add(Counter& counter, uint64_t count) noexcept {
        if (enabled()) counter.fetch_add(count, std::memory_order_relaxed);
    }

    std::atomic<bool> enabled_;
    std::atomic<uint64_t> update_count_{0};
    std::atomic<uint64_t> render_count_{0};
    std::atomic<uint64_t> present_count_{0};
    std::atomic<uint64_t> input_count_{0};
    std::atomic<uint64_t> mouse_move_raw_count_{0};
    std::atomic<uint64_t> mouse_move_consume_count_{0};
    std::atomic<uint64_t> scheduler_wakeup_count_{0};
    std::atomic<uint64_t> sleep_enter_count_{0};
    std::atomic<uint64_t> sleep_exit_count_{0};
};

}
