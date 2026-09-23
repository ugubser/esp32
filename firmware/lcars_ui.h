#pragma once
#include "controller_state.h"
#include "lcars_theme.h"
#include "lvgl.h"
#include "transit_model.h"
#include "transit_icons.h"
#include "weather_model.h"
#include "weather_icons.h"
#include <ctime>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

namespace lcars {
enum class SoundEffect { MENU, ACTION };

class Panel {
 public:
  static constexpr uint32_t DRAWER_IDLE_MS=10000, PAGE_IDLE_MS=120000;
  ControllerState state;
  void build(lv_obj_t *root, std::function<void(size_t, bool)> power,
             std::function<void(size_t, uint32_t)> scene,
             std::function<void(int, uint32_t)> dimmer,
             std::function<void(float)> backlight,
             std::function<void(bool)> rotation, bool inverted,
             std::function<void(SoundEffect)> sound,
             std::function<void(float)> volume, float initial_volume,
             std::function<void(size_t)> scheme, size_t initial_scheme) {
    power_ = std::move(power); scene_ = std::move(scene);
    dimmer_ = std::move(dimmer); backlight_ = std::move(backlight);
    rotation_ = std::move(rotation); inverted_ = inverted;
    sound_ = std::move(sound);
    volume_ = std::move(volume); volume_percent_ = int(std::lround(initial_volume*100));
    scheme_ = std::move(scheme); scheme_index_ = initial_scheme < THEMES.size() ? initial_scheme : 0;
    root_ = root;
    layout();
  }
  void transit_tick(const TransitBoard &board,int64_t now,bool wifi) {
    board_=&board;board_now_=now;board_wifi_=wifi;
    if(!ready_)return;
    const bool clock_ready=now>1700000000;
    char clock[16]="--:--:--",date[32]="-- --.--.";
    if(clock_ready) {
      static constexpr const char *days[]={"SO","MO","DI","MI","DO","FR","SA"};
      time_t stamp=now;tm local{};localtime_r(&stamp,&local);
      std::strftime(clock,sizeof(clock),"%H:%M:%S",&local);
      std::snprintf(date,sizeof(date),"%s %02d.%02d.",days[local.tm_wday%7],local.tm_mday,local.tm_mon+1);
    }
    set_label(clock_,clock);set_label(date_,date);
    bool errors=false;int64_t oldest=now;
    size_t row=0;
    for(size_t r=0;r<board.size();++r) {
      const auto &group=board[r];
      const bool stale=group.fetched && now-group.fetched>180;
      const bool available=wifi && clock_ready && group.error.empty() && !stale;
      auto next=available?transit_next(group,now,TRANSIT_ROUTES[r].rows):std::vector<Departure>{};
      if(!available)errors=true;
      if(group.fetched)oldest=std::min(oldest,group.fetched);
      for(size_t n=0;n<TRANSIT_ROUTES[r].rows;++n,++row) {
        const auto i=row;
        std::string destination,time="--:--",minutes="",delay="";
        bool imminent=false;
        if(n<next.size()) {
          destination=transit_display_text(next[n].destination);
          time_t stamp=next[n].at;tm local{};localtime_r(&stamp,&local);char value[8];
          std::strftime(value,sizeof(value),"%H:%M",&local);time=value;
          const auto remaining=(next[n].at-now+59)/60;
          imminent=remaining<=1;
          minutes=imminent?"<1 min":std::to_string(remaining)+" min";
          if(next[n].delay)delay="+"+std::to_string(next[n].delay)+"'";
        } else if(n==0) {
          destination=!wifi?"WLAN nicht verbunden":!clock_ready?"Uhr wird synchronisiert":stale?"Daten veraltet":
            !group.error.empty()?group.error:"Keine weiteren Abfahrten";
        } else destination="—";
        set_label(transit_destination_[i],destination.c_str());
        set_label(transit_time_[i],time.c_str());set_label(transit_minutes_[i],minutes.c_str());
        set_label(transit_delay_[i],delay.c_str());
        // Departures leaving within a minute blink between alert and normal.
        set_text_color(transit_minutes_[i],transit_minutes_color_[i],
                       imminent&&(now&1)?theme().alert:theme().tertiary);
      }
    }
    transit_status_=!wifi?"OFFLINE / WLAN":!clock_ready?"WARTE AUF UHRZEIT":errors?"DATEN NICHT VOLLSTAENDIG / AUTOMATISCHE AKTUALISIERUNG":
      "LIVE / AKTUALISIERT VOR "+std::to_string(std::max<int64_t>(0,now-oldest))+" S";
    update_status();
  }
  void weather_tick(const WeatherReading &weather,int64_t now,bool wifi) {
    weather_=&weather;weather_now_=now;weather_wifi_=wifi;
    if(!ready_)return;
    time_t stamp=now;tm local{};localtime_r(&stamp,&local);
    char day[11];std::strftime(day,sizeof(day),"%Y-%m-%d",&local);
    const bool available=wifi && weather_fresh(weather,now,day);
    if(available) {
      const auto age=std::max<int64_t>(0,now-weather.fetched)/60;
      weather_status_="SONNE "+weather.sunrise+" - "+weather.sunset+" / OPEN-METEO VOR "+std::to_string(age)+" MIN";
    } else weather_status_=!wifi?"OFFLINE / WLAN":"WETTERDATEN NICHT VERFUEGBAR";
    update_status();
    if(!available) {
      set_label(weather_now_label_,"--°");set_label(weather_high_label_,"--°");
      set_weather_icon(weather_now_icon_,weather_now_unknown_,now_weather_icon_,WeatherIcon::UNKNOWN,false);
      set_weather_icon(weather_today_icon_,weather_today_unknown_,today_weather_icon_,WeatherIcon::UNKNOWN,false);
      for(size_t i=0;i<FORECAST_DAYS;++i) {
        auto &c=forecast_[i];
        set_label(c.day,i==0?"HEUTE":"--");set_label(c.date,"--.--.");
        set_label(c.high,"--°");set_label(c.low,"--°");
        set_label(c.chance,"--%");set_label(c.rain,"-- MM");set_label(c.wind,"-- KM/H");
        set_weather_icon(c.icon,c.unknown,c.shown,WeatherIcon::UNKNOWN,true);
        visible(c.bar,false);c.bar_top=c.bar_height=-1;
      }
      return;
    }
    char value[24];
    std::snprintf(value,sizeof(value),"%d°",int(std::lround(weather.temperature_c)));
    set_label(weather_now_label_,value);
    std::snprintf(value,sizeof(value),"%d°",int(std::lround(weather.days[0].high_c)));
    set_label(weather_high_label_,value);
    set_weather_icon(weather_now_icon_,weather_now_unknown_,now_weather_icon_,
                     weather_icon(weather.current_code,weather.is_day),false);
    set_weather_icon(weather_today_icon_,weather_today_unknown_,today_weather_icon_,
                     weather_icon(weather.days[0].code,true),false);
    float lowest=weather.days[0].low_c,highest=weather.days[0].high_c;
    for(const auto &d:weather.days){lowest=std::min(lowest,d.low_c);highest=std::max(highest,d.high_c);}
    const float span=std::max(1.0f,highest-lowest);
    for(size_t i=0;i<FORECAST_DAYS;++i) {
      const auto &d=weather.days[i];auto &c=forecast_[i];
      set_label(c.day,i==0?"HEUTE":weather_weekday(d.date));
      std::snprintf(value,sizeof(value),"%.2s.%.2s.",d.date.c_str()+8,d.date.c_str()+5);set_label(c.date,value);
      std::snprintf(value,sizeof(value),"%d°",int(std::lround(d.high_c)));set_label(c.high,value);
      std::snprintf(value,sizeof(value),"%d°",int(std::lround(d.low_c)));set_label(c.low,value);
      std::snprintf(value,sizeof(value),"%d%%",d.precipitation_probability);set_label(c.chance,value);
      std::snprintf(value,sizeof(value),"%.1f MM",d.precipitation_mm);set_label(c.rain,value);
      std::snprintf(value,sizeof(value),"%d KM/H",int(std::lround(d.wind_kmh)));set_label(c.wind,value);
      set_weather_icon(c.icon,c.unknown,c.shown,weather_icon(d.code,true),true);
      // The bar spans the day's low to high on the week's shared scale.
      const int top=int(std::lround((highest-d.high_c)/span*BAR_TRACK));
      const int bottom=int(std::lround((highest-d.low_c)/span*BAR_TRACK));
      const int height=std::max(BAR_WIDTH,bottom-top);
      const int y=BAR_TOP+std::min(top,BAR_TRACK-height);
      if(y!=c.bar_top||height!=c.bar_height) {
        c.bar_top=y;c.bar_height=height;
        lv_obj_set_pos(c.bar,c.bar_x,y);lv_obj_set_height(c.bar,height);
      }
      visible(c.bar,true);
    }
  }
  void sound_status(bool busy, bool failed = false) {
    sound_busy_ = busy;sound_failed_=failed;
    if (!ready_) return;
    enabled(sound_button_, !busy);
    set_label(sound_label_, failed ? "AUDIO ERROR" : busy ? "PLAYING..." : "SPEAKER TEST");
  }
  void connection(bool wifi, bool ha) {
    const bool connected=wifi&&ha;
    if(wifi_==wifi&&state.connected()==connected)return;
    wifi_ = wifi;
    if (!wifi || !ha) {
      scene_ready_.fill(false); brightness_ = -1;
      living_request_.cancel(); feedback_.clear();
    }
    state.set_connected(connected); render();
  }
  void update(size_t index, const std::string &value) {
    state.update(index, value);
    ESP_LOGI("lcars", "Control %u received state: %s", static_cast<unsigned>(index + 1), value.c_str());
    render();
  }
  void update_scene(size_t index, const std::string &value) {
    if (index < SCENES.size()) scene_ready_[index] = state.connected() && scene_available(value);
    render();
  }
  void update_brightness(float value) {
    brightness_ = state.connected() && std::isfinite(value) ? static_cast<int>(std::lround(value * 100 / 255)) : -1;
    render();
  }
  void living_result(uint32_t request, bool success) {
    if (living_request_.finish(request, success)) {
      feedback_ = success ? "SENT: " + requested_label_ : "COMMAND FAILED";
      ESP_LOGI("lcars", "Living request %u: %s", static_cast<unsigned>(request), success ? "accepted" : "failed");
      render();
    }
  }
  void fail(size_t index) { state.fail(index); render(); }
  void rotation_changed(bool inverted) {
    inverted_=inverted;
    if(ready_)set_label(rotation_label_,inverted?"ORIENTATION: 180 DEG":"ORIENTATION: 0 DEG");
  }
  void tick(const std::string &ip) {
    const auto now=lv_tick_get();
    bool changed=state.tick(now);
    if (living_request_.tick(now)) { feedback_ = "COMMAND TIMED OUT"; changed=true; }
    if(ip_!=ip) {
      ip_=ip;changed=true;
      if(ready_)set_label(ip_label_,("IP ADDRESS: "+ip_).c_str());
    }
    if(ready_) {
      const auto idle=lv_tick_elaps(last_input_);
      if(drawer_open_&&idle>=DRAWER_IDLE_MS)close_drawer();
      if(page_!=Page::TRANSIT&&idle>=PAGE_IDLE_MS) {
        ESP_LOGI("lcars","Idle: returning to transit");
        show_page(Page::TRANSIT);changed=false;
      }
    }
    if(changed)render();
  }
  void show_room(size_t index) {
    if (index >= ROOMS.size()) return;
    room_index_ = index;
    show_page(index == 0 ? Page::LIVING : Page::ROOM);
    ESP_LOGI("lcars", "Room: %s", ROOMS[index].name);
  }
  size_t scheme() const { return scheme_index_; }
  bool drawer_open() const { return drawer_open_; }

