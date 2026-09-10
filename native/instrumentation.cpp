#include "instrumentation.h"

namespace aliya {

void RenderInstrumentation::reset() noexcept {
    update_count_.store(0, std::memory_order_relaxed);
    render_count_.store(0, std::memory_order_relaxed);
    present_count_.store(0, std::memory_order_relaxed);
    input_count_.store(0, std::memory_order_relaxed);
    mouse_move_raw_count_.store(0, std::memory_order_relaxed);
    mouse_move_consume_count_.store(0, std::memory_order_relaxed);
    scheduler_wakeup_count_.store(0, std::memory_order_relaxed);
    sleep_enter_count_.store(0, std::memory_order_relaxed);
    sleep_exit_count_.store(0, std::memory_order_relaxed);
}

void RenderInstrumentation::recordUpdate(uint64_t count) noexcept { add(update_count_, count); }
void RenderInstrumentation::recordRender(uint64_t count) noexcept { add(render_count_, count); }
void RenderInstrumentation::recordPresent(uint64_t count) noexcept { add(present_count_, count); }
void RenderInstrumentation::recordInput(uint64_t count) noexcept { add(input_count_, count); }
void RenderInstrumentation::recordMouseMoveRaw(uint64_t count) noexcept { add(mouse_move_raw_count_, count); }
void RenderInstrumentation::recordMouseMoveConsumed(uint64_t count) noexcept {
    add(mouse_move_consume_count_, count);
}
void RenderInstrumentation::recordSchedulerWakeup(uint64_t count) noexcept {
    add(scheduler_wakeup_count_, count);
}
void RenderInstrumentation::recordSleepEnter(uint64_t count) noexcept {
    add(sleep_enter_count_, count);
}
void RenderInstrumentation::recordSleepExit(uint64_t count) noexcept {
    add(sleep_exit_count_, count);
}

InstrumentationSnapshot RenderInstrumentation::snapshot() const noexcept {
    return {
        update_count_.load(std::memory_order_relaxed),
        render_count_.load(std::memory_order_relaxed),
        present_count_.load(std::memory_order_relaxed),
        input_count_.load(std::memory_order_relaxed),
        mouse_move_raw_count_.load(std::memory_order_relaxed),
        mouse_move_consume_count_.load(std::memory_order_relaxed),
        scheduler_wakeup_count_.load(std::memory_order_relaxed),
        sleep_enter_count_.load(std::memory_order_relaxed),
        sleep_exit_count_.load(std::memory_order_relaxed),
    };
}

}
