#pragma once

#include <chrono>
#include <cstdint>

namespace aliya {

enum class SchedulerState : uint8_t {
    Active,
    Idle,
    DeepIdle,
    Sleep,
};

struct SchedulerConfig {
    double active_fps = 60.0;
    double idle_fps = 10.0;
    double deep_idle_fps = 5.0;
    std::chrono::milliseconds idle_after{1000};
    std::chrono::milliseconds deep_idle_after{2000};
    std::chrono::milliseconds sleep_after{3000};
    double max_delta_seconds = 0.25;
};

struct SchedulerTick {
    SchedulerState state = SchedulerState::Active;
    bool due = false;
    double delta_seconds = 0.0;
    double target_fps = 0.0;
};

struct SchedulerCounters {
    uint64_t transition_count = 0;
    uint64_t wakeup_count = 0;
};

class RenderScheduler final {
public:
    using Clock = std::chrono::steady_clock;

    explicit RenderScheduler(SchedulerConfig config = {});

    void reset(Clock::time_point now);
    void wake(Clock::time_point now);
    SchedulerTick tick(Clock::time_point now, bool animating);
    void setActiveFps(double fps);

    SchedulerState state() const { return state_; }
    double activeFps() const { return config_.active_fps; }
    const SchedulerConfig& config() const { return config_; }
    const SchedulerCounters& counters() const { return counters_; }

private:
    static double seconds(Clock::duration duration);
    static void validateConfig(const SchedulerConfig& config);
    double targetFps() const;
    void transitionForElapsed(Clock::time_point now);
    void transitionTo(SchedulerState state, Clock::time_point now);

    SchedulerConfig config_;
    SchedulerCounters counters_{};
    SchedulerState state_ = SchedulerState::Active;
    Clock::time_point last_activity_{};
    Clock::time_point last_frame_{};
    Clock::time_point next_frame_{};
    bool initialized_ = false;
    bool delta_reset_ = false;
};

const char* schedulerStateName(SchedulerState state);

}
