#pragma once

#include "scheduler.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace aliya {

enum class DirtySource : uint8_t {
    ParameterChanged,
    MotionAdvanced,
    ExpressionChanged,
    Resize,
    ModelLoad,
    VisibilityChanged,
};

enum class WaitResult : uint8_t {
    Frame,
    Shutdown,
};

struct DirtySleepCounters {
    uint64_t dirty_event_count = 0;
    uint64_t frame_count = 0;
    uint64_t sleep_enter_count = 0;
    uint64_t sleep_exit_count = 0;
    uint64_t wakeup_count = 0;
    uint64_t last_wake_latency_us = 0;
};

class DirtySleepController final {
public:
    using Clock = RenderScheduler::Clock;

    explicit DirtySleepController(SchedulerConfig config = {});

    WaitResult waitForFrame();
    void framePresented();
    void markDirty(DirtySource source);
    void setAnimating(bool animating);
    void setVisible(bool visible);
    void notifyWake();
    void shutdown();

    bool dirty() const;
    bool sleeping() const;
    DirtySleepCounters counters() const;

private:
    void signalWakeLocked(Clock::time_point now);
    bool wakePredicateLocked() const;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    RenderScheduler scheduler_;
    bool dirty_ = true;
    bool animating_ = false;
    bool visible_ = true;
    bool stopped_ = false;
    bool sleeping_ = false;
    Clock::time_point sleep_entered_at_{};
    Clock::time_point wake_requested_at_{};
    DirtySleepCounters counters_{};
};

}
