#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace lcars {
// Feed a short flash-resident PCM clip without blocking touch or networking.
class PcmPlayback {
 public:
  enum class Result { IDLE, PLAYING, FINISHED, FAILED };
  bool begin(const uint8_t *data, size_t size, uint32_t now) {
    if (active_ || !data || !size || size % 2) return false;
    data_ = data; size_ = size; offset_ = 0; started_ = now;
    draining_ = false; active_ = true; return true;
  }
  bool active() const { return active_; }
  template<class Speaker> Result pump(Speaker &speaker, uint32_t now) {
    if (!active_) return Result::IDLE;
    if (speaker.is_failed() || uint32_t(now - started_) >= 5000) {
      speaker.stop(); active_ = false; return Result::FAILED;
    }
    if (draining_) {
      if (speaker.is_stopped()) { active_ = false; return Result::FINISHED; }
    } else {
      const size_t wanted = std::min(size_t(1024), size_ - offset_);
      const size_t written = speaker.play(data_ + offset_, wanted, 0);
      if (written > wanted || written % 2) {
        speaker.stop(); active_ = false; return Result::FAILED;
      }
      offset_ += written;
      if (offset_ == size_) { speaker.finish(); draining_ = true; }
    }
    return Result::PLAYING;
  }
 private:
  const uint8_t *data_{};
  size_t size_{}, offset_{};
  uint32_t started_{};
  bool active_{}, draining_{};
};
inline PcmPlayback sound_playback;
}
