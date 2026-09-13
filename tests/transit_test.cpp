#include "../firmware/transit_json.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace lcars;
  const auto now=transit_timestamp("2026-09-13T17:00:00Z");
  assert(now==1789318800);
  assert(transit_timestamp("2026-09-13T19:00:00+02:00")==now);
  assert(transit_timestamp("2026-09-13T17:00:00.123Z")==now);
  for(const auto &bad:{"bad","2026-02-30T00:00:00Z","2026-01-01T25:00:00Z","2026-09-13T17:00:00","2026-09-13T17:00:00Zjunk"})assert(!transit_timestamp(bad));
  assert(transit_matches(0,"7","Bahnhof Stettbach"));
  assert(!transit_matches(0,"7","Wollishoferplatz"));
  assert(transit_matches(1,"S24","Thayngen"));
  assert(!transit_matches(1,"S24","Zug"));
  assert(!transit_matches(1,"24","Weinfelden"));
  assert(transit_matches(2,"S8","Winterthur"));
  assert(transit_matches(3,"72","Milchbuck"));
  assert(!transit_matches(3,"72","Morgental"));
  TransitBoard board;
  const std::string json=R"([
    {"number":"S24","to":"Weinfelden","category":"B","departure":"2026-09-13T17:03:00Z","delay":3},
    {"number":"S24","to":"Zug","category":"B","departure":"2026-09-13T17:02:00Z","delay":0},
    {"number":"S8","to":"Winterthur","category":"B","departure":"2026-09-13T17:15:00Z","delay":0}
  ])";
  assert(transit_parse(json,1,now,board));
  assert(board[1].departures.size()==1 && board[2].departures.size()==1);
  assert(board[1].departures[0].at==now+180); // Delay is already included in the API time.
  for(int i=15;i>=1;--i)transit_add(board[1],{"Thayngen",now+i*60,0},now);
  assert(board[1].departures.size()==12);
  auto next=transit_next(board[1],now);assert(next.size()==3 && next[0].at==now+60);
  transit_add(board[1],next[0],now);assert(board[1].departures.size()==12);
  assert(transit_next(board[1],now+181).empty()); // Stale responses never presented as live.
  board[1].fetched=now+60;
  next=transit_next(board[1],now+60);assert(next[0].at==now+120); // Departed services disappear.
  board[1].error="HTTP 401";assert(transit_next(board[1],now+60).empty());
  assert(!transit_parse("{}",1,now,board));
  assert(!transit_parse("[{\"number\":\"S8\"}]",1,now,board));
  assert(!transit_parse("[] trailing",1,now,board));
  assert(transit_parse("[]",1,now,board)&&board[1].departures.empty()&&board[1].error.empty());
  std::cout<<"Transit tests passed: routes, UTC/offsets, invalid dates, JSON validation, delay handling, sorting, deduplication, expiry and stale/error states\n";
}
