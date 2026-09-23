#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <cstring>
#include <array>
#include <string>

namespace lcars {
inline constexpr size_t FORECAST_DAYS=7;
struct WeatherDay {
  std::string date;
  int code{-1};
  float high_c{};
  float low_c{};
  float precipitation_mm{};
  int precipitation_probability{};
  float wind_kmh{};
};
struct WeatherReading {
  float temperature_c{};
  int current_code{-1};
  bool is_day{};
  int64_t fetched{};
  std::string day;
  std::string sunrise, sunset;
  std::array<WeatherDay,FORECAST_DAYS> days{};
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

inline bool weather_integer(const cJSON *item,double low,double high) {
  return weather_number(item,low,high) && std::floor(item->valuedouble)==item->valuedouble;
}

// Open-Meteo local times look like 2026-09-23T07:13; keep HH:MM of that date.
inline bool weather_clock(const cJSON *item,const std::string &date,std::string &out) {
  if(!cJSON_IsString(item) || !item->valuestring)return false;
  const std::string value=item->valuestring;
  if(value.size()!=16 || value.compare(0,10,date)!=0 || value[10]!='T' || value[13]!=':')return false;
  for(size_t i:{11,12,14,15})if(value[i]<'0' || value[i]>'9')return false;
  out=value.substr(11);
  return true;
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
  const auto field=[daily](const char *name){return cJSON_GetObjectItemCaseSensitive(daily,name);};
  const cJSON *dates=field("time"),*codes=field("weather_code"),*highs=field("temperature_2m_max"),
    *lows=field("temperature_2m_min"),*rain=field("precipitation_sum"),
    *chance=field("precipitation_probability_max"),*wind=field("wind_speed_10m_max"),
    *sunrises=field("sunrise"),*sunsets=field("sunset");
  WeatherReading result;
  bool valid=cJSON_IsObject(current) && cJSON_IsObject(daily) &&
    weather_number(temperature,-80,60) &&
    weather_integer(current_code,0,99) && weather_integer(is_day,0,1) &&
    weather_icon(int(current_code->valuedouble),is_day->valuedouble==1)!=WeatherIcon::UNKNOWN;
  for(const auto *list:{dates,codes,highs,lows,rain,chance,wind,sunrises,sunsets})
    valid=valid && cJSON_IsArray(list) && cJSON_GetArraySize(list)==int(FORECAST_DAYS);
  for(size_t i=0;valid && i<FORECAST_DAYS;++i) {
    const auto *date=cJSON_GetArrayItem(dates,i),*code=cJSON_GetArrayItem(codes,i),
      *high=cJSON_GetArrayItem(highs,i),*low=cJSON_GetArrayItem(lows,i),
      *sum=cJSON_GetArrayItem(rain,i),*probability=cJSON_GetArrayItem(chance,i),
      *speed=cJSON_GetArrayItem(wind,i);
    valid=cJSON_IsString(date) && date->valuestring && std::strlen(date->valuestring)==10 &&
      (i>0 || today==date->valuestring) &&
      weather_integer(code,0,99) && weather_icon(int(code->valuedouble),true)!=WeatherIcon::UNKNOWN &&
      weather_number(high,-80,60) && weather_number(low,-80,60) && low->valuedouble<=high->valuedouble &&
      weather_number(sum,0,2000) && weather_integer(probability,0,100) && weather_number(speed,0,500);
    if(valid)result.days[i]={date->valuestring,int(code->valuedouble),float(high->valuedouble),
      float(low->valuedouble),float(sum->valuedouble),int(probability->valuedouble),float(speed->valuedouble)};
  }
  valid=valid && weather_clock(cJSON_GetArrayItem(sunrises,0),today,result.sunrise) &&
    weather_clock(cJSON_GetArrayItem(sunsets,0),today,result.sunset);
  if(valid) {
    result.temperature_c=float(temperature->valuedouble);
    result.current_code=int(current_code->valuedouble);
    result.is_day=is_day->valuedouble==1;
    result.fetched=now;result.day=today;
    out=std::move(result);
  }
  cJSON_Delete(root);
  return valid;
}

// German two-letter weekday for a YYYY-MM-DD date, or "--" when malformed.
inline const char *weather_weekday(const std::string &date) {
  static constexpr const char *names[]={"SO","MO","DI","MI","DO","FR","SA"};
  int year=0,month=0,day=0;
  if(date.size()!=10 || std::sscanf(date.c_str(),"%4d-%2d-%2d",&year,&month,&day)!=3 ||
     month<1 || month>12 || day<1 || day>31)return "--";
  // Sakamoto's method; independent of the process time zone.
  static constexpr int offsets[]={0,3,2,5,0,3,5,1,4,6,2,4};
  if(month<3)--year;
  return names[(year+year/4-year/100+year/400+offsets[month-1]+day)%7];
}

inline bool weather_fresh(const WeatherReading &reading,int64_t now,const std::string &today) {
  return reading.fetched>0 && now>=reading.fetched && now-reading.fetched<=5400 && reading.day==today;
}
}
