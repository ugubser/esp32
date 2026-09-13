#include "../firmware/controller_state.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
  lcars::ControllerState state;
  assert(!state.begin(0, 0));
  state.update(0, "on");
  assert(state.at(0).state == lcars::State::UNKNOWN);
  state.set_connected(true);
  assert(!state.can_control(0));
  for (const auto *invalid : {"unknown", "unavailable", "", "true", "ON"}) {
    state.update(0, invalid);
    assert(!state.begin(0, 1));
  }
  state.update(0, "off");
  assert(state.begin(0, 100));
  assert(!state.begin(0, 101));
  assert(state.at(0).state == lcars::State::OFF); // Never assume success.
  state.tick(8099);
  assert(state.at(0).pending);
  state.tick(8100);
  assert(!state.at(0).pending && state.at(0).failed);
  state.update(0, "on");
  assert(!state.at(0).failed);
  assert(state.begin(0, 8200));
  state.fail(0);
  assert(state.at(0).failed && state.at(0).state == lcars::State::ON);
  state.set_connected(false);
  state.set_connected(true);
  assert(!state.can_control(0)); // Reconnection must receive fresh state.
  state.update(1, "on");
  assert(state.can_control(1) && !state.can_control(0));
  const uint32_t near_wrap = std::numeric_limits<uint32_t>::max() - 1000;
  assert(state.begin(1, near_wrap));
  state.tick(near_wrap + 7999U);
  assert(state.at(1).pending);
  state.tick(near_wrap + 8000U);
  assert(state.at(1).failed);
  assert(!state.begin(lcars::CONTROL_COUNT, 0));
  state.update(lcars::CONTROL_COUNT, "on");
  state.fail(lcars::CONTROL_COUNT);
  // A duplicate old state must not confirm a new command.
  state.update(0, "off");
  assert(state.begin(0, 10));
  state.update(0, "off");
  assert(state.at(0).pending);
  state.update(0, "on");
  assert(!state.at(0).pending);
  // Kitchen contains two distinct working switch circuits; dining contains
  // three lights. Partial availability must not allow an all-room action.
  state.update(1, "on"); state.update(2, "off");
  auto kitchen=lcars::summarize(state,1);
  assert(kitchen.ready() && kitchen.on==1 && lcars::ROOMS[1].count==2);
  state.update(2,"unavailable");
  assert(!lcars::summarize(state,1).ready());
  state.update(4,"off"); state.update(5,"on"); state.update(6,"on");
  auto dining=lcars::summarize(state,3);
  assert(dining.ready() && dining.on==2 && lcars::ROOMS[3].count==3);
  std::array<int,lcars::CONTROL_COUNT> membership{};
  for(const auto &room:lcars::ROOMS)
    for(size_t i=0;i<room.count;++i) ++membership.at(room.members[i]);
  for(auto count:membership) assert(count==1);
  assert(lcars::scene_available("unknown"));
  assert(lcars::scene_available("2026-09-12T20:00:00+00:00"));
  assert(!lcars::scene_available("unavailable") && !lcars::scene_available(""));
  assert(lcars::SCENES.size()==18 && lcars::SCENE_PAGES==3);
  lcars::RequestState request;
  auto first=request.begin(100); assert(first && !request.begin(101));
  assert(!request.finish(first+1,true) && request.pending);
  assert(request.tick(8100) && request.failed);
  auto second=request.begin(8200);
  assert(!request.finish(first,true) && request.pending);
  assert(request.finish(second,true) && !request.failed);
  auto third=request.begin(near_wrap);
  assert(!request.tick(near_wrap+7999U));
  assert(request.tick(near_wrap+8000U));
  assert(!request.finish(third,true));
  auto fourth=request.begin(1); request.cancel();
  assert(!request.finish(fourth,true));
  std::cout << "Controller state tests passed\n";
}
