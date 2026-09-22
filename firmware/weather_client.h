#pragma once
#include "weather_model.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace lcars {
class WeatherClient {
 public:
  void tick(bool wifi,bool location_ready,double latitude,double longitude,bool transit_busy) {
    if(done_.load(std::memory_order_acquire)) {
      reading=result_;
      status=result_status_;
      interval_ms_=reading.fetched ? 1800000u : 300000u;
      done_.store(false,std::memory_order_relaxed);
      busy_=false;
      ESP_LOGI("weather","%s",status.c_str());
    }
    if(!wifi || std::time(nullptr)<1700000000 || !location_ready || transit_busy || busy_ ||
       !std::isfinite(latitude) || !std::isfinite(longitude) ||
       latitude<-90 || latitude>90 || longitude<-180 || longitude>180)return;
    const auto now=uint32_t(xTaskGetTickCount()*portTICK_PERIOD_MS);
    if(!started_ && now<45000u)return;
    const auto stamp=std::time(nullptr);
    std::tm local{};localtime_r(&stamp,&local);
    char day[11];std::strftime(day,sizeof(day),"%Y-%m-%d",&local);
    if(started_ && (reading.day.empty() || reading.day==day) && uint32_t(now-last_)<interval_ms_)return;
    latitude_=latitude;longitude_=longitude;started_=true;last_=now;busy_=true;
    if(xTaskCreate([](void *arg){
      auto *self=static_cast<WeatherClient*>(arg);self->fetch();
      self->done_.store(true,std::memory_order_release);vTaskDelete(nullptr);
    },"weather_fetch",8192,this,1,nullptr)!=pdPASS){busy_=false;status="Weather task failed";}
  }
  WeatherReading reading{};
  std::string status{"Waiting for Wi-Fi / Home location"};
 private:
  bool busy_{},started_{};uint32_t last_{},interval_ms_{1800000u};
  double latitude_{},longitude_{};
  std::atomic<bool> done_{false};
  WeatherReading result_{};
  std::string result_status_;
  struct Response {std::string body;bool too_large{};};
  static esp_err_t event(esp_http_client_event_t *e) {
    if(e->event_id==HTTP_EVENT_ON_DATA) {
      auto *response=static_cast<Response*>(e->user_data);
      if(e->data_len<0 || response->body.size()+size_t(e->data_len)>4096){response->too_large=true;return ESP_FAIL;}
      response->body.append(static_cast<const char*>(e->data),e->data_len);
    }
    return ESP_OK;
  }
  void fetch() {
    result_={};
    char url[512];
    const int size=std::snprintf(url,sizeof(url),
      "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
      "&current=temperature_2m,weather_code,is_day"
      "&daily=temperature_2m_max,weather_code"
      "&timezone=Europe%%2FZurich&forecast_days=1",latitude_,longitude_);
    if(size<0 || size>=int(sizeof(url))){result_status_="Weather URL invalid";return;}
    Response response;
    esp_http_client_config_t cfg{};
    cfg.url=url;cfg.crt_bundle_attach=esp_crt_bundle_attach;
    cfg.timeout_ms=15000;cfg.disable_auto_redirect=true;cfg.event_handler=event;cfg.user_data=&response;
    auto client=esp_http_client_init(&cfg);
    if(!client){result_status_="Weather network unavailable";return;}
    esp_http_client_set_header(client,"Accept","application/json");
    const auto err=esp_http_client_perform(client);
    const int code=esp_http_client_get_status_code(client);
    if(response.too_large)result_status_="Weather response too large";
    else if(err!=ESP_OK)result_status_="Weather connection failed";
    else if(code!=200)result_status_="Weather HTTP "+std::to_string(code);
    else {
      const auto now=std::time(nullptr);
      std::tm local{};localtime_r(&now,&local);
      char day[11];std::strftime(day,sizeof(day),"%Y-%m-%d",&local);
      result_status_=weather_parse(response.body,day,now,result_)?"Weather updated":"Weather data invalid";
    }
    esp_http_client_cleanup(client);
  }
};
inline WeatherClient weather_client;
}
