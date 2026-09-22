#pragma once
#include "transit_json.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <ctime>

namespace lcars {
class TransitClient {
 public:
  void tick(bool wifi,const std::string &key) {
    if(done_.load(std::memory_order_acquire)) {
      board=std::move(result_);result_={};done_.store(false,std::memory_order_relaxed);busy_=false;
      status="";
      for(size_t r=0;r<board.size();++r) {
        if(!status.empty())status+="; ";
        status+=std::string(TRANSIT_ROUTES[r].line)+": "+(board[r].error.empty()?std::to_string(board[r].departures.size())+" departures":board[r].error);
      }
      ESP_LOGI("transit","%s",status.c_str());
    }
    if(!wifi || std::time(nullptr)<1700000000 || busy_)return;
    const auto now=uint32_t(xTaskGetTickCount()*portTICK_PERIOD_MS);
    if(started_ && uint32_t(now-last_)<60000)return;
    if(key.empty()){status="API key missing";return;}
    key_=key;started_=true;last_=now;busy_=true;
    if(xTaskCreate([](void *arg){
      auto *self=static_cast<TransitClient*>(arg);self->fetch();
      self->done_.store(true,std::memory_order_release);vTaskDelete(nullptr);
    },"transit_fetch",12288,this,1,nullptr)!=pdPASS){busy_=false;status="Network task failed";}
  }
  TransitBoard board{};
  std::string status{"Waiting for clock / Wi-Fi"};
 private:
  bool busy_{},started_{};uint32_t last_{};std::atomic<bool> done_{false};
  std::string key_;TransitBoard result_{};
  struct Response {std::string body;bool too_large{};unsigned connections{};};
  static esp_err_t event(esp_http_client_event_t *e) {
    auto *response=static_cast<Response*>(e->user_data);
    if(e->event_id==HTTP_EVENT_ON_CONNECTED)++response->connections;
    if(e->event_id==HTTP_EVENT_ON_DATA) {
      if(e->data_len<0 || response->body.size()+size_t(e->data_len)>65536){response->too_large=true;return ESP_FAIL;}
      response->body.append(static_cast<const char*>(e->data),e->data_len);
    }
    return ESP_OK;
  }
  void fetch() {
    ESP_LOGI("transit","Fetching departures; internal free %u, largest %u, clock %lld",
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),static_cast<long long>(std::time(nullptr)));
    const auto started=xTaskGetTickCount();
    Response response;
    const auto first_url=std::string("https://transit.tribecans.com/api/stationboard?stop=")+TRANSIT_STOPS[0]+"&limit=60";
    esp_http_client_config_t cfg{};
    cfg.url=first_url.c_str();cfg.crt_bundle_attach=esp_crt_bundle_attach;
    cfg.timeout_ms=15000;cfg.disable_auto_redirect=true;cfg.event_handler=event;cfg.user_data=&response;
    auto client=esp_http_client_init(&cfg);
    const auto auth="Bearer "+key_;
    if(client) {
      esp_http_client_set_header(client,"Authorization",auth.c_str());
      esp_http_client_set_header(client,"Accept","application/json");
    }
    for(unsigned stop=0;stop<TRANSIT_STOPS.size();++stop) {
      std::string error;response.body.clear();response.too_large=false;
      if(!client)error="Netzwerkfehler";
      else {
        esp_err_t err=ESP_OK;
        if(stop) {
          const auto url=std::string("https://transit.tribecans.com/api/stationboard?stop=")+TRANSIT_STOPS[stop]+"&limit=60";
          err=esp_http_client_set_url(client,url.c_str());
        }
        if(err==ESP_OK)err=esp_http_client_perform(client);
        const int code=err==ESP_OK?esp_http_client_get_status_code(client):0;
        if(err!=ESP_OK) {
          int tls_error=0,verify_flags=0;
          esp_http_client_get_and_clear_last_tls_error(client,&tls_error,&verify_flags);
          ESP_LOGW("transit","Stop %u: %s, HTTP %d, TLS %d, verify flags %d",stop,esp_err_to_name(err),code,tls_error,verify_flags);
        }
        if(response.too_large)error="Antwort zu gross";
        else if(err!=ESP_OK)error="Verbindung fehlgeschlagen";
        else if(code!=200)error="API HTTP "+std::to_string(code);
        else if(!transit_parse(response.body,stop,std::time(nullptr),result_))error="Daten ungueltig";
      }
      if(!error.empty())for(size_t r=0;r<result_.size();++r)
        if(TRANSIT_ROUTES[r].stop==stop)result_[r]=TransitGroup{{},error,0};
    }
    if(client)esp_http_client_cleanup(client);
    ESP_LOGI("transit","Refresh used %u connection(s) in %u ms",response.connections,
      unsigned((xTaskGetTickCount()-started)*portTICK_PERIOD_MS));
  }
};
inline TransitClient transit_client;
}
