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
  int current_code{-1};
  int today_code{-1};
  bool is_day{};
  int64_t fetched{};
  std::string day;
};

enum class WeatherIcon : uint8_t {
  UNKNOWN, SUN, MOON, CLOUD_SUN, CLOUD_MOON, CLOUD, FOG, DRIZZLE,
  RAIN, SUN_RAIN, MOON_RAIN, SNOW, STORM
};

inline WeatherIcon weather_icon(int code,bool is_day) {
  switch(code) {
    case 0: return is_day?WeatherIcon::SUN:WeatherIcon::MOON;
    case 1: case 2: return is_day?WeatherIcon::CLOUD_SUN:WeatherIcon::CLOUD_MOON;
    case 3: return WeatherIcon::CLOUD;
    case 45: case 48: return WeatherIcon::FOG;
    case 51: case 53: case 55: case 56: case 57: return WeatherIcon::DRIZZLE;
    case 61: case 63: case 65: case 66: case 67: return WeatherIcon::RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return WeatherIcon::SNOW;
    case 80: case 81: case 82: return is_day?WeatherIcon::SUN_RAIN:WeatherIcon::MOON_RAIN;
    case 95: case 96: case 99: return WeatherIcon::STORM;
    default: return WeatherIcon::UNKNOWN;
  }
}

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
  const auto *current_code=cJSON_GetObjectItemCaseSensitive(current,"weather_code");
  const auto *is_day=cJSON_GetObjectItemCaseSensitive(current,"is_day");
  const auto *days=cJSON_GetObjectItemCaseSensitive(daily,"time");
  const auto *highs=cJSON_GetObjectItemCaseSensitive(daily,"temperature_2m_max");
  const auto *today_codes=cJSON_GetObjectItemCaseSensitive(daily,"weather_code");
  const auto *day=cJSON_GetArrayItem(days,0);
  const auto *high=cJSON_GetArrayItem(highs,0);
  const auto *today_code=cJSON_GetArrayItem(today_codes,0);
  const bool valid=cJSON_IsObject(current) && cJSON_IsObject(daily) &&
    cJSON_IsString(day) && day->valuestring && today==day->valuestring &&
    weather_number(temperature,-80,60) && weather_number(high,-80,60) &&
    weather_number(current_code,0,99) && std::floor(current_code->valuedouble)==current_code->valuedouble &&
    weather_number(today_code,0,99) && std::floor(today_code->valuedouble)==today_code->valuedouble &&
    weather_number(is_day,0,1) && std::floor(is_day->valuedouble)==is_day->valuedouble &&
    weather_icon(int(current_code->valuedouble),is_day->valuedouble==1)!=WeatherIcon::UNKNOWN &&
    weather_icon(int(today_code->valuedouble),true)!=WeatherIcon::UNKNOWN;
  if(valid)out={float(temperature->valuedouble),float(high->valuedouble),
                int(current_code->valuedouble),int(today_code->valuedouble),
                is_day->valuedouble==1,now,today};
  cJSON_Delete(root);
  return valid;
}

inline bool weather_fresh(const WeatherReading &reading,int64_t now,const std::string &today) {
  return reading.fetched>0 && now>=reading.fetched && now-reading.fetched<=5400 && reading.day==today;
}
}
