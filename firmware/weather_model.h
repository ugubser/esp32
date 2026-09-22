#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace lcars {
struct WeatherReading {
  float temperature_c{};
  float high_c{};
  float rain_now_mm{};
  float rain_today_mm{};
  int64_t fetched{};
  std::string day;
};

inline bool weather_number(const cJSON *item,double low,double high) {
  return cJSON_IsNumber(item) && std::isfinite(item->valuedouble) &&
         item->valuedouble>=low && item->valuedouble<=high;
}

inline bool weather_parse(const std::string &body,const std::string &today,int64_t now,WeatherReading &out) {
  const char *end=nullptr;
  cJSON *root=cJSON_ParseWithLengthOpts(body.c_str(),body.size()+1,&end,true);
  if(!cJSON_IsObject(root)){cJSON_Delete(root);return false;}
  const auto *current=cJSON_GetObjectItemCaseSensitive(root,"current");
  const auto *daily=cJSON_GetObjectItemCaseSensitive(root,"daily");
  const auto *temperature=cJSON_GetObjectItemCaseSensitive(current,"temperature_2m");
  const auto *rain=cJSON_GetObjectItemCaseSensitive(current,"rain");
  const auto *showers=cJSON_GetObjectItemCaseSensitive(current,"showers");
  const auto *days=cJSON_GetObjectItemCaseSensitive(daily,"time");
  const auto *highs=cJSON_GetObjectItemCaseSensitive(daily,"temperature_2m_max");
  const auto *rains=cJSON_GetObjectItemCaseSensitive(daily,"rain_sum");
  const auto *daily_showers=cJSON_GetObjectItemCaseSensitive(daily,"showers_sum");
  const auto *day=cJSON_GetArrayItem(days,0);
  const auto *high=cJSON_GetArrayItem(highs,0);
  const auto *rain_today=cJSON_GetArrayItem(rains,0);
  const auto *showers_today=cJSON_GetArrayItem(daily_showers,0);
  const bool valid=cJSON_IsObject(current) && cJSON_IsObject(daily) &&
    cJSON_IsString(day) && day->valuestring && today==day->valuestring &&
    weather_number(temperature,-80,60) && weather_number(high,-80,60) &&
    weather_number(rain,0,500) && weather_number(showers,0,500) &&
    weather_number(rain_today,0,500) && weather_number(showers_today,0,500);
  if(valid)out={float(temperature->valuedouble),float(high->valuedouble),
                float(rain->valuedouble+showers->valuedouble),
                float(rain_today->valuedouble+showers_today->valuedouble),now,today};
  cJSON_Delete(root);
  return valid;
}

inline bool weather_fresh(const WeatherReading &reading,int64_t now,const std::string &today) {
  return reading.fetched>0 && now>=reading.fetched && now-reading.fetched<=5400 && reading.day==today;
}
inline bool weather_raining_now(const WeatherReading &reading) {return reading.rain_now_mm>0.0f;}
inline bool weather_rain_today(const WeatherReading &reading) {return reading.rain_today_mm>0.0f;}
}
