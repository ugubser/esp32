#pragma once
#include "controller_state.h"
#include "lvgl.h"
#include "transit_model.h"
#include "transit_icons.h"
#include <ctime>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace lcars {
enum class SoundEffect { MENU, ACTION };

class Panel {
 public:
  ControllerState state;
  void build(lv_obj_t *root, std::function<void(size_t, bool)> power,
             std::function<void(size_t, uint32_t)> scene,
             std::function<void(int, uint32_t)> dimmer,
             std::function<void(float)> backlight,
             std::function<void(bool)> rotation, bool inverted,
             std::function<void(SoundEffect)> sound,
             std::function<void(float)> volume, float initial_volume) {
    power_ = std::move(power); scene_ = std::move(scene);
    dimmer_ = std::move(dimmer); backlight_ = std::move(backlight);
    rotation_ = std::move(rotation); inverted_ = inverted;
    sound_ = std::move(sound);
    volume_ = std::move(volume); volume_percent_ = int(std::lround(initial_volume*100));
    auto *screen_root=root;
    lv_obj_set_style_pad_all(root,0,0);
    lcars_root_=box(root,0,0,800,480,BLACK,0);root=lcars_root_;
    lv_obj_set_style_bg_color(root, color(BLACK), 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    box(root, 18, 18, 118, 70, PEACH, 42);
    box(root, 98, 51, 38, 37, BLACK, 23);
    box(root, 155, 18, 119, 21, LILAC, 0);
    label(root, "LCARS / HOME CONTROL", 295, 15, PEACH, &lv_font_montserrat_28);
    box(root, 155, 59, 627, 13, PEACH, 0);
    home_nav_ = button(root, "LIGHTS", 18, 94, 118, 64, PEACH);
    system_nav_ = button(root, "SYSTEM", 18, 166, 118, 64, BLUE);
    back_nav_ = button(root, "< BACK", 18, 238, 118, 64, LILAC);
    bind(home_nav_, [](lv_event_t *e) { self(e)->show_page(Page::HOME); }, this, SoundEffect::MENU);
    bind(system_nav_, [](lv_event_t *e) { self(e)->show_page(Page::SYSTEM); }, this, SoundEffect::MENU);
    bind(back_nav_, [](lv_event_t *e) { self(e)->show_page(Page::HOME); }, this, SoundEffect::MENU);
    auto *transit_nav=button(root,"TRANSIT",18,310,118,106,BLUE,12);
    bind(transit_nav,[](lv_event_t *e){self(e)->show_page(Page::TRANSIT);},this,SoundEffect::MENU);
    box(root, 18, 432, 118, 25, LILAC, 13);
    footer_ = label(root, "WAITING FOR WIFI", 155, 437, BLUE, &lv_font_montserrat_16);
    home_ = content(root); system_ = content(root); room_ = content(root); living_ = content(root);
    build_home(); build_system(); build_room(); build_living();
    build_transit(screen_root);
    ready_ = true; show_page(Page::TRANSIT);
  }
  void transit_tick(const TransitBoard &board,int64_t now,bool wifi) {
    if(!ready_)return;
    const bool clock_ready=now>1700000000;
    char clock[16]="--:--:--";
    if(clock_ready){time_t stamp=now;tm local{};localtime_r(&stamp,&local);std::strftime(clock,sizeof(clock),"%H:%M:%S",&local);}
    lv_label_set_text(transit_clock_,clock);
    bool errors=false;int64_t oldest=now;
    for(size_t r=0;r<board.size();++r) {
      const auto &group=board[r];
      const bool stale=group.fetched && now-group.fetched>180;
      const bool available=wifi && clock_ready && group.error.empty() && !stale;
      auto next=available?transit_next(group,now):std::vector<Departure>{};
      if(!available)errors=true;
      if(group.fetched)oldest=std::min(oldest,group.fetched);
      for(size_t n=0;n<3;++n) {
        const auto i=r*3+n;
        std::string destination,time="--:--",minutes="",delay="";
        if(n<next.size()) {
          destination=next[n].destination;
          time_t stamp=next[n].at;tm local{};localtime_r(&stamp,&local);char value[8];
          std::strftime(value,sizeof(value),"%H:%M",&local);time=value;
          const auto remaining=(next[n].at-now+59)/60;
          minutes=remaining<=1?"<1 min":std::to_string(remaining)+" min";
          if(next[n].delay)delay="+"+std::to_string(next[n].delay)+"'";
        } else if(n==0) {
          destination=!wifi?"WLAN nicht verbunden":!clock_ready?"Uhr wird synchronisiert":stale?"Daten veraltet":
            !group.error.empty()?group.error:"Keine weiteren Abfahrten";
        } else destination="—";
        lv_label_set_text(transit_destination_[i],destination.c_str());
        lv_label_set_text(transit_time_[i],time.c_str());lv_label_set_text(transit_minutes_[i],minutes.c_str());
        lv_label_set_text(transit_delay_[i],delay.c_str());
      }
    }
    const std::string status=!wifi?"OFFLINE / WLAN":!clock_ready?"WARTE AUF UHRZEIT":errors?"DATEN NICHT VOLLSTAENDIG / AUTOMATISCHE AKTUALISIERUNG":
      "LIVE / AKTUALISIERT VOR "+std::to_string(std::max<int64_t>(0,now-oldest))+" S";
    lv_label_set_text(transit_status_,status.c_str());
  }
  void sound_status(bool busy, bool failed = false) {
    sound_busy_ = busy;
    if (!ready_) return;
    enabled(sound_button_, !busy);
    lv_label_set_text(sound_label_, failed ? "AUDIO ERROR" : busy ? "PLAYING..." : "SPEAKER TEST");
  }
  void connection(bool wifi, bool ha) {
    wifi_ = wifi;
    if (!wifi || !ha) {
      scene_ready_.fill(false); brightness_ = -1;
      living_request_.cancel(); feedback_.clear();
    }
    state.set_connected(wifi && ha); render();
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
    if(ready_)lv_label_set_text(rotation_label_,inverted?"ORIENTATION: 180 DEG":"ORIENTATION: 0 DEG");
  }
  void tick(const std::string &ip) {
    state.tick(lv_tick_get());
    if (living_request_.tick(lv_tick_get())) feedback_ = "COMMAND TIMED OUT";
    if (ready_) lv_label_set_text(ip_label_, ("IP ADDRESS: " + ip).c_str());
    render();
  }
  void show_room(size_t index) {
    if (index >= ROOMS.size()) return;
    room_index_ = index;
    show_page(index == 0 ? Page::LIVING : Page::ROOM);
    ESP_LOGI("lcars", "Room: %s", ROOMS[index].name);
  }
 private:
  enum class Page { HOME, SYSTEM, ROOM, LIVING, TRANSIT };
  static constexpr uint32_t BLACK=0x060608, PEACH=0xFFB780, LILAC=0xC2A5E5,
      BLUE=0x99B9EE, TEXT=0xFFE3C6, OFF_BG=0x241D2D;
  struct Context { Panel *panel; size_t index; };
  bool ready_{false}, wifi_{false};
  bool inverted_{false};
  Page page_{Page::HOME};
  size_t room_index_{0}, scene_page_{0};
  int brightness_{-1};
  RequestState living_request_;
  std::string feedback_, requested_label_;
  std::array<bool, SCENES.size()> scene_ready_{};
  std::function<void(size_t, bool)> power_;
  std::function<void(size_t, uint32_t)> scene_;
  std::function<void(int, uint32_t)> dimmer_;
  std::function<void(float)> backlight_;
  std::function<void(bool)> rotation_;
  std::function<void(SoundEffect)> sound_;
  std::function<void(float)> volume_;
  int volume_percent_{60};
  lv_obj_t *volume_label_{};
  bool sound_busy_{};
  lv_obj_t *sound_button_{}, *sound_label_{};
  lv_obj_t *rotation_label_{};
  lv_obj_t *lcars_root_{}, *transit_{}, *transit_clock_{}, *transit_status_{};
  std::array<lv_obj_t*,12> transit_destination_{},transit_time_{},transit_minutes_{},transit_delay_{};
  lv_obj_t *home_{}, *system_{}, *room_{}, *living_{}, *home_nav_{}, *system_nav_{}, *back_nav_{};
  lv_obj_t *footer_{}, *link_label_{}, *ip_label_{}, *room_title_{}, *room_on_{}, *room_off_{};
  lv_obj_t *living_on_{}, *living_off_{}, *slider_{}, *brightness_label_{}, *feedback_label_{};
  lv_obj_t *prev_{}, *next_{}, *page_label_{};
  std::array<lv_obj_t *, ROOMS.size()> home_buttons_{}, home_titles_{};
  std::array<Context, ROOMS.size()> home_contexts_{};
  std::array<lv_obj_t *, 3> member_buttons_{}, member_titles_{}, member_values_{}, brightness_buttons_{};
  std::array<Context, 3> member_contexts_{}, brightness_contexts_{};
  std::array<lv_obj_t *, SCENES_PER_PAGE> scene_buttons_{}, scene_labels_{};
  std::array<Context, SCENES_PER_PAGE> scene_contexts_{};

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
    if (!lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_DISABLED) &&
        !p->sound_busy_) p->sound_(effect);
  }
  static void menu_sound_event(lv_event_t *e) { sound_event(e, SoundEffect::MENU); }
  static void action_sound_event(lv_event_t *e) { sound_event(e, SoundEffect::ACTION); }
  static lv_color_t color(uint32_t value) { return lv_color_hex(value); }
  static void visible(lv_obj_t *o, bool show) {
    if (show) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  static void enabled(lv_obj_t *o, bool enable) {
    if (enable) lv_obj_remove_state(o, LV_STATE_DISABLED); else lv_obj_add_state(o, LV_STATE_DISABLED);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_STATE_DISABLED);
  }
  static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius, bool clickable=false) {
    auto *o = clickable ? lv_button_create(parent) : lv_obj_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color(bg), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0); lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    if (!clickable) lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
  }
  static lv_obj_t *content(lv_obj_t *root) { return box(root, 155, 88, 627, 338, BLACK, 0); }
  static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, uint32_t fg, const lv_font_t *font) {
    auto *o = lv_label_create(parent); lv_label_set_text(o, text); lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_color(o, color(fg), 0); lv_obj_set_style_text_font(o, font, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE); return o;
  }
  static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, uint32_t bg,
                          int radius=0, const lv_font_t *font=&lv_font_montserrat_20) {
    auto *o = box(parent, x, y, w, h, bg, radius, true);
    auto *l = label(o, text, 0, 0, BLACK, font); lv_obj_center(l); return o;
  }
  void build_transit(lv_obj_t *root) {
    constexpr uint32_t bg=0x00157E,header=0x1C48A1,white=0xFFFFFF,yellow=0xE2BA29;
    transit_=box(root,0,0,800,480,bg,0);
    box(transit_,0,0,800,44,header,0);
    label(transit_,"ABFAHRTEN",12,8,white,&lv_font_montserrat_28);
    transit_clock_=label(transit_,"--:--:--",431,11,white,&lv_font_montserrat_20);
    auto *lights=button(transit_,"LIGHTS",580,5,102,34,0x6083C6,5,&lv_font_montserrat_16);
    auto *system=button(transit_,"SYSTEM",690,5,100,34,0x6083C6,5,&lv_font_montserrat_16);
    bind(lights,[](lv_event_t *e){self(e)->show_page(Page::HOME);},this,SoundEffect::MENU);
    bind(system,[](lv_event_t *e){self(e)->show_page(Page::SYSTEM);},this,SoundEffect::MENU);
    label(transit_,"Linie",55,46,white,&lv_font_montserrat_16);
    label(transit_,"Ziel",137,46,white,&lv_font_montserrat_16);
    label(transit_,"Abfahrt",480,46,white,&lv_font_montserrat_16);
    label(transit_,"In",611,46,white,&lv_font_montserrat_16);
    label(transit_,"Delay",724,46,white,&lv_font_montserrat_16);
    for(size_t r=0;r<4;++r) {
      const int y=66+int(r)*98;
      box(transit_,0,y,800,20,0x102A85,0);
      label(transit_,TRANSIT_ROUTES[r].station,8,y+1,0xAAB8E1,&lv_font_montserrat_16);
      for(size_t n=0;n<3;++n) {
        const auto i=r*3+n;const int row=y+20+int(n)*26;
        auto *icon=lv_image_create(transit_);lv_image_set_src(icon,r==0?&tram_icon:r==3?&bus_icon:&train_icon);
        lv_obj_set_pos(icon,7,row-1);lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE);
        label(transit_,TRANSIT_ROUTES[r].line,55,row+1,white,&lv_font_montserrat_20);
        transit_destination_[i]=label(transit_,n==0?"Laden...":"—",137,row+1,white,&lv_font_montserrat_20);
        lv_obj_set_width(transit_destination_[i],330);lv_label_set_long_mode(transit_destination_[i],LV_LABEL_LONG_CLIP);
        transit_time_[i]=label(transit_,"--:--",480,row+1,white,&lv_font_montserrat_20);
        transit_minutes_[i]=label(transit_,"",611,row+1,white,&lv_font_montserrat_20);
        transit_delay_[i]=label(transit_,"",724,row+1,yellow,&lv_font_montserrat_20);
        box(transit_,0,row+25,800,1,0x5C6BA8,0);
      }
    }
    box(transit_,0,458,800,22,header,0);
    transit_status_=label(transit_,"VERBINDE MIT TRANSIT",10,461,white,&lv_font_montserrat_16);
  }
  void build_home() {
    label(home_, "ENVIRONMENTAL CONTROL", 0, 0, LILAC, &lv_font_montserrat_20);
    for (size_t i = 0; i < ROOMS.size(); ++i) {
      const int x=(i % 2)*320, y=36+(i / 2)*102;
      auto *b = box(home_, x, y, 189, 92, OFF_BG, 32, true);
      home_buttons_[i] = b;
      home_titles_[i] = label(b, ROOMS[i].name, 0, 0, TEXT, &lv_font_montserrat_20);
      lv_obj_center(home_titles_[i]);
      home_contexts_[i] = {this, i};
      bind(b, [](lv_event_t *e) { auto *c=context(e); c->panel->toggle_room(c->index); }, &home_contexts_[i]);
      auto *menu=button(home_, "> MENU", x+197, y, 109, 92, LILAC, 28);
      bind(menu, [](lv_event_t *e) { auto *c=context(e); c->panel->show_room(c->index); },
           &home_contexts_[i], SoundEffect::MENU);
    }
    label(home_, "HOME ASSISTANT", 340, 264, LILAC, &lv_font_montserrat_16);
    link_label_ = label(home_, "WAITING", 340, 292, BLUE, &lv_font_montserrat_20);
  }
  void build_system() {
    label(system_, "SYSTEM CONFIGURATION", 0, 0, LILAC, &lv_font_montserrat_20);
    ip_label_ = label(system_, "IP ADDRESS: CONNECTING", 0, 45, TEXT, &lv_font_montserrat_20);
    label(system_, "DISPLAY BRIGHTNESS", 0, 85, BLUE, &lv_font_montserrat_20);
    constexpr const char *names[] = {"DIM", "NORMAL", "BRIGHT"};
    for (size_t i = 0; i < 3; ++i) {
      auto *b=button(system_, names[i], i*210, 115, 197, 56, i==1?PEACH:LILAC, 28);
      brightness_buttons_[i]=b; brightness_contexts_[i]={this,i};
      bind(b, [](lv_event_t *e) {
        auto *c=context(e); const float levels[]={.2f,.6f,1.f}; c->panel->backlight_(levels[c->index]);
        for (size_t j=0;j<3;++j) lv_obj_set_style_bg_color(c->panel->brightness_buttons_[j],color(j==c->index?PEACH:LILAC),0);
      }, &brightness_contexts_[i]);
    }
    rotation_label_=label(system_,inverted_?"ORIENTATION: 180 DEG":"ORIENTATION: 0 DEG",0,197,TEXT,&lv_font_montserrat_16);
    auto *rotate=button(system_,"ROTATE 180",315,177,311,56,BLUE,28);
    bind(rotate,[](lv_event_t *e){auto *p=self(e);p->rotation_(!p->inverted_);},this);
    volume_label_=label(system_, "", 0, 235, LILAC, &lv_font_montserrat_16);
    update_volume_label();
    auto *lower=button(system_, "- VOL", 0, 258, 118, 56, BLUE, 28);
    auto *higher=button(system_, "+ VOL", 126, 258, 118, 56, BLUE, 28);
    bind(lower,[](lv_event_t *e){self(e)->change_volume(-10);},this);
    bind(higher,[](lv_event_t *e){self(e)->change_volume(10);},this);
    sound_label_=label(system_, "SPEAKER TEST", 315, 235, LILAC, &lv_font_montserrat_16);
    sound_button_=button(system_, "CONTROL SOUND", 315, 258, 311, 56, LILAC, 28);
    bind(sound_button_, [](lv_event_t *) {}, this); // Shared feedback plays the test sound once.
    label(system_, "FNK0115Q / LCARS 1.6", 0, 318, BLUE, &lv_font_montserrat_16);
  }
  void update_volume_label() {
    lv_label_set_text(volume_label_, volume_percent_ == 0 ? "VOLUME: MUTED" :
      ("VOLUME: " + std::to_string(volume_percent_) + "%").c_str());
  }
  void change_volume(int delta) {
    int next=std::max(0,std::min(100,volume_percent_+delta));
    if(next==volume_percent_)return;
    volume_percent_=next;update_volume_label();volume_(next/100.0f);
  }
  void build_room() {
    room_title_=label(room_, "ROOM", 0, 10, LILAC, &lv_font_montserrat_20);
    room_on_=button(room_, "ALL ON", 340, 0, 136, 42, PEACH, 21);
    room_off_=button(room_, "ALL OFF", 490, 0, 136, 42, BLUE, 21);
    bind(room_on_, [](lv_event_t *e){self(e)->set_room(true);},this);
    bind(room_off_, [](lv_event_t *e){self(e)->set_room(false);},this);
    for(size_t i=0;i<3;++i) {
      auto *b=box(room_,0,64+i*86,627,74,OFF_BG,28,true); member_buttons_[i]=b;
      member_titles_[i]=label(b,"",20,12,TEXT,&lv_font_montserrat_20);
      member_values_[i]=label(b,"",20,43,BLUE,&lv_font_montserrat_16);
      member_contexts_[i]={this,i};
      bind(b,[](lv_event_t *e){auto *c=context(e); const auto &r=ROOMS[c->panel->room_index_];
        if(c->index<r.count)c->panel->toggle(r.members[c->index]);},&member_contexts_[i]);
    }
  }
  void build_living() {
    label(living_,"LIVING ROOM",0,8,LILAC,&lv_font_montserrat_20);
    living_on_=button(living_,"ON",438,0,88,38,PEACH,19);
    living_off_=button(living_,"OFF",538,0,88,38,BLUE,19);
    bind(living_on_,[](lv_event_t *e){self(e)->set_room(true);},this);
    bind(living_off_,[](lv_event_t *e){self(e)->set_room(false);},this);
    brightness_label_=label(living_,"BRIGHTNESS: --",0,45,TEXT,&lv_font_montserrat_16);
    slider_=lv_slider_create(living_); lv_obj_set_pos(slider_,20,83);lv_obj_set_size(slider_,586,14);
    lv_slider_set_range(slider_,1,100);lv_obj_set_ext_click_area(slider_,16);
    lv_obj_set_style_bg_color(slider_,color(OFF_BG),LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_,color(PEACH),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_,color(LILAC),LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_,9,LV_PART_KNOB);
    lv_obj_add_event_cb(slider_,[](lv_event_t *e){auto *p=self(e);
      lv_label_set_text(brightness_label_of(p), ("BRIGHTNESS: "+std::to_string(lv_slider_get_value(p->slider_))+"%").c_str());
    },LV_EVENT_VALUE_CHANGED,this);
    lv_obj_add_event_cb(slider_,[](lv_event_t *e){self(e)->apply_brightness();},LV_EVENT_RELEASED,this);
    for(size_t i=0;i<SCENES_PER_PAGE;++i) {
      auto *b=button(living_,"",(i%2)*320,113+(i/2)*57,306,48,LILAC,24,&lv_font_montserrat_16);
      scene_buttons_[i]=b;scene_labels_[i]=lv_obj_get_child(b,0);scene_contexts_[i]={this,i};
      bind(b,[](lv_event_t *e){auto *c=context(e);c->panel->activate_scene(c->panel->scene_page_*SCENES_PER_PAGE+c->index);},&scene_contexts_[i]);
    }
    prev_=button(living_,"<",0,294,76,39,BLUE,19);
    page_label_=label(living_,"1 / 3",99,305,PEACH,&lv_font_montserrat_16);
    next_=button(living_,">",174,294,76,39,BLUE,19);
    bind(prev_,[](lv_event_t *e){auto *p=self(e);if(p->scene_page_>0)--p->scene_page_;p->render();},this,SoundEffect::MENU);
    bind(next_,[](lv_event_t *e){auto *p=self(e);if(p->scene_page_+1<SCENE_PAGES)++p->scene_page_;p->render();},this,SoundEffect::MENU);
    feedback_label_=label(living_,"",271,297,BLUE,&lv_font_montserrat_16);
    lv_obj_set_width(feedback_label_,350);lv_label_set_long_mode(feedback_label_,LV_LABEL_LONG_WRAP);
  }
  static lv_obj_t *brightness_label_of(Panel *p) { return p->brightness_label_; }
  void show_page(Page page) {
    page_=page;
    visible(lcars_root_,page!=Page::TRANSIT);visible(transit_,page==Page::TRANSIT);
    visible(home_,page==Page::HOME);visible(system_,page==Page::SYSTEM);
    visible(room_,page==Page::ROOM);visible(living_,page==Page::LIVING);
    visible(back_nav_,page!=Page::HOME);
    lv_obj_set_style_bg_color(home_nav_,color(page==Page::HOME?PEACH:LILAC),0);
    lv_obj_set_style_bg_color(system_nav_,color(page==Page::SYSTEM?PEACH:BLUE),0);
    render();
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
    lv_label_set_text(footer_,!wifi_?"WIFI DISCONNECTED":state.connected()?"HOME ASSISTANT / CONNECTED":"WIFI READY / WAITING FOR HOME ASSISTANT");
    lv_label_set_text(link_label_,state.connected()?"CONNECTED":"OFFLINE");
    for(size_t i=0;i<ROOMS.size();++i) {
      const auto summary=summarize(state,i);
      const bool on=state.connected()&&summary.on>0;
      enabled(home_buttons_[i],state.connected()&&summary.ready()&&!(i==0&&living_request_.pending));
      lv_obj_set_style_bg_color(home_buttons_[i],color(on?PEACH:OFF_BG),0);
      lv_obj_set_style_text_color(home_titles_[i],color(on?BLACK:TEXT),0);
    }
    const auto &room=ROOMS[room_index_];const auto summary=summarize(state,room_index_);
    if(page_==Page::ROOM) {
      lv_label_set_text(room_title_,room.name);
      enabled(room_on_,state.connected()&&summary.ready()&&summary.on<room.count);
      enabled(room_off_,state.connected()&&summary.ready()&&summary.on>0);
      for(size_t n=0;n<3;++n) {
        visible(member_buttons_[n],n<room.count);if(n>=room.count)continue;
        const auto i=room.members[n];const bool on=state.at(i).state==State::ON;
        lv_label_set_text(member_titles_[n],ENTITIES[i].name);lv_label_set_text(member_values_[n],control_status(i));
        enabled(member_buttons_[n],state.can_control(i));
        lv_obj_set_style_bg_color(member_buttons_[n],color(on?PEACH:OFF_BG),0);
        lv_obj_set_style_text_color(member_titles_[n],color(on?BLACK:TEXT),0);
        lv_obj_set_style_text_color(member_values_[n],color(on?BLACK:BLUE),0);
      }
    }
    if(page_==Page::LIVING) {
      const bool ready=state.can_control(0)&&!living_request_.pending;
      enabled(living_on_,ready&&state.at(0).state==State::OFF);
      enabled(living_off_,ready&&state.at(0).state==State::ON);enabled(slider_,ready);
      if(!lv_obj_has_state(slider_,LV_STATE_PRESSED)) {
        const int value=state.at(0).state==State::OFF?0:brightness_;
        if(value>=0)lv_slider_set_value(slider_,value<1?1:value,LV_ANIM_OFF);
        lv_label_set_text(brightness_label_,(value>=0?"BRIGHTNESS: "+std::to_string(value)+"%":"BRIGHTNESS: --").c_str());
      }
      for(size_t n=0;n<SCENES_PER_PAGE;++n) {
        const size_t i=scene_page_*SCENES_PER_PAGE+n;visible(scene_buttons_[n],i<SCENES.size());if(i>=SCENES.size())continue;
        lv_label_set_text(scene_labels_[n],SCENES[i].name);lv_obj_center(scene_labels_[n]);
        const bool available=ready&&scene_ready_[i];enabled(scene_buttons_[n],available);
        lv_obj_set_style_bg_color(scene_buttons_[n],color(available?LILAC:OFF_BG),0);
        lv_obj_set_style_text_color(scene_labels_[n],color(available?BLACK:BLUE),0);
      }
      enabled(prev_,scene_page_>0);enabled(next_,scene_page_+1<SCENE_PAGES);
      lv_label_set_text(page_label_,(std::to_string(scene_page_+1)+" / "+std::to_string(SCENE_PAGES)).c_str());
      lv_label_set_text(feedback_label_,!state.connected()?"HOME ASSISTANT OFFLINE":state.at(0).failed?"LIGHT COMMAND FAILED":feedback_.c_str());
    }
  }
};
inline Panel panel;
}  // namespace lcars
