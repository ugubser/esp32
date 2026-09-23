#include "../firmware/weather_model.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>

namespace {
// Shape of a live Open-Meteo response (2026-09-23), trimmed of metadata.
std::string response(const std::string &current,const std::string &daily_override="") {
  std::string daily=R"("time":["2026-09-22","2026-09-23","2026-09-24","2026-09-25","2026-09-26","2026-09-27","2026-09-28"],
    "weather_code":[80,3,3,1,1,1,45],
    "temperature_2m_max":[20.7,22.3,22.0,26.0,27.0,27.7,27.6],
    "temperature_2m_min":[9.5,12.5,11.0,11.4,12.5,13.9,11.1],
    "precipitation_sum":[2.4,0.00,0.00,0.00,0.00,0.00,0.00],
    "precipitation_probability_max":[60,3,0,0,0,3,3],
    "wind_speed_10m_max":[9.2,14.3,7.4,6.1,6.1,5.6,4.6],
    "sunrise":["2026-09-22T07:12","2026-09-23T07:13","2026-09-24T07:15","2026-09-25T07:16","2026-09-26T07:17","2026-09-27T07:19","2026-09-28T07:20"],
    "sunset":["2026-09-22T19:23","2026-09-23T19:21","2026-09-24T19:19","2026-09-25T19:17","2026-09-26T19:15","2026-09-27T19:13","2026-09-28T19:11"])";
  if(!daily_override.empty())daily=daily_override;
  return R"({"current":)"+current+R"(,"daily":{)"+daily+"}}";
}
const std::string CURRENT=R"({"temperature_2m":17.2,"weather_code":2,"is_day":1})";

std::string replace(std::string text,const std::string &from,const std::string &to) {
  const auto at=text.find(from);assert(at!=std::string::npos);
  return text.replace(at,from.size(),to);
}
}

int main() {
  using namespace lcars;
  const auto valid=response(CURRENT);
  WeatherReading reading;
  assert(weather_parse(valid,"2026-09-22",1000,reading));
  assert(std::fabs(reading.temperature_c-17.2f)<0.01f);
  assert(reading.current_code==2&&reading.is_day);
  assert(reading.sunrise=="07:12"&&reading.sunset=="19:23");
  const auto &today=reading.days[0];
  assert(today.date=="2026-09-22"&&today.code==80);
  assert(std::fabs(today.high_c-20.7f)<0.01f&&std::fabs(today.low_c-9.5f)<0.01f);
  assert(std::fabs(today.precipitation_mm-2.4f)<0.01f&&today.precipitation_probability==60);
  assert(std::fabs(today.wind_kmh-9.2f)<0.01f);
  const auto &last=reading.days[FORECAST_DAYS-1];
  assert(last.date=="2026-09-28"&&last.code==45&&std::fabs(last.high_c-27.6f)<0.01f);
  assert(weather_icon(reading.current_code,reading.is_day)==WeatherIcon::CLOUD_SUN);
  assert(weather_icon(today.code,true)==WeatherIcon::SUN_RAIN);
  assert(weather_fresh(reading,1000,"2026-09-22"));
  assert(!weather_fresh(reading,6401,"2026-09-22"));
  assert(!weather_fresh(reading,1100,"2026-09-23"));
  auto original=reading;
  assert(!weather_parse(valid,"2026-09-23",1100,reading));
  assert(reading.fetched==original.fetched&&reading.days[0].date==original.days[0].date);

  assert(std::string(weather_weekday("2026-09-22"))=="DI");
  assert(std::string(weather_weekday("2026-09-27"))=="SO");
  assert(std::string(weather_weekday("2024-02-29"))=="DO");
  assert(std::string(weather_weekday("2026-01-01"))=="DO");
  assert(std::string(weather_weekday("2026-13-01"))=="--");
  assert(std::string(weather_weekday("bad"))=="--");

  for(const auto &[code,icon]:{
    std::pair{0,WeatherIcon::SUN}, {1,WeatherIcon::CLOUD_SUN},
    {2,WeatherIcon::CLOUD_SUN}, {3,WeatherIcon::CLOUD},
    {45,WeatherIcon::FOG}, {48,WeatherIcon::FOG},
    {51,WeatherIcon::DRIZZLE}, {53,WeatherIcon::DRIZZLE},
    {55,WeatherIcon::DRIZZLE}, {56,WeatherIcon::DRIZZLE},
    {57,WeatherIcon::DRIZZLE}, {61,WeatherIcon::RAIN},
    {63,WeatherIcon::RAIN}, {65,WeatherIcon::RAIN},
    {66,WeatherIcon::RAIN}, {67,WeatherIcon::RAIN},
    {71,WeatherIcon::SNOW}, {73,WeatherIcon::SNOW},
    {75,WeatherIcon::SNOW}, {77,WeatherIcon::SNOW},
    {80,WeatherIcon::SUN_RAIN}, {81,WeatherIcon::SUN_RAIN},
    {82,WeatherIcon::SUN_RAIN}, {85,WeatherIcon::SNOW},
    {86,WeatherIcon::SNOW}, {95,WeatherIcon::STORM},
    {96,WeatherIcon::STORM}, {99,WeatherIcon::STORM}
  })assert(weather_icon(code,true)==icon);
  assert(weather_icon(0,false)==WeatherIcon::MOON);
  assert(weather_icon(1,false)==WeatherIcon::CLOUD_MOON);
  assert(weather_icon(2,false)==WeatherIcon::CLOUD_MOON);
  assert(weather_icon(80,false)==WeatherIcon::MOON_RAIN);
  assert(weather_icon(4,true)==WeatherIcon::UNKNOWN);

  const std::string bad[]={
    "{}",
    "[]",
    "{\"current\":{},\"daily\":{}}",
    response(R"({"temperature_2m":17,"weather_code":4,"is_day":1})"),
    response(R"({"temperature_2m":17,"weather_code":2.5,"is_day":1})"),
    response(R"({"temperature_2m":17,"weather_code":2,"is_day":2})"),
    replace(valid,"[80,3,3,1,1,1,45]","[80,3,3,1,1,1]"),           // a missing day
    replace(valid,"[80,3,3,1,1,1,45]","[80,3,3,1,1,1,4]"),          // unknown condition
    replace(valid,"[60,3,0,0,0,3,3]","[60,3,null,0,0,3,3]"),        // null probability
    replace(valid,"[60,3,0,0,0,3,3]","[160,3,0,0,0,3,3]"),          // probability above 100
    replace(valid,"[9.5,12.5","[29.5,12.5"),                        // low above high
    replace(valid,"[2.4,0.00","[-1,0.00"),                          // negative rain
    replace(valid,"\"2026-09-22T07:12\"","\"2026-09-21T07:12\""),   // sunrise on another day
    replace(valid,"\"2026-09-22T19:23\"","\"2026-09-22T19-23\""),   // malformed sunset
    valid+" trailing"
  };
  for(const auto &body:bad)assert(!weather_parse(body,"2026-09-22",1100,reading));
  assert(reading.fetched==original.fetched);
  std::cout<<"Weather tests passed: condition codes, day/night icons, 7-day forecast parsing, weekdays, sunrise/sunset, local day, stale data and invalid responses\n";
}
