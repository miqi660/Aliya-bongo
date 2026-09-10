#include "input_state.h"

#include <cmath>

namespace aliya {

bool InputState::hasPendingLocked() const {
    return mouse_pending_ || !discrete_events_.empty();
}

bool InputState::requestWakeLocked() {
    if (hasPendingLocked()) return false;
    ++counters_.wake_request_count;
    return true;
}

bool InputState::publishMouseMove(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    std::lock_guard lock(mutex_);
    const bool wake = requestWakeLocked();
    latest_mouse_x_ = x;
    latest_mouse_y_ = y;
    mouse_pending_ = true;
    ++counters_.raw_mouse_move_count;
    return wake;
}

bool InputState::publishKeyboard(uint16_t key, bool pressed) {
    std::lock_guard lock(mutex_);
    const bool wake = requestWakeLocked();
    keyboard_state_.set(key, pressed);
    discrete_events_.push_back({InputEventKind::Keyboard, key, pressed, next_sequence_++});
    ++counters_.keyboard_event_count;
    return wake;
}

bool InputState::publishMouseButton(MouseButton button, bool pressed) {
    const auto index = static_cast<uint8_t>(button);
    if (index >= 32) return false;
    std::lock_guard lock(mutex_);
    const bool wake = requestWakeLocked();
    const uint32_t mask = uint32_t{1} << index;
    if (pressed) mouse_buttons_ |= mask;
    else mouse_buttons_ &= ~mask;
    discrete_events_.push_back({InputEventKind::MouseButton, index, pressed, next_sequence_++});
    ++counters_.mouse_button_event_count;
    return wake;
}

InputSnapshot InputState::consume() {
    std::lock_guard lock(mutex_);
    InputSnapshot snapshot;
    if (mouse_pending_) {
        snapshot.has_mouse_move = true;
        snapshot.mouse_x = latest_mouse_x_;
        snapshot.mouse_y = latest_mouse_y_;
        mouse_pending_ = false;
        ++counters_.consumed_mouse_state_count;
    }
    snapshot.discrete_events.reserve(discrete_events_.size());
    while (!discrete_events_.empty()) {
        snapshot.discrete_events.push_back(discrete_events_.front());
        discrete_events_.pop_front();
    }
    return snapshot;
}

bool InputState::hasPending() const {
    std::lock_guard lock(mutex_);
    return hasPendingLocked();
}

bool InputState::keyDown(uint16_t key) const {
    std::lock_guard lock(mutex_);
    return keyboard_state_.test(key);
}

uint32_t InputState::mouseButtons() const {
    std::lock_guard lock(mutex_);
    return mouse_buttons_;
}

InputCounters InputState::counters() const {
    std::lock_guard lock(mutex_);
    return counters_;
}

}
