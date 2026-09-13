#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace lcars {
struct TransitRoute { const char *station; const char *line; unsigned stop; };
inline constexpr std::array<TransitRoute,4> TRANSIT_ROUTES{{
  {"WOLLISHOFEN BHF / STAUBSTRASSE", "7", 0},
  {"WOLLISHOFEN / S24", "S24", 1},
  {"WOLLISHOFEN / S8", "S8", 1},
  {"JUGENDHERBERGE", "72", 2}
}};
inline constexpr std::array<const char*,3> TRANSIT_STOPS{{"8591081","8503009","8591216"}};
struct Departure { std::string destination; int64_t at{}; int delay{}; };
struct TransitGroup { std::vector<Departure> departures; std::string error{"Laden..."}; int64_t fetched{}; };
using TransitBoard = std::array<TransitGroup,4>;
inline bool transit_matches(size_t route, const std::string &line, const std::string &destination) {
  if (route >= TRANSIT_ROUTES.size() || line != TRANSIT_ROUTES[route].line) return false;
  switch(route) {
    case 0:return destination=="Bahnhof Stettbach" || destination=="Stettbach, Bahnhof";
    case 1:return destination=="Weinfelden" || destination=="Thayngen";
    case 2:return destination=="Winterthur";
    case 3:return destination=="Milchbuck" || destination=="Zürich, Milchbuck";
  }
  return false;
}
inline int64_t transit_timestamp(const std::string &s) {
  int y,m,d,h,min,sec,n=0;
  if (std::sscanf(s.c_str(),"%d-%d-%dT%d:%d:%d%n",&y,&m,&d,&h,&min,&sec,&n)!=6 ||
      y<2020 || y>2099 || m<1 || m>12 || h<0 || h>23 || min<0 || min>59 || sec<0 || sec>59) return 0;
  static constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap=y%4==0 && (y%100!=0 || y%400==0);
  if(d<1 || d>days[m-1]+(m==2&&leap))return 0;
  if(s.size()>size_t(n) && s[n]=='.') {
    ++n;const int start=n;while(size_t(n)<s.size() && s[n]>='0' && s[n]<='9')++n;
    if(n==start)return 0;
  }
  int offset=0;
  if(s.substr(n)=="Z") {} else {
    int oh,om,used=0;
    if(size_t(n)>=s.size() || (s[n]!='+' && s[n]!='-') ||
       std::sscanf(s.c_str()+n+1,"%d:%d%n",&oh,&om,&used)!=2 ||
       size_t(n+1+used)!=s.size() || oh<0 || oh>14 || om<0 || om>59) return 0;
    offset=(oh*3600+om*60)*(s[n]=='+'?1:-1);
  }
  y-=m<=2;const int era=y/400;const unsigned yo=y-era*400;
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  const unsigned doe=yo*365+yo/4-yo/100+doy;
  return (int64_t(era)*146097+doe-719468)*86400+h*3600+min*60+sec-offset;
}
inline void transit_add(TransitGroup &g, Departure d, int64_t now) {
  if(d.at<=now || d.delay<0 || d.delay>1440)return;
  for(const auto &old:g.departures)if(old.at==d.at && old.destination==d.destination)return;
  g.departures.push_back(std::move(d));
  std::sort(g.departures.begin(),g.departures.end(),[](const Departure &a,const Departure &b){return a.at<b.at;});
  if(g.departures.size()>12)g.departures.resize(12);
}
inline std::vector<Departure> transit_next(const TransitGroup &g,int64_t now) {
  std::vector<Departure> result;
  if(!g.error.empty() || !g.fetched || now-g.fetched>180)return result;
  for(const auto &d:g.departures)if(d.at>now) {result.push_back(d);if(result.size()==3)break;}
  return result;
}
}
