#pragma once

#include <bitset>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace aliya {

enum class InputEventKind : uint8_t {
    Keyboard,
    MouseButton,
};

enum class MouseButton : uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
};

struct InputEvent {
    InputEventKind kind = InputEventKind::Keyboard;
    uint16_t code = 0;
    bool pressed = false;
    uint64_t sequence = 0;
};

struct InputSnapshot {
    bool has_mouse_move = false;
    double mouse_x = 0.0;
    double mouse_y = 0.0;
    std::vector<InputEvent> discrete_events;
};

struct InputCounters {
    uint64_t raw_mouse_move_count = 0;
    uint64_t consumed_mouse_state_count = 0;
    uint64_t keyboard_event_count = 0;
    uint64_t mouse_button_event_count = 0;
    uint64_t wake_request_count = 0;
};

class InputState final {
public:
    // 返回 true 表示这次 publish 将 pending 从空变为非空，需要唤醒 Scheduler。
    bool publishMouseMove(double x, double y);
    bool publishKeyboard(uint16_t key, bool pressed);
    bool publishMouseButton(MouseButton button, bool pressed);

    // 只允许 Render/Update 消费线程调用；一次消费取走最新坐标和全部离散事件。
    InputSnapshot consume();

    bool hasPending() const;
    bool keyDown(uint16_t key) const;
    uint32_t mouseButtons() const;
    InputCounters counters() const;

private:
    bool hasPendingLocked() const;
    bool requestWakeLocked();

    mutable std::mutex mutex_;
    bool mouse_pending_ = false;
    double latest_mouse_x_ = 0.0;
    double latest_mouse_y_ = 0.0;
    std::bitset<65536> keyboard_state_;
    uint32_t mouse_buttons_ = 0;
    uint64_t next_sequence_ = 0;
    std::deque<InputEvent> discrete_events_;
    InputCounters counters_{};
};

}
