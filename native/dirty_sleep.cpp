#include "dirty_sleep.h"

#include <algorithm>

namespace aliya {

DirtySleepController::DirtySleepController(SchedulerConfig config) : scheduler_(config) {
    scheduler_.reset(Clock::now());
}

bool DirtySleepController::wakePredicateLocked() const {
    return stopped_ || dirty_ || animating_ || !visible_;
}

void DirtySleepController::signalWakeLocked(Clock::time_point now) {
    scheduler_.wake(now);
    ++counters_.wakeup_count;
    if (sleeping_) wake_requested_at_ = now;
    condition_.notify_one();
}

WaitResult DirtySleepController::waitForFrame() {
    std::unique_lock lock(mutex_);
    for (;;) {
        if (stopped_) return WaitResult::Shutdown;
        if (!visible_) {
            // 隐藏窗口即使收到 dirty 事件也不能提交帧；只等待重新显示或关闭。
            condition_.wait(lock, [&] { return stopped_ || visible_; });
            continue;
        }
        const auto now = Clock::now();
        const auto tick = scheduler_.tick(now, animating_ || dirty_);
        if (tick.due && (dirty_ || animating_)) return WaitResult::Frame;

        if (!dirty_ && !animating_ && scheduler_.state() == SchedulerState::Sleep) {
            if (!sleeping_) {
                sleeping_ = true;
                sleep_entered_at_ = now;
                ++counters_.sleep_enter_count;
            }
            condition_.wait(lock, [&] { return wakePredicateLocked(); });
            if (sleeping_) {
                sleeping_ = false;
                ++counters_.sleep_exit_count;
                if (wake_requested_at_ != Clock::time_point{}) {
                    counters_.last_wake_latency_us = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            Clock::now() - wake_requested_at_).count());
                    wake_requested_at_ = {};
                }
            }
            continue;
        }

        const auto deadline = scheduler_.nextDeadline(now);
        if (deadline == Clock::time_point::max()) {
            condition_.wait(lock, [&] { return wakePredicateLocked(); });
        } else {
            condition_.wait_until(lock, deadline, [&] { return wakePredicateLocked(); });
        }
    }
}

void DirtySleepController::framePresented() {
    std::lock_guard lock(mutex_);
    dirty_ = false;
    ++counters_.frame_count;
}

void DirtySleepController::markDirty(DirtySource) {
    std::lock_guard lock(mutex_);
    dirty_ = true;
    ++counters_.dirty_event_count;
    signalWakeLocked(Clock::now());
}

void DirtySleepController::setAnimating(bool animating) {
    std::lock_guard lock(mutex_);
    if (animating_ == animating) return;
    animating_ = animating;
    if (animating_) signalWakeLocked(Clock::now());
    else condition_.notify_one();
}

void DirtySleepController::setVisible(bool visible) {
    std::lock_guard lock(mutex_);
    if (visible_ == visible) return;
    visible_ = visible;
    dirty_ = true;
    ++counters_.dirty_event_count;
    signalWakeLocked(Clock::now());
}

void DirtySleepController::notifyWake() {
    std::lock_guard lock(mutex_);
    signalWakeLocked(Clock::now());
}

void DirtySleepController::shutdown() {
    std::lock_guard lock(mutex_);
    stopped_ = true;
    condition_.notify_all();
}

bool DirtySleepController::dirty() const {
    std::lock_guard lock(mutex_);
    return dirty_;
}

bool DirtySleepController::sleeping() const {
    std::lock_guard lock(mutex_);
    return sleeping_;
}

DirtySleepCounters DirtySleepController::counters() const {
    std::lock_guard lock(mutex_);
    return counters_;
}

}
