#define ESP_LOGI(...) do {} while (0)
#include "../firmware/lcars_ui.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

static std::array<uint16_t,800*480> pixels;
static size_t sounds=0;
static void refresh() { lv_tick_inc(40); lv_timer_handler(); lv_refr_now(nullptr); }
static lv_obj_t *find(lv_obj_t *obj,const char *text) {
  if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return nullptr;
  if(lv_obj_check_type(obj,&lv_label_class)&&std::strcmp(lv_label_get_text(obj),text)==0)return obj;
  for(uint32_t i=0;i<lv_obj_get_child_count(obj);++i)
    if(auto *r=find(lv_obj_get_child(obj,i),text))return r;
  return nullptr;
}
static lv_obj_t *click(const char *text) {
  lcars::panel.sound_status(false); // Normal taps are spaced after playback completes.
  auto sounds_before=sounds;
  auto *label=find(lv_screen_active(),text);assert(label);
  auto *button=lv_obj_get_parent(label);assert(!lv_obj_has_state(button,LV_STATE_DISABLED));
  lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);refresh();
  assert(sounds==sounds_before+1);return button;
}
static void open_room(const char *name) {
  lcars::panel.sound_status(false);
  auto sounds_before=sounds;
  auto *title=find(lv_screen_active(),name);assert(title);
  auto *toggle=lv_obj_get_parent(title);
  auto *menu=lv_obj_get_child(lv_obj_get_parent(toggle),lv_obj_get_index(toggle)+1);
  assert(menu&&find(menu,"> MENU")&&!lv_obj_has_state(menu,LV_STATE_DISABLED));
  lv_area_t left,right;lv_obj_get_coords(toggle,&left);lv_obj_get_coords(menu,&right);
  assert(right.x1>left.x2&&right.y1==left.y1&&right.y2==left.y2);
  lv_obj_send_event(menu,LV_EVENT_CLICKED,nullptr);refresh();
  assert(sounds==sounds_before+1);
}
static void bounds(lv_obj_t *obj) {
  if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return;
  lv_area_t a;lv_obj_get_coords(obj,&a);
  if(!(a.x1>=0&&a.y1>=0&&a.x2<800&&a.y2<480)) {
    std::cerr<<"Out of bounds: "<<a.x1<<","<<a.y1<<"-"<<a.x2<<","<<a.y2;
    if(lv_obj_check_type(obj,&lv_label_class))std::cerr<<" label="<<lv_label_get_text(obj);
    std::cerr<<"\n";
  }
  assert(a.x1>=0&&a.y1>=0&&a.x2<800&&a.y2<480);
  for(uint32_t i=0;i<lv_obj_get_child_count(obj);++i)bounds(lv_obj_get_child(obj,i));
}
static void snapshot(const char *name) {
  for(int i=0;i<6;++i)refresh();
  bounds(lv_screen_active());
  std::ofstream out(std::string("logs/ui-")+name+".ppm",std::ios::binary);
  out<<"P6\n800 480\n255\n";
  for(auto c:pixels){char rgb[]={static_cast<char>(((c>>11)&31)*255/31),static_cast<char>(((c>>5)&63)*255/63),static_cast<char>((c&31)*255/31)};out.write(rgb,3);}
}
int main() {
  lv_init();auto *display=lv_display_create(800,480);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size()*sizeof(uint16_t),LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display,[](lv_display_t *d,const lv_area_t *,uint8_t *){lv_display_flush_ready(d);});
  auto *root=lv_obj_create(lv_screen_active());lv_obj_set_size(root,800,480);lv_obj_set_pos(root,0,0);
  lv_obj_set_style_border_width(root,0,0);lv_obj_set_style_radius(root,0,0);
  std::vector<std::pair<size_t,bool>> power;
  std::vector<std::pair<size_t,uint32_t>> scenes;
  std::vector<std::pair<int,uint32_t>> dim;
  std::vector<bool> rotations;
  std::vector<float> volumes;
  lcars::panel.build(root,[&](size_t i,bool on){power.emplace_back(i,on);},
    [&](size_t i,uint32_t request){scenes.emplace_back(i,request);},
    [&](int level,uint32_t request){dim.emplace_back(level,request);},[](float){},
    [&](bool inverted){rotations.push_back(inverted);lcars::panel.rotation_changed(inverted);},false,
    [&](){++sounds;lcars::panel.sound_status(true);},
    [&](float volume){volumes.push_back(volume);},0.6f);
  setenv("TZ","Europe/Zurich",1);tzset();
  lcars::TransitBoard transit;
  const auto transit_now=lcars::transit_timestamp("2026-09-13T17:00:00Z");
  for(size_t r=0;r<4;++r) {
    transit[r].error="";transit[r].fetched=transit_now;
    for(int n=0;n<3;++n)transit[r].departures.push_back({r==0?"Bahnhof Stettbach":r==1?"Weinfelden":r==2?"Winterthur":"Milchbuck",transit_now+int64_t(180+r*60+n*600),n==0?2:0});
  }
  lcars::panel.transit_tick(transit,transit_now,true);
  assert(find(root,"ABFAHRTEN")&&find(root,"Bahnhof Stettbach")&&find(root,"Weinfelden")&&find(root,"Winterthur")&&find(root,"Milchbuck"));
  assert(find(root,"19:00:00")&&find(root,"19:03"));snapshot("transit");
  lcars::panel.transit_tick(transit,transit_now,false);
  assert(find(root,"WLAN nicht verbunden")&&!find(root,"Bahnhof Stettbach"));snapshot("transit-offline");
  lcars::panel.transit_tick(transit,transit_now+181,true);
  assert(find(root,"Daten veraltet")&&!find(root,"Weinfelden"));
  lcars::panel.transit_tick(transit,transit_now,true);
  refresh();assert(sounds==0); // Building and rendering never play a sound.
  click("SYSTEM");assert(sounds==1);
  // A second tap during playback still navigates, without stacking another hail.
  lv_obj_send_event(lv_obj_get_parent(find(root,"LIGHTS")),LV_EVENT_CLICKED,nullptr);
  refresh();assert(sounds==1&&find(root,"KITCHEN"));
  open_room("LIVING ROOM");assert(power.empty()); // Navigation never toggles a room.
  auto *bright=find(root,"BRIGHT");assert(bright&&lv_obj_has_state(lv_obj_get_parent(bright),LV_STATE_DISABLED));
  click("< BACK");
  lcars::panel.connection(true,true);
  for(size_t i=0;i<lcars::CONTROL_COUNT;++i)lcars::panel.update(i,i%2?"off":"on");
  for(size_t i=0;i<lcars::SCENES.size();++i)lcars::panel.update_scene(i,"unknown");
  lcars::panel.update_brightness(128);snapshot("main");
  open_room("LIVING ROOM");snapshot("living-1");click("BRIGHT");assert(scenes.back().first==0);
  lcars::panel.living_result(scenes.back().second,true);
  click(">");snapshot("living-2");click("RELAX");assert(scenes.back().first==6);
  lcars::panel.living_result(scenes.back().second,true);
  click(">");snapshot("living-3");click("100% BRIGHT");assert(scenes.back().first==17);
  lcars::panel.living_result(scenes.back().second,true);
  auto *living=lv_obj_get_parent(find(root,"LIVING ROOM"));lv_obj_t *slider=nullptr;
  for(uint32_t i=0;i<lv_obj_get_child_count(living);++i){auto *o=lv_obj_get_child(living,i);if(lv_obj_check_type(o,&lv_slider_class))slider=o;}
  auto quiet_count=sounds;lcars::panel.sound_status(false);
  assert(slider);lv_slider_set_value(slider,37,LV_ANIM_OFF);lv_obj_send_event(slider,LV_EVENT_RELEASED,nullptr);
  assert(dim.size()==1&&dim.back().first==37);lcars::panel.living_result(dim.back().second,true);
  refresh();assert(sounds==quiet_count); // Slider and incoming state updates are quiet.
  click("< BACK");open_room("KITCHEN");snapshot("kitchen");assert(!find(root,"HALLWAY"));
  quiet_count=sounds;
  lv_obj_send_event(lv_obj_get_parent(find(root,"ALL ON")),LV_EVENT_CLICKED,nullptr);
  assert(power.size()==1&&power[0].first==1&&power[0].second);
  assert(sounds==quiet_count); // Audio busy never blocks a lighting command.
  lcars::panel.update(1,"on");
  click("< BACK");open_room("DINING TABLE");snapshot("dining");
  click("ALL OFF");assert(power.size()==3&&power[1].first==4&&!power[1].second&&power[2].first==6&&!power[2].second);
  lcars::panel.update(4,"off");lcars::panel.update(6,"off");
  click("< BACK");open_room("ENTRY HALL");snapshot("entry");
  click("HUE ENTRY HALL");assert(power.back().first==7&&power.back().second);
  click("< BACK");click("SYSTEM");snapshot("system");
  click("DIM");click("NORMAL");click("BRIGHT");
  click("ROTATE 180");assert(rotations.size()==1&&rotations.back());
  assert(find(root,"ORIENTATION: 180 DEG"));
  click("ROTATE 180");assert(rotations.size()==2&&!rotations.back());
  assert(find(root,"ORIENTATION: 0 DEG"));
  auto hail_before=sounds;
  auto *sound_button=click("HAIL");assert(sounds==hail_before+1&&find(root,"PLAYING..."));
  lv_obj_send_event(sound_button,LV_EVENT_CLICKED,nullptr);assert(sounds==hail_before+1);
  lcars::panel.sound_status(false,true);assert(find(root,"AUDIO ERROR"));
  lcars::panel.connection(false,false);click("HAIL");assert(sounds==hail_before+2);
  lcars::panel.sound_status(false);assert(find(root,"SPEAKER TEST"));snapshot("1.4-system");
  lv_area_t version_area, system_area;
  auto *version_label=find(root,"FNK0115Q / LCARS 1.4");assert(version_label);
  lv_obj_get_coords(version_label,&version_area);
  lv_obj_get_coords(lv_obj_get_parent(version_label),&system_area);
  assert(version_area.y2<=system_area.y2);
  lv_area_t rotate_area, sound_label_area;
  lv_obj_get_coords(lv_obj_get_parent(find(root,"ROTATE 180")),&rotate_area);
  lv_obj_get_coords(find(root,"SPEAKER TEST"),&sound_label_area);
  assert(sound_label_area.y1>rotate_area.y2);
  assert(find(root,"VOLUME: 60%"));click("+ VOL");assert(find(root,"VOLUME: 70%"));
  for(int i=0;i<8;++i)click("+ VOL");
  assert(volumes.size()==4&&volumes.back()==1.0f&&find(root,"VOLUME: 100%"));
  for(int i=0;i<12;++i)click("- VOL");
  assert(volumes.size()==14&&volumes.back()==0.0f&&find(root,"VOLUME: MUTED"));
  for(int i=0;i<6;++i)click("+ VOL");snapshot("1.4-system");
  click("< BACK");
  lcars::panel.connection(false,false);open_room("DINING TABLE");snapshot("offline");
  assert(lv_obj_has_state(lv_obj_get_parent(find(root,"ALL ON")),LV_STATE_DISABLED));
  // Test both sides of every main tile, including when another submenu was
  // last opened. A toggle must use its own room, never the last selected room.
  click("< BACK");lcars::panel.connection(true,true);
  for(size_t i=0;i<lcars::CONTROL_COUNT;++i)lcars::panel.update(i,"off");
  power.clear();
  for(const auto &room:lcars::ROOMS) {
    auto before=power.size();auto *toggle=click(room.name);
    assert(power.size()==before+room.count&&find(root,"KITCHEN"));
    for(size_t n=0;n<room.count;++n)assert(power[before+n]==std::make_pair(room.members[n],true));
    // Bypassing LVGL's disabled guard still must not send duplicate actions.
    lcars::panel.sound_status(false);quiet_count=sounds;
    lv_obj_send_event(toggle,LV_EVENT_CLICKED,nullptr);assert(power.size()==before+room.count);
    assert(sounds==quiet_count); // Disabled pending buttons stay quiet even after audio finishes.
    open_room(room.name);assert(power.size()==before+room.count);click("< BACK");
    for(size_t n=0;n<room.count;++n)lcars::panel.update(room.members[n],"on");
    before=power.size();click(room.name);assert(power.size()==before+room.count);
    for(size_t n=0;n<room.count;++n){assert(power[before+n]==std::make_pair(room.members[n],false));lcars::panel.update(room.members[n],"off");}
  }
  lcars::panel.update(4,"on");lcars::panel.update(6,"on");
  auto before=power.size();click("DINING TABLE");
  assert(power.size()==before+2&&power[before]==std::make_pair(size_t(4),false)&&power[before+1]==std::make_pair(size_t(6),false));
  lcars::panel.update(4,"off");lcars::panel.update(6,"off");
  lcars::panel.update(5,"unavailable");
  auto *disabled=lv_obj_get_parent(find(root,"DINING TABLE"));assert(lv_obj_has_state(disabled,LV_STATE_DISABLED));
  lcars::panel.sound_status(false);quiet_count=sounds;
  before=power.size();lv_obj_send_event(disabled,LV_EVENT_CLICKED,nullptr);assert(power.size()==before);
  assert(sounds==quiet_count);
  open_room("DINING TABLE");assert(power.size()==before);click("< BACK");
  lcars::panel.connection(false,false);
  disabled=lv_obj_get_parent(find(root,"LIVING ROOM"));assert(lv_obj_has_state(disabled,LV_STATE_DISABLED));
  lv_obj_send_event(disabled,LV_EVENT_CLICKED,nullptr);assert(power.size()==before);
  open_room("LIVING ROOM");assert(power.size()==before);click("< BACK");snapshot("split-offline");
  click("TRANSIT");assert(find(root,"ABFAHRTEN"));
  click("LIGHTS");assert(find(root,"KITCHEN"));
  std::cout<<"LVGL UI tests passed: button hail feedback, rapid taps, disabled-button silence, quiet state updates and sliders, split toggles for all rooms, separate menus, mixed states, duplicate prevention, room navigation, Back, scene paging, action routing, brightness, offline guards, layout bounds\n";
}