 private:
  enum class Page { HOME, SYSTEM, ROOM, LIVING, TRANSIT, WEATHER };
  // Shared LCARS frame: header band, left rail with the drawer tab, footer band.
  static constexpr int CX=56, CY=48, CW=738, CH=392, INSET_W=627, INSET_H=338;
  static constexpr int DRAWER_W=212, BAR_TOP=150, BAR_TRACK=96, BAR_WIDTH=14;
  struct Context { Panel *panel; size_t index; };
  struct ForecastColumn {
    lv_obj_t *day{}, *date{}, *icon{}, *unknown{}, *high{}, *low{}, *bar{}, *chance{}, *rain{}, *wind{};
    WeatherIcon shown{WeatherIcon::UNKNOWN};
    int bar_x{}, bar_top{-1}, bar_height{-1};
  };
  bool ready_{false}, wifi_{false};
  bool inverted_{false};
  Page page_{Page::TRANSIT};
  size_t room_index_{0}, scene_page_{0}, scheme_index_{0}, pending_scheme_{0}, brightness_index_{1};
  int brightness_{-1};
  RequestState living_request_;
  std::string feedback_, requested_label_, ip_, transit_status_{"VERBINDE MIT TRANSIT"}, weather_status_, ha_status_;
  std::array<bool, SCENES.size()> scene_ready_{};
  std::function<void(size_t, bool)> power_;
  std::function<void(size_t, uint32_t)> scene_;
  std::function<void(int, uint32_t)> dimmer_;
  std::function<void(float)> backlight_;
  std::function<void(bool)> rotation_;
  std::function<void(SoundEffect)> sound_;
  std::function<void(float)> volume_;
  std::function<void(size_t)> scheme_;
  int volume_percent_{60};
  bool sound_busy_{}, sound_failed_{}, drawer_open_{};
  uint32_t last_input_{};
  // Latest inputs, re-applied immediately after a colour scheme rebuild.
  const TransitBoard *board_{};int64_t board_now_{};bool board_wifi_{};
  const WeatherReading *weather_{};int64_t weather_now_{};bool weather_wifi_{};
  lv_obj_t *root_{}, *volume_label_{}, *sound_button_{}, *sound_label_{}, *rotation_label_{};
  lv_obj_t *clock_{}, *date_{}, *status_{}, *status_bar_{};
  lv_obj_t *weather_now_label_{}, *weather_high_label_{}, *weather_now_icon_{}, *weather_today_icon_{};
  lv_obj_t *weather_now_unknown_{}, *weather_today_unknown_{};
  WeatherIcon now_weather_icon_{WeatherIcon::UNKNOWN},today_weather_icon_{WeatherIcon::UNKNOWN};
  std::array<ForecastColumn,FORECAST_DAYS> forecast_{};
  std::array<lv_obj_t*,12> transit_destination_{},transit_time_{},transit_minutes_{},transit_delay_{};
  std::array<uint32_t,12> transit_minutes_color_{};
  lv_obj_t *transit_{}, *weather_page_{}, *home_{}, *system_{}, *room_{}, *living_{};
  lv_obj_t *dim_{}, *drawer_{};
  std::array<lv_obj_t *, 4> nav_buttons_{};
  std::array<Context, 4> nav_contexts_{};
  lv_obj_t *link_label_{}, *ip_label_{}, *room_title_{}, *room_on_{}, *room_off_{};
  lv_obj_t *living_on_{}, *living_off_{}, *slider_{}, *brightness_label_{}, *feedback_label_{};
  lv_obj_t *prev_{}, *next_{}, *page_label_{};
  std::array<lv_obj_t *, ROOMS.size()> home_buttons_{}, home_titles_{};
  std::array<Context, ROOMS.size()> home_contexts_{};
  std::array<lv_obj_t *, 3> member_buttons_{}, member_titles_{}, member_values_{}, brightness_buttons_{};
  std::array<Context, 3> member_contexts_{}, brightness_contexts_{};
  std::array<lv_obj_t *, THEMES.size()> scheme_buttons_{}, scheme_labels_{};
  std::array<Context, THEMES.size()> scheme_contexts_{};
  std::array<lv_obj_t *, SCENES_PER_PAGE> scene_buttons_{}, scene_labels_{};
  std::array<Context, SCENES_PER_PAGE> scene_contexts_{};

