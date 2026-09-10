#include "scheduler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aliya {

RenderScheduler::RenderScheduler(SchedulerConfig config) : config_(config) {
    validateConfig(config_);
}

double RenderScheduler::seconds(Clock::duration duration) {
    return std::chrono::duration<double>(duration).count();
}

void RenderScheduler::validateConfig(const SchedulerConfig& config) {
    if (!std::isfinite(config.active_fps) || !std::isfinite(config.idle_fps)
        || !std::isfinite(config.deep_idle_fps) || config.active_fps <= 0
        || config.idle_fps <= 0 || config.deep_idle_fps <= 0
        || !std::isfinite(config.max_delta_seconds) || config.max_delta_seconds <= 0
        || config.idle_after.count() <= 0 || config.deep_idle_after <= config.idle_after
        || config.sleep_after <= config.deep_idle_after) {
        throw std::invalid_argument("RenderScheduler 配置无效");
    }
}

void RenderScheduler::reset(Clock::time_point now) {
    initialized_ = true;
    state_ = SchedulerState::Active;
    counters_ = {};
    last_activity_ = now;
    last_frame_ = now;
    next_frame_ = now;
    delta_reset_ = true;
}

void RenderScheduler::setActiveFps(double fps) {
    if (!std::isfinite(fps) || fps <= 0) throw std::invalid_argument("ACTIVE FPS 无效");
    config_.active_fps = fps;
    if (initialized_ && state_ == SchedulerState::Active) next_frame_ = last_frame_;
}

double RenderScheduler::targetFps() const {
    switch (state_) {
    case SchedulerState::Active: return config_.active_fps;
    case SchedulerState::Idle: return config_.idle_fps;
    case SchedulerState::DeepIdle: return config_.deep_idle_fps;
    case SchedulerState::Sleep: return 0.0;
    }
    return 0.0;
}

void RenderScheduler::transitionTo(SchedulerState state, Clock::time_point now) {
    if (state_ == state) return;
    state_ = state;
    ++counters_.transition_count;
    // 状态改变后立即允许一次新状态帧，随后按该状态 FPS 排期。
    if (state_ != SchedulerState::Sleep) next_frame_ = now;
}

void RenderScheduler::transitionForElapsed(Clock::time_point now) {
    const auto elapsed = now - last_activity_;
    SchedulerState desired = SchedulerState::Active;
    if (elapsed >= config_.sleep_after) desired = SchedulerState::Sleep;
    else if (elapsed >= config_.deep_idle_after) desired = SchedulerState::DeepIdle;
    else if (elapsed >= config_.idle_after) desired = SchedulerState::Idle;
    transitionTo(desired, now);
}

void RenderScheduler::wake(Clock::time_point now) {
    if (!initialized_) reset(now);
    ++counters_.wakeup_count;
    last_activity_ = now;
    transitionTo(SchedulerState::Active, now);
    // Sleep/Wake 后第一帧不把休眠时长交给 Cubism，避免 Motion 突跳。
    last_frame_ = now;
    next_frame_ = now;
    delta_reset_ = true;
}

SchedulerTick RenderScheduler::tick(Clock::time_point now, bool animating) {
    if (!initialized_) reset(now);
    if (now < last_frame_) now = last_frame_;
    if (now < last_activity_) now = last_activity_;

    if (animating) {
        last_activity_ = now;
        if (state_ != SchedulerState::Active) wake(now);
    } else {
        transitionForElapsed(now);
    }

    SchedulerTick result{state_, false, 0.0, targetFps()};
    if (state_ == SchedulerState::Sleep || now < next_frame_) return result;

    result.due = true;
    if (delta_reset_) {
        delta_reset_ = false;
    } else {
        result.delta_seconds = std::clamp(seconds(now - last_frame_), 0.0, config_.max_delta_seconds);
    }
    last_frame_ = now;
    const double fps = targetFps();
    next_frame_ = now + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / fps));
    return result;
}

const char* schedulerStateName(SchedulerState state) {
    switch (state) {
    case SchedulerState::Active: return "ACTIVE";
    case SchedulerState::Idle: return "IDLE";
    case SchedulerState::DeepIdle: return "DEEP_IDLE";
    case SchedulerState::Sleep: return "SLEEP";
    }
    return "UNKNOWN";
}

}
