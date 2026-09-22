#pragma once
#include "lighting_config.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace lcars {
enum class State { UNKNOWN, OFF, ON, UNAVAILABLE };
inline State parse_state(const std::string &value) {
  if (value == "on") return State::ON;
  if (value == "off") return State::OFF;
  if (value == "unavailable") return State::UNAVAILABLE;
  return State::UNKNOWN;
}
struct Control {
  State state{State::UNKNOWN};
  bool pending{false};
  bool failed{false};
  uint32_t requested_at{0};
  State expected{State::UNKNOWN};
};
class ControllerState {
 public:
  void set_connected(bool connected) {
    if (!connected) controls_ = {};
    connected_ = connected;
  }
  bool connected() const { return connected_; }
  const Control &at(size_t index) const { return controls_.at(index); }
  void update(size_t index, const std::string &value) {
    if (!connected_ || index >= CONTROL_COUNT) return;
    auto &control = controls_[index];
    control.state = parse_state(value);
    if (!control.pending || control.state == control.expected) {
      control.pending = false;
      control.failed = false;
    }
  }
  bool can_control(size_t index) const {
    if (index >= CONTROL_COUNT || !connected_) return false;
    const auto &c = controls_[index];
    return !c.pending && (c.state == State::ON || c.state == State::OFF);
  }
  bool begin(size_t index, uint32_t now) {
    if (!can_control(index)) return false;
    auto &c = controls_[index];
    c.pending = true;
    c.failed = false;
    c.requested_at = now;
    c.expected = c.state == State::ON ? State::OFF : State::ON;
    return true;
  }
  void fail(size_t index) {
    if (index >= CONTROL_COUNT) return;
    controls_[index].pending = false;
    controls_[index].failed = true;
  }
  bool tick(uint32_t now) {
    bool changed = false;
    for (auto &c : controls_) {
      if (c.pending && static_cast<uint32_t>(now - c.requested_at) >= 8000) {
        c.pending = false;
        c.failed = true;
        changed = true;
      }
    }
    return changed;
  }
 private:
  bool connected_{false};
  std::array<Control, CONTROL_COUNT> controls_{};
};

struct RoomSummary {
  size_t on{0}, unknown{0}, unavailable{0}, pending{0}, failed{0};
  bool ready() const { return unknown == 0 && unavailable == 0 && pending == 0; }
};
inline RoomSummary summarize(const ControllerState &state, size_t room_index) {
  RoomSummary result;
  const auto &room = ROOMS.at(room_index);
  for (size_t n = 0; n < room.count; ++n) {
    const auto &c = state.at(room.members[n]);
    if (c.state == State::ON) ++result.on;
    if (c.state == State::UNKNOWN) ++result.unknown;
    if (c.state == State::UNAVAILABLE) ++result.unavailable;
    result.pending += c.pending;
    result.failed += c.failed;
  }
  return result;
}

// Scene states are timestamps or "unknown" until first use, not light states.
inline bool scene_available(const std::string &value) {
  return !value.empty() && value != "unavailable";
}
class RequestState {
 public:
  bool pending{false}, failed{false};
  uint32_t begin(uint32_t now) {
    if (pending) return 0;
    if (++sequence_ == 0) ++sequence_;
    pending = true; failed = false; started_ = now;
    return sequence_;
  }
  bool finish(uint32_t sequence, bool success) {
    if (!pending || sequence != sequence_) return false;
    pending = false; failed = !success; return true;
  }
  void cancel() { pending = false; failed = false; ++sequence_; }
  bool tick(uint32_t now) {
    if (pending && static_cast<uint32_t>(now - started_) >= 8000) {
      pending = false; failed = true; return true;
    }
    return false;
  }
 private:
  uint32_t sequence_{0}, started_{0};
};
}  // namespace lcars
