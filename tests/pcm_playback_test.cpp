#include "../firmware/pcm_playback.h"
#include <cassert>
#include <vector>

struct Speaker {
  bool failed{}, stopped{}, finished{}, aborted{};
  size_t capacity{};
  std::vector<uint8_t> received;
  bool is_failed() const { return failed; }
  bool is_stopped() const { return stopped; }
  size_t play(const uint8_t *data, size_t size, int wait) {
    assert(wait == 0);
    size = std::min(size, capacity); received.insert(received.end(),data,data+size); return size;
  }
  void finish() { finished = true; }
  void stop() { aborted = true; }
};
int main() {
  const uint8_t clip[]{1,2,3,4,5,6};
  lcars::PcmPlayback p; Speaker s;
  using R = lcars::PcmPlayback::Result;
  assert(!p.begin(nullptr,6,0) && !p.begin(clip,5,0));
  assert(p.begin(clip,6,0)); assert(!p.begin(clip,6,0));
  assert(p.pump(s,10)==R::PLAYING && s.received.empty());
  s.capacity=2;
  p.pump(s,20);p.pump(s,30);p.pump(s,40);
  assert(s.received==std::vector<uint8_t>(clip,clip+6) && s.finished);
  assert(p.pump(s,50)==R::PLAYING); s.stopped=true;
  assert(p.pump(s,60)==R::FINISHED && !p.active());
  assert(p.pump(s,70)==R::IDLE);
  assert(p.begin(clip,6,UINT32_MAX-100));
  assert(p.pump(s,4899)==R::FAILED && s.aborted); // timeout across clock wrap
  s=Speaker{};assert(p.begin(clip,6,0));s.failed=true;
  assert(p.pump(s,1)==R::FAILED && s.aborted);
}
