#include "../firmware/weather_model.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  using namespace lcars;
  const std::string valid=R"({
    "current":{"temperature_2m":17.2,"rain":0.1,"showers":0.2},
    "daily":{"time":["2026-09-22"],"temperature_2m_max":[20.7],
             "rain_sum":[0.0],"showers_sum":[0.4]}})";
  WeatherReading reading;
  assert(weather_parse(valid,"2026-09-22",1000,reading));
  assert(std::fabs(reading.temperature_c-17.2f)<0.01f);
  assert(std::fabs(reading.high_c-20.7f)<0.01f);
  assert(std::fabs(reading.rain_now_mm-0.3f)<0.01f);
  assert(std::fabs(reading.rain_today_mm-0.4f)<0.01f);
  assert(weather_raining_now(reading)&&weather_rain_today(reading));
  assert(weather_fresh(reading,1000,"2026-09-22"));
  assert(!weather_fresh(reading,6401,"2026-09-22"));
  assert(!weather_fresh(reading,1100,"2026-09-23"));
  auto original=reading;
  assert(!weather_parse(valid,"2026-09-23",1100,reading));
  assert(reading.fetched==original.fetched); // A wrong-day forecast cannot replace good data.
  for(const auto &bad:{
    "{}",
    "[]",
    "{\"current\":{},\"daily\":{}}",
    "{\"current\":{\"temperature_2m\":null,\"rain\":0,\"showers\":0},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"rain_sum\":[0],\"showers_sum\":[0]}}",
    "{\"current\":{\"temperature_2m\":17,\"rain\":-1,\"showers\":0},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"rain_sum\":[0],\"showers_sum\":[0]}}",
    "{\"current\":{\"temperature_2m\":17,\"rain\":0,\"showers\":0},\"daily\":{\"time\":[\"2026-09-22\"],\"temperature_2m_max\":[20],\"rain_sum\":[0],\"showers_sum\":[]}}",
    "{} trailing"})assert(!weather_parse(bad,"2026-09-22",1100,reading));
  const std::string dry=R"({"current":{"temperature_2m":-2,"rain":0,"showers":0},"daily":{"time":["2026-09-22"],"temperature_2m_max":[1],"rain_sum":[0],"showers_sum":[0]}})";
  assert(weather_parse(dry,"2026-09-22",2000,reading));
  assert(!weather_raining_now(reading)&&!weather_rain_today(reading));
  std::cout<<"Weather tests passed: current/daily parsing, rain, local day, stale data and invalid responses\n";
}
