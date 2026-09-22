#include "../firmware/weather_model.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>

int main() {
  using namespace lcars;
  const std::string valid=R"({
    "current":{"temperature_2m":17.2,"weather_code":2,"is_day":1},
    "daily":{"time":["2026-09-22"],"temperature_2m_max":[20.7],
             "weather_code":[80]}})";
  WeatherReading reading;
  assert(weather_parse(valid,"2026-09-22",1000,reading));
  assert(std::fabs(reading.temperature_c-17.2f)<0.01f);
  assert(std::fabs(reading.high_c-20.7f)<0.01f);
  assert(reading.current_code==2&&reading.today_code==80&&reading.is_day);
  assert(weather_icon(reading.current_code,reading.is_day)==WeatherIcon::CLOUD_SUN);
  assert(weather_icon(reading.today_code,true)==WeatherIcon::SUN_RAIN);
  assert(weather_fresh(reading,1000,"2026-09-22"));
  assert(!weather_fresh(reading,6401,"2026-09-22"));
  assert(!weather_fresh(reading,1100,"2026-09-23"));
  auto original=reading;
  assert(!weather_parse(valid,"2026-09-23",1100,reading));
  assert(reading.fetched==original.fetched);

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

  for(const auto &bad:{
    "{}",
    "[]",
    "{\"current\":{},\"daily\":{}}",
    "{\"current\":{\"temperature_2m\":17,\"weather_code\":4,\"is_day\":1},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"weather_code\":[0]}}",
    "{\"current\":{\"temperature_2m\":17,\"weather_code\":2.5,\"is_day\":1},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"weather_code\":[0]}}",
    "{\"current\":{\"temperature_2m\":17,\"weather_code\":2,\"is_day\":2},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"weather_code\":[0]}}",
    "{\"current\":{\"temperature_2m\":17,\"weather_code\":2,\"is_day\":1},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"weather_code\":[]}}",
    "{} trailing"
  })assert(!weather_parse(bad,"2026-09-22",1100,reading));
  std::cout<<"Weather tests passed: condition codes, day/night icons, parsing, local day, stale data and invalid responses\n";
}