  const Theme &theme() const { return THEMES[scheme_index_]; }
  static Panel *self(lv_event_t *e) { return static_cast<Panel *>(lv_event_get_user_data(e)); }
  static Context *context(lv_event_t *e) { return static_cast<Context *>(lv_event_get_user_data(e)); }
  void bind(lv_obj_t *o, lv_event_cb_t fn, void *data, SoundEffect effect=SoundEffect::ACTION) {
    // Check before the action can disable its button. PCM is fed later, so a
    // volume change in this same event applies before the first audio samples.
    lv_obj_add_event_cb(o, effect == SoundEffect::MENU ? menu_sound_event : action_sound_event,
                        LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(o, fn, LV_EVENT_CLICKED, data);
  }
  static void sound_event(lv_event_t *e, SoundEffect effect) {
    auto *p=self(e);
    p->last_input_=lv_tick_get();
    if (!lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_DISABLED) &&
        !p->sound_busy_) p->sound_(effect);
  }
  static void menu_sound_event(lv_event_t *e) { sound_event(e, SoundEffect::MENU); }
  static void action_sound_event(lv_event_t *e) { sound_event(e, SoundEffect::ACTION); }
  static lv_color_t color(uint32_t value) { return lv_color_hex(value); }
  static void set_label(lv_obj_t *o,const char *text) {
    if(std::strcmp(lv_label_get_text(o),text)!=0)lv_label_set_text(o,text);
  }
  static void set_text_color(lv_obj_t *o,uint32_t &current,uint32_t value) {
    if(current==value)return;
    current=value;lv_obj_set_style_text_color(o,color(value),0);
  }
  static const lv_image_dsc_t *weather_icon_source(WeatherIcon icon,bool large) {
    switch(icon) {
      case WeatherIcon::SUN: return large?&weather_sun_large_icon:&weather_sun_icon;
      case WeatherIcon::MOON: return large?&weather_moon_large_icon:&weather_moon_icon;
      case WeatherIcon::CLOUD_SUN: return large?&weather_cloud_sun_large_icon:&weather_cloud_sun_icon;
      case WeatherIcon::CLOUD_MOON: return large?&weather_cloud_moon_large_icon:&weather_cloud_moon_icon;
      case WeatherIcon::CLOUD: return large?&weather_cloud_large_icon:&weather_cloud_icon;
      case WeatherIcon::FOG: return large?&weather_cloud_fog_large_icon:&weather_cloud_fog_icon;
      case WeatherIcon::DRIZZLE: return large?&weather_cloud_drizzle_large_icon:&weather_cloud_drizzle_icon;
      case WeatherIcon::RAIN: return large?&weather_cloud_rain_large_icon:&weather_cloud_rain_icon;
      case WeatherIcon::SUN_RAIN: return large?&weather_cloud_sun_rain_large_icon:&weather_cloud_sun_rain_icon;
      case WeatherIcon::MOON_RAIN: return large?&weather_cloud_moon_rain_large_icon:&weather_cloud_moon_rain_icon;
      case WeatherIcon::SNOW: return large?&weather_cloud_snow_large_icon:&weather_cloud_snow_icon;
      case WeatherIcon::STORM: return large?&weather_cloud_lightning_large_icon:&weather_cloud_lightning_icon;
      case WeatherIcon::UNKNOWN: return nullptr;
    }
    return nullptr;
  }
  static void set_weather_icon(lv_obj_t *image,lv_obj_t *unknown,WeatherIcon &old_icon,WeatherIcon icon,bool large) {
    if(old_icon==icon)return;
    old_icon=icon;
    const auto *source=weather_icon_source(icon,large);
    if(source)lv_image_set_src(image,source);
    visible(image,source!=nullptr);
    visible(unknown,source==nullptr);
  }
  static void visible(lv_obj_t *o, bool show) {
    if(show==!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if (show) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  static void enabled(lv_obj_t *o, bool enable) {
    if(enable==!lv_obj_has_state(o,LV_STATE_DISABLED))return;
    if (enable) lv_obj_remove_state(o, LV_STATE_DISABLED); else lv_obj_add_state(o, LV_STATE_DISABLED);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_STATE_DISABLED);
  }
  lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius, bool clickable=false) const {
    auto *o = clickable ? lv_button_create(parent) : lv_obj_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color(bg), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0); lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    if (!clickable) lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    else press_feedback(o);
    return o;
  }
  // LCARS press animation: the button flashes bright and shrinks slightly on
  // touch, then fades back to its own colour after release.
  void press_feedback(lv_obj_t *o) const {
    static const lv_style_prop_t props[]={LV_STYLE_BG_COLOR,LV_STYLE_TRANSFORM_WIDTH,
                                          LV_STYLE_TRANSFORM_HEIGHT,LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t press,release;
    static const bool initialized=[] {
      lv_style_transition_dsc_init(&press,props,lv_anim_path_ease_out,60,0,nullptr);
      lv_style_transition_dsc_init(&release,props,lv_anim_path_ease_out,350,0,nullptr);
      return true;
    }();
    (void)initialized;
    lv_obj_set_style_transition(o,&release,0);
    lv_obj_set_style_transition(o,&press,LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(o,color(theme().flash),LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(o,-3,LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(o,-3,LV_STATE_PRESSED);
  }
  lv_obj_t *content(int x=CX, int y=CY, int w=CW, int h=CH) { return box(root_, x, y, w, h, BLACK, 0); }
  lv_obj_t *inset() { return content(CX+(CW-INSET_W)/2, CY+(CH-INSET_H)/2, INSET_W, INSET_H); }
  static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, uint32_t fg, const lv_font_t *font) {
    auto *o = lv_label_create(parent); lv_label_set_text(o, text); lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_color(o, color(fg), 0); lv_obj_set_style_text_font(o, font, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE); return o;
  }
  static lv_obj_t *centered(lv_obj_t *parent, const char *text, int x, int y, int w, uint32_t fg, const lv_font_t *font) {
    auto *o=label(parent,text,x,y,fg,font);lv_obj_set_width(o,w);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    return o;
  }
  lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, uint32_t bg,
                   int radius=0, const lv_font_t *font=&lv_font_montserrat_20) const {
    auto *o = box(parent, x, y, w, h, bg, radius, true);
    auto *l = label(o, text, 0, 0, BLACK, font); lv_obj_center(l); return o;
  }
  static lv_obj_t *image(lv_obj_t *parent, const lv_image_dsc_t *source, int x, int y) {
    auto *o=lv_image_create(parent);
    if(source)lv_image_set_src(o,source);
    lv_obj_set_pos(o,x,y);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    return o;
  }

  // (Re)creates every widget in the current colour scheme.
  void layout() {
    ready_=false;
    lv_anim_delete(nullptr,drawer_x);
    lv_obj_clean(root_);
    lv_obj_set_style_pad_all(root_,0,0);
    lv_obj_set_style_bg_color(root_,color(BLACK),0);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
    now_weather_icon_=today_weather_icon_=WeatherIcon::UNKNOWN;
    transit_minutes_color_.fill(0);drawer_open_=false;
    build_frame();
    transit_=content();weather_page_=content();system_=content();
    home_=inset();room_=inset();living_=inset();
    build_transit(); build_weather(); build_home(); build_system(); build_room(); build_living();
    build_drawer();
    last_input_=lv_tick_get();
    ready_ = true;
    if(board_)transit_tick(*board_,board_now_,board_wifi_);
    if(weather_)weather_tick(*weather_,weather_now_,weather_wifi_);
    sound_status(sound_busy_,sound_failed_);
    show_page(page_);
    // The frame sweeps in after boot and after a colour scheme change.
    reveal(root_);
  }
  void build_frame() {
    const auto &t=theme();
    // Top-left elbow: outer rounded block, squared joins and a concave inner corner.
    box(root_,6,6,130,70,t.primary,34);
    box(root_,70,6,66,36,t.primary,0);
    box(root_,6,40,40,70,t.primary,0);
    box(root_,46,42,120,60,BLACK,14);
    // Header band: weather at a glance, date and time in the gaps between segments.
    label(root_,"JETZT",146,15,t.secondary,&lv_font_montserrat_16);
    weather_now_label_=label(root_,"--°",200,13,t.text,&lv_font_montserrat_20);
    weather_now_icon_=image(root_,nullptr,244,10);lv_obj_add_flag(weather_now_icon_,LV_OBJ_FLAG_HIDDEN);
    weather_now_unknown_=label(root_,"?",250,13,t.tertiary,&lv_font_montserrat_20);
    box(root_,282,6,12,36,t.secondary,0);
    label(root_,"MAX HEUTE",304,15,t.secondary,&lv_font_montserrat_16);
    weather_high_label_=label(root_,"--°",412,13,t.text,&lv_font_montserrat_20);
    weather_today_icon_=image(root_,nullptr,454,10);lv_obj_add_flag(weather_today_icon_,LV_OBJ_FLAG_HIDDEN);
    weather_today_unknown_=label(root_,"?",460,13,t.tertiary,&lv_font_montserrat_20);
    box(root_,490,6,66,36,t.tertiary,0);
    date_=label(root_,"-- --.--.",566,13,t.tertiary,&lv_font_montserrat_20);
    clock_=label(root_,"--:--:--",676,13,t.primary,&lv_font_montserrat_20);
    box(root_,780,6,14,36,t.primary,0);
    // Left rail. The tab in the middle slides the navigation drawer in.
    box(root_,6,116,40,34,t.tertiary,0);
    auto *tab=button(root_,LV_SYMBOL_RIGHT,6,156,40,168,t.secondary,0);
    bind(tab,[](lv_event_t *e){self(e)->open_drawer();},this,SoundEffect::MENU);
    box(root_,6,330,40,34,t.tertiary,0);
    // Bottom-left elbow and footer band with the page status.
    box(root_,6,370,40,74,t.primary,0);
    box(root_,6,410,130,64,t.primary,32);
    box(root_,70,446,66,28,t.primary,0);
    box(root_,46,378,120,68,BLACK,14);
    status_=label(root_,"",146,451,t.tertiary,&lv_font_montserrat_16);
    lv_obj_set_style_max_width(status_,600,0);lv_label_set_long_mode(status_,LV_LABEL_LONG_CLIP);
    status_bar_=box(root_,760,446,20,28,t.secondary,0);
    box(root_,784,446,10,28,t.primary,0);
  }
  void update_status() {
    if(!ready_)return;
    const std::string &text=page_==Page::TRANSIT?transit_status_:page_==Page::WEATHER?weather_status_:ha_status_;
    if(std::strcmp(lv_label_get_text(status_),text.c_str())==0)return;
    lv_label_set_text(status_,text.c_str());
    // The footer segment grows to meet the end of the status text.
    lv_obj_update_layout(status_);
    const int x=std::min(760,int(lv_obj_get_x(status_)+lv_obj_get_width(status_))+12);
    lv_obj_set_x(status_bar_,x);lv_obj_set_width(status_bar_,780-x);
  }
  void build_drawer() {
    const auto &t=theme();
    dim_=box(root_,0,0,800,480,BLACK,0);
    lv_obj_set_style_bg_opa(dim_,LV_OPA_60,0);lv_obj_add_flag(dim_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dim_,[](lv_event_t *e){auto *p=self(e);p->last_input_=lv_tick_get();p->close_drawer();},
                        LV_EVENT_CLICKED,this);
    lv_obj_add_flag(dim_,LV_OBJ_FLAG_HIDDEN);
    drawer_=box(root_,-DRAWER_W,0,DRAWER_W,480,BLACK,0);
    box(drawer_,6,6,160,64,t.primary,32);
    box(drawer_,6,38,160,32,t.primary,0);
    box(drawer_,88,6,78,32,t.primary,0);
    auto *title=label(drawer_,"LCARS",0,0,BLACK,&lv_font_montserrat_16);
    lv_obj_align(title,LV_ALIGN_TOP_RIGHT,-58,48);
    constexpr const char *names[]={"TRANSIT","WEATHER","LIGHTS","SYSTEM"};
    for(size_t i=0;i<nav_buttons_.size();++i) {
      auto *b=box(drawer_,6,76+int(i)*84,160,78,t.secondary,0,true);
      auto *l=label(b,names[i],0,0,BLACK,&lv_font_montserrat_20);
      lv_obj_align(l,LV_ALIGN_BOTTOM_RIGHT,-10,-6);
      nav_buttons_[i]=b;nav_contexts_[i]={this,i};
      bind(b,[](lv_event_t *e){
        auto *c=context(e);
        constexpr Page pages[]={Page::TRANSIT,Page::WEATHER,Page::HOME,Page::SYSTEM};
        c->panel->close_drawer();c->panel->show_page(pages[c->index]);
      },&nav_contexts_[i],SoundEffect::MENU);
    }
    box(drawer_,6,412,160,62,t.primary,31);
    box(drawer_,6,412,160,30,t.primary,0);
    box(drawer_,88,442,78,32,t.primary,0);
    auto *close=button(drawer_,LV_SYMBOL_LEFT,172,6,34,468,t.secondary,17);
    bind(close,[](lv_event_t *e){self(e)->close_drawer();},this,SoundEffect::MENU);
    lv_obj_add_flag(drawer_,LV_OBJ_FLAG_HIDDEN);
  }
  static void drawer_x(void *o,int32_t x) { lv_obj_set_x(static_cast<lv_obj_t*>(o),x); }
  void slide_drawer(int32_t to,uint32_t ms,lv_anim_path_cb_t path,lv_anim_completed_cb_t done) {
    lv_anim_delete(drawer_,drawer_x);
    lv_anim_t a;lv_anim_init(&a);
    lv_anim_set_var(&a,drawer_);lv_anim_set_exec_cb(&a,drawer_x);
    lv_anim_set_values(&a,lv_obj_get_x(drawer_),to);
    lv_anim_set_duration(&a,ms);lv_anim_set_path_cb(&a,path);
    lv_anim_set_user_data(&a,this);lv_anim_set_completed_cb(&a,done);
    lv_anim_start(&a);
  }
  void open_drawer() {
    if(drawer_open_)return;
    drawer_open_=true;
    visible(dim_,true);visible(drawer_,true);
    slide_drawer(0,220,lv_anim_path_ease_out,nullptr);
  }
  void close_drawer() {
    if(!drawer_open_)return;
    drawer_open_=false;
    slide_drawer(-DRAWER_W,180,lv_anim_path_ease_in,[](lv_anim_t *a){
      auto *p=static_cast<Panel*>(lv_anim_get_user_data(a));
      visible(p->dim_,false);visible(p->drawer_,false);
    });
  }
  // Staggered fade-in from the top-left corner, like an LCARS panel powering up.
  static void reveal(lv_obj_t *parent) {
    lv_area_t origin;lv_obj_get_coords(parent,&origin);
    for(uint32_t i=0;i<lv_obj_get_child_count(parent);++i) {
      auto *child=lv_obj_get_child(parent,i);
      if(lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;
      lv_area_t a;lv_obj_get_coords(child,&a);
      const int delay=std::clamp(int(a.x1-origin.x1)/12+int(a.y1-origin.y1)/6,0,100);
      lv_obj_fade_in(child,120,uint32_t(delay));
    }
  }
  void build_transit() {
    const auto &t=theme();
    int y=0;size_t row_index=0;
    const char *last_station=nullptr;
    for(size_t r=0;r<TRANSIT_ROUTES.size();++r) {
      if(!last_station || std::strcmp(last_station,TRANSIT_ROUTES[r].station)!=0) {
        const int header_y=y;y+=20;last_station=TRANSIT_ROUTES[r].station;
        box(transit_,0,header_y+4,28,13,t.secondary,6);
        label(transit_,TRANSIT_ROUTES[r].station,38,header_y+1,t.secondary,&lv_font_montserrat_16);
      }
      for(size_t n=0;n<TRANSIT_ROUTES[r].rows;++n,++row_index) {
        const auto i=row_index;const int row=y;y+=26;
        auto *icon=image(transit_,r==0?&tram_icon:r>=3?&bus_icon:&train_icon,0,row-1);
        lv_obj_set_style_image_recolor(icon,color(t.tertiary),0);
        lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0);
        label(transit_,TRANSIT_ROUTES[r].line,38,row+1,t.primary,&lv_font_montserrat_20);
        transit_destination_[i]=label(transit_,n==0?"Laden...":"—",100,row+1,t.text,&lv_font_montserrat_20);
        lv_obj_set_width(transit_destination_[i],350);lv_label_set_long_mode(transit_destination_[i],LV_LABEL_LONG_CLIP);
        transit_time_[i]=label(transit_,"--:--",466,row+1,t.text,&lv_font_montserrat_20);
        transit_minutes_[i]=label(transit_,"",566,row+1,t.tertiary,&lv_font_montserrat_20);
        transit_minutes_color_[i]=t.tertiary;
        transit_delay_[i]=label(transit_,"",676,row+1,t.alert,&lv_font_montserrat_20);
        box(transit_,38,row+25,CW-38,1,t.off_bg,0);
      }
    }
  }
  void build_weather() {
    const auto &t=theme();
    constexpr int step=CW/int(FORECAST_DAYS), width=step-6;
    for(size_t i=0;i<FORECAST_DAYS;++i) {
      auto &c=forecast_[i];const int x=int(i)*step;
      c=ForecastColumn{};
      auto *pill=box(weather_page_,x,0,width,34,i==0?t.primary:t.secondary,17);
      c.day=centered(pill,i==0?"HEUTE":"--",0,8,width,BLACK,&lv_font_montserrat_16);
      c.date=centered(weather_page_,"--.--.",x,40,width,t.tertiary,&lv_font_montserrat_16);
      c.icon=image(weather_page_,nullptr,x+(width-48)/2,62);lv_obj_add_flag(c.icon,LV_OBJ_FLAG_HIDDEN);
      c.unknown=centered(weather_page_,"?",x,72,width,t.tertiary,&lv_font_montserrat_28);
      c.high=centered(weather_page_,"--°",x,112,width,t.primary,&lv_font_montserrat_28);
      c.bar_x=x+(width-BAR_WIDTH)/2;
      box(weather_page_,c.bar_x,BAR_TOP,BAR_WIDTH,BAR_TRACK,t.off_bg,BAR_WIDTH/2);
      c.bar=box(weather_page_,c.bar_x,BAR_TOP,BAR_WIDTH,BAR_TRACK,i==0?t.primary:t.secondary,BAR_WIDTH/2);
      lv_obj_add_flag(c.bar,LV_OBJ_FLAG_HIDDEN);
      c.low=centered(weather_page_,"--°",x,252,width,t.tertiary,&lv_font_montserrat_20);
      box(weather_page_,x,282,width,2,t.off_bg,0);
      c.chance=centered(weather_page_,"--%",x,290,width,t.tertiary,&lv_font_montserrat_20);
      c.rain=centered(weather_page_,"-- MM",x,316,width,t.text,&lv_font_montserrat_16);
      box(weather_page_,x,342,width,2,t.off_bg,0);
      c.wind=centered(weather_page_,"-- KM/H",x,350,width,t.text,&lv_font_montserrat_16);
    }
  }
  void build_home() {
    const auto &t=theme();
    label(home_, "ENVIRONMENTAL CONTROL", 0, 0, t.secondary, &lv_font_montserrat_20);
    for (size_t i = 0; i < ROOMS.size(); ++i) {
      const int x=(i % 2)*320, y=36+(i / 2)*102;
      auto *b = box(home_, x, y, 189, 92, t.off_bg, 32, true);
      home_buttons_[i] = b;
      home_titles_[i] = label(b, ROOMS[i].name, 0, 0, t.text, &lv_font_montserrat_20);
      lv_obj_center(home_titles_[i]);
      home_contexts_[i] = {this, i};
      bind(b, [](lv_event_t *e) { auto *c=context(e); c->panel->toggle_room(c->index); }, &home_contexts_[i]);
      auto *menu=button(home_, "> MENU", x+197, y, 109, 92, t.secondary, 28);
      bind(menu, [](lv_event_t *e) { auto *c=context(e); c->panel->show_room(c->index); },
           &home_contexts_[i], SoundEffect::MENU);
    }
    label(home_, "HOME ASSISTANT", 340, 264, t.secondary, &lv_font_montserrat_16);
    link_label_ = label(home_, "WAITING", 340, 292, t.tertiary, &lv_font_montserrat_20);
  }
  void build_system() {
    const auto &t=theme();
    label(system_, "SYSTEM CONFIGURATION", 0, 0, t.secondary, &lv_font_montserrat_20);
    ip_label_ = label(system_, ip_.empty()?"IP ADDRESS: CONNECTING":("IP ADDRESS: "+ip_).c_str(), 400, 4, t.text, &lv_font_montserrat_16);
    label(system_, "DISPLAY BRIGHTNESS", 0, 36, t.tertiary, &lv_font_montserrat_16);
    constexpr const char *names[] = {"DIM", "NORMAL", "BRIGHT"};
    for (size_t i = 0; i < 3; ++i) {
      auto *b=button(system_, names[i], i*160, 58, 150, 50, i==brightness_index_?t.primary:t.secondary, 25);
      brightness_buttons_[i]=b; brightness_contexts_[i]={this,i};
      bind(b, [](lv_event_t *e) {
        auto *c=context(e); const float levels[]={.2f,.6f,1.f}; c->panel->backlight_(levels[c->index]);
        c->panel->brightness_index_=c->index;
        const auto &t=c->panel->theme();
        for (size_t j=0;j<3;++j) lv_obj_set_style_bg_color(c->panel->brightness_buttons_[j],color(j==c->index?t.primary:t.secondary),0);
      }, &brightness_contexts_[i]);
    }
    rotation_label_=label(system_,inverted_?"ORIENTATION: 180 DEG":"ORIENTATION: 0 DEG",500,36,t.text,&lv_font_montserrat_16);
    auto *rotate=button(system_,"ROTATE 180",500,58,238,50,t.tertiary,25);
    bind(rotate,[](lv_event_t *e){auto *p=self(e);p->rotation_(!p->inverted_);},this);
    volume_label_=label(system_, "", 0, 124, t.secondary, &lv_font_montserrat_16);
    update_volume_label();
    auto *lower=button(system_, "- VOL", 0, 146, 150, 50, t.tertiary, 25);
    auto *higher=button(system_, "+ VOL", 160, 146, 150, 50, t.tertiary, 25);
    bind(lower,[](lv_event_t *e){self(e)->change_volume(-10);},this);
    bind(higher,[](lv_event_t *e){self(e)->change_volume(10);},this);
    sound_label_=label(system_, "SPEAKER TEST", 320, 124, t.secondary, &lv_font_montserrat_16);
    sound_button_=button(system_, "CONTROL SOUND", 320, 146, 418, 50, t.secondary, 25);
    bind(sound_button_, [](lv_event_t *) {}, this); // Shared feedback plays the test sound once.
    label(system_, "COLOUR SCHEME", 0, 212, t.tertiary, &lv_font_montserrat_16);
    for(size_t i=0;i<THEMES.size();++i) {
      // Each button previews its scheme; the active one is filled.
      const bool active=i==scheme_index_;
      auto *b=button(system_,THEMES[i].name,int(i)*187,234,176,60,active?THEMES[i].primary:t.off_bg,30,&lv_font_montserrat_16);
      scheme_buttons_[i]=b;scheme_labels_[i]=lv_obj_get_child(b,0);scheme_contexts_[i]={this,i};
      lv_obj_set_style_text_color(scheme_labels_[i],color(active?BLACK:THEMES[i].primary),0);
      bind(b,[](lv_event_t *e){auto *c=context(e);c->panel->select_scheme(c->index);},&scheme_contexts_[i]);
    }
    label(system_, "FNK0115Q / LCARS 2.0.0", 0, 318, t.tertiary, &lv_font_montserrat_16);
  }
  void select_scheme(size_t index) {
    if(index>=THEMES.size()||index==scheme_index_)return;
    pending_scheme_=index;
    // Rebuild after this click event returns; its button is about to be deleted.
    lv_async_call([](void *data){
      auto *p=static_cast<Panel*>(data);
      p->scheme_index_=p->pending_scheme_;
      ESP_LOGI("lcars","Colour scheme: %s",THEMES[p->scheme_index_].name);
      p->scheme_(p->scheme_index_);
      p->layout();
    },this);
  }
  void update_volume_label() {
    set_label(volume_label_, volume_percent_ == 0 ? "VOLUME: MUTED" :
      ("VOLUME: " + std::to_string(volume_percent_) + "%").c_str());
  }
  void change_volume(int delta) {
    int next=std::max(0,std::min(100,volume_percent_+delta));
    if(next==volume_percent_)return;
    volume_percent_=next;update_volume_label();volume_(next/100.0f);
  }
  void back_button(lv_obj_t *parent,int h) {
    auto *back=button(parent,"< BACK",0,0,100,h,theme().tertiary,h/2,&lv_font_montserrat_16);
    bind(back,[](lv_event_t *e){self(e)->show_page(Page::HOME);},this,SoundEffect::MENU);
  }
  void build_room() {
    const auto &t=theme();
    back_button(room_,42);
    room_title_=label(room_, "ROOM", 115, 10, t.secondary, &lv_font_montserrat_20);
    room_on_=button(room_, "ALL ON", 340, 0, 136, 42, t.primary, 21);
    room_off_=button(room_, "ALL OFF", 490, 0, 136, 42, t.tertiary, 21);
    bind(room_on_, [](lv_event_t *e){self(e)->set_room(true);},this);
    bind(room_off_, [](lv_event_t *e){self(e)->set_room(false);},this);
    for(size_t i=0;i<3;++i) {
      auto *b=box(room_,0,64+i*86,627,74,t.off_bg,28,true); member_buttons_[i]=b;
      member_titles_[i]=label(b,"",20,12,t.text,&lv_font_montserrat_20);
      member_values_[i]=label(b,"",20,43,t.tertiary,&lv_font_montserrat_16);
      member_contexts_[i]={this,i};
      bind(b,[](lv_event_t *e){auto *c=context(e); const auto &r=ROOMS[c->panel->room_index_];
        if(c->index<r.count)c->panel->toggle(r.members[c->index]);},&member_contexts_[i]);
    }
  }
  void build_living() {
    const auto &t=theme();
    back_button(living_,38);
    label(living_,"LIVING ROOM",115,8,t.secondary,&lv_font_montserrat_20);
    living_on_=button(living_,"ON",438,0,88,38,t.primary,19);
    living_off_=button(living_,"OFF",538,0,88,38,t.tertiary,19);
    bind(living_on_,[](lv_event_t *e){self(e)->set_room(true);},this);
    bind(living_off_,[](lv_event_t *e){self(e)->set_room(false);},this);
    brightness_label_=label(living_,"BRIGHTNESS: --",0,45,t.text,&lv_font_montserrat_16);
    slider_=lv_slider_create(living_); lv_obj_set_pos(slider_,20,83);lv_obj_set_size(slider_,586,14);
    lv_slider_set_range(slider_,1,100);lv_obj_set_ext_click_area(slider_,16);
    lv_obj_set_style_bg_color(slider_,color(t.off_bg),LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_,color(t.primary),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_,color(t.secondary),LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_,9,LV_PART_KNOB);
    lv_obj_add_event_cb(slider_,[](lv_event_t *e){auto *p=self(e);p->last_input_=lv_tick_get();
      lv_label_set_text(brightness_label_of(p), ("BRIGHTNESS: "+std::to_string(lv_slider_get_value(p->slider_))+"%").c_str());
    },LV_EVENT_VALUE_CHANGED,this);
    lv_obj_add_event_cb(slider_,[](lv_event_t *e){self(e)->apply_brightness();},LV_EVENT_RELEASED,this);
    for(size_t i=0;i<SCENES_PER_PAGE;++i) {
      auto *b=button(living_,"",(i%2)*320,113+(i/2)*57,306,48,t.secondary,24,&lv_font_montserrat_16);
      scene_buttons_[i]=b;scene_labels_[i]=lv_obj_get_child(b,0);scene_contexts_[i]={this,i};
      bind(b,[](lv_event_t *e){auto *c=context(e);c->panel->activate_scene(c->panel->scene_page_*SCENES_PER_PAGE+c->index);},&scene_contexts_[i]);
    }
    prev_=button(living_,"<",0,294,76,39,t.tertiary,19);
    page_label_=label(living_,"1 / 3",99,305,t.primary,&lv_font_montserrat_16);
    next_=button(living_,">",174,294,76,39,t.tertiary,19);
    bind(prev_,[](lv_event_t *e){auto *p=self(e);if(p->scene_page_>0)--p->scene_page_;p->render();},this,SoundEffect::MENU);
    bind(next_,[](lv_event_t *e){auto *p=self(e);if(p->scene_page_+1<SCENE_PAGES)++p->scene_page_;p->render();},this,SoundEffect::MENU);
    feedback_label_=label(living_,"",271,297,t.tertiary,&lv_font_montserrat_16);
    lv_obj_set_width(feedback_label_,350);lv_label_set_long_mode(feedback_label_,LV_LABEL_LONG_WRAP);
  }
  static lv_obj_t *brightness_label_of(Panel *p) { return p->brightness_label_; }
  void show_page(Page page) {
    // A rebuild keeps the page; layout() then reveals the whole screen instead.
    const bool changed=page!=page_;
    page_=page;
    close_drawer();
    visible(transit_,page==Page::TRANSIT);visible(weather_page_,page==Page::WEATHER);
    visible(home_,page==Page::HOME);visible(system_,page==Page::SYSTEM);
    visible(room_,page==Page::ROOM);visible(living_,page==Page::LIVING);
    const auto &t=theme();
    const size_t active=page==Page::TRANSIT?0:page==Page::WEATHER?1:page==Page::SYSTEM?3:2;
    for(size_t i=0;i<nav_buttons_.size();++i)
      lv_obj_set_style_bg_color(nav_buttons_[i],color(i==active?t.primary:i%2?t.tertiary:t.secondary),0);
    render();update_status();
    if(changed)reveal(page_container(page));
  }
  lv_obj_t *page_container(Page page) const {
    switch(page) {
      case Page::HOME: return home_;
      case Page::SYSTEM: return system_;
      case Page::ROOM: return room_;
      case Page::LIVING: return living_;
      case Page::WEATHER: return weather_page_;
      case Page::TRANSIT: return transit_;
    }
    return transit_;
  }
  void toggle(size_t index) {
    const bool on=state.at(index).state!=State::ON;
    if(state.begin(index,lv_tick_get())) {power_(index,on);render();}
  }
  void set_room(bool on) {
    set_room_power(room_index_,on);
  }
  void toggle_room(size_t index) {
    if(index>=ROOMS.size())return;
    set_room_power(index,summarize(state,index).on==0);
  }
  void set_room_power(size_t index, bool on) {
    const auto &r=ROOMS[index];const auto summary=summarize(state,index);
    if(!state.connected()||!summary.ready()||(index==0&&living_request_.pending))return;
    ESP_LOGI("lcars","Room power: %s %s",r.name,on?"ON":"OFF");
    for(size_t i=0;i<r.count;++i) {
      const size_t index=r.members[i];
      if((state.at(index).state==State::ON)!=on)toggle(index);
    }
  }
  void activate_scene(size_t index) {
    if(index>=SCENES.size()||!scene_ready_[index]||!state.can_control(0)||living_request_.pending)return;
    const auto request=living_request_.begin(lv_tick_get());
    requested_label_=SCENES[index].name;feedback_="SENDING: "+requested_label_;
    ESP_LOGI("lcars","Scene requested: %s",SCENES[index].id);scene_(index,request);render();
  }
  void apply_brightness() {
    if(!state.can_control(0)||living_request_.pending)return;
    const int value=lv_slider_get_value(slider_);const auto request=living_request_.begin(lv_tick_get());
    requested_label_="BRIGHTNESS "+std::to_string(value)+"%";feedback_="APPLYING BRIGHTNESS";
    ESP_LOGI("lcars","Living brightness requested: %d%%",value);dimmer_(value,request);render();
  }
  const char *control_status(size_t index) const {
    const auto &c=state.at(index);
    return !state.connected()?"OFFLINE":c.pending?"SENDING...":c.failed?"COMMAND FAILED":
      c.state==State::ON?"ON / TAP TO TURN OFF":c.state==State::OFF?"OFF / TAP TO TURN ON":
      c.state==State::UNAVAILABLE?"UNAVAILABLE":"WAITING FOR STATE";
  }
  void render() {
    if(!ready_)return;
    const auto &t=theme();
    ha_status_=!wifi_?"WIFI DISCONNECTED":state.connected()?"HOME ASSISTANT / CONNECTED":"WIFI READY / WAITING FOR HOME ASSISTANT";
    update_status();
    set_label(link_label_,state.connected()?"CONNECTED":"OFFLINE");
    for(size_t i=0;i<ROOMS.size();++i) {
      const auto summary=summarize(state,i);
      const bool on=state.connected()&&summary.on>0;
      enabled(home_buttons_[i],state.connected()&&summary.ready()&&!(i==0&&living_request_.pending));
      lv_obj_set_style_bg_color(home_buttons_[i],color(on?t.primary:t.off_bg),0);
      lv_obj_set_style_text_color(home_titles_[i],color(on?BLACK:t.text),0);
    }
    const auto &room=ROOMS[room_index_];const auto summary=summarize(state,room_index_);
    if(page_==Page::ROOM) {
      set_label(room_title_,room.name);
      enabled(room_on_,state.connected()&&summary.ready()&&summary.on<room.count);
      enabled(room_off_,state.connected()&&summary.ready()&&summary.on>0);
      for(size_t n=0;n<3;++n) {
        visible(member_buttons_[n],n<room.count);if(n>=room.count)continue;
        const auto i=room.members[n];const bool on=state.at(i).state==State::ON;
        set_label(member_titles_[n],ENTITIES[i].name);set_label(member_values_[n],control_status(i));
        enabled(member_buttons_[n],state.can_control(i));
        lv_obj_set_style_bg_color(member_buttons_[n],color(on?t.primary:t.off_bg),0);
        lv_obj_set_style_text_color(member_titles_[n],color(on?BLACK:t.text),0);
        lv_obj_set_style_text_color(member_values_[n],color(on?BLACK:t.tertiary),0);
      }
    }
    if(page_==Page::LIVING) {
      const bool ready=state.can_control(0)&&!living_request_.pending;
      enabled(living_on_,ready&&state.at(0).state==State::OFF);
      enabled(living_off_,ready&&state.at(0).state==State::ON);enabled(slider_,ready);
      if(!lv_obj_has_state(slider_,LV_STATE_PRESSED)) {
        const int value=state.at(0).state==State::OFF?0:brightness_;
        if(value>=0)lv_slider_set_value(slider_,value<1?1:value,LV_ANIM_ON);
        set_label(brightness_label_,(value>=0?"BRIGHTNESS: "+std::to_string(value)+"%":"BRIGHTNESS: --").c_str());
      }
      for(size_t n=0;n<SCENES_PER_PAGE;++n) {
        const size_t i=scene_page_*SCENES_PER_PAGE+n;visible(scene_buttons_[n],i<SCENES.size());if(i>=SCENES.size())continue;
        set_label(scene_labels_[n],SCENES[i].name);lv_obj_center(scene_labels_[n]);
        const bool available=ready&&scene_ready_[i];enabled(scene_buttons_[n],available);
        lv_obj_set_style_bg_color(scene_buttons_[n],color(available?t.secondary:t.off_bg),0);
        lv_obj_set_style_text_color(scene_labels_[n],color(available?BLACK:t.tertiary),0);
      }
      enabled(prev_,scene_page_>0);enabled(next_,scene_page_+1<SCENE_PAGES);
      set_label(page_label_,(std::to_string(scene_page_+1)+" / "+std::to_string(SCENE_PAGES)).c_str());
      set_label(feedback_label_,!state.connected()?"HOME ASSISTANT OFFLINE":state.at(0).failed?"LIGHT COMMAND FAILED":feedback_.c_str());
    }
  }
};
inline Panel panel;
}  // namespace lcars
