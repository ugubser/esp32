#pragma once
#include <array>
#include <cstddef>

namespace lcars {
struct Entity { const char *name; const char *id; bool is_switch; };
inline constexpr std::array<Entity, 9> ENTITIES{{
    {"LIVING ROOM", "light.living", false},
    {"KITCHEN LIGHT", "switch.xiaomi_power_switch_kitchen_switch", true},
    {"COOKING AREA", "switch.xiaomi_power_switch_cooking_switch", true},
    {"HALLWAY", "switch.xiaomi_power_switch_hallway_switch", true},
    {"HUE SIDE TABLE", "light.signify_netherlands_b_v_lct007_light", false},
    {"XIAOMI SIDE TABLE", "light.lumi_lumi_light_aqcn02_light_2", false},
    {"IKEA TABLE LAMP", "light.ikea_matter", false},
    {"HUE ENTRY HALL", "light.entry_hall", false},
    {"HUE RETRO BULB", "light.signify_netherlands_b_v_lwv001_light", false},
}};
constexpr size_t CONTROL_COUNT = ENTITIES.size();
struct Room { const char *name; std::array<size_t, 3> members; size_t count; };
inline constexpr std::array<Room, 5> ROOMS{{
    {"LIVING ROOM", {0, 0, 0}, 1}, {"KITCHEN", {1, 2, 0}, 2},
    {"HALLWAY", {3, 0, 0}, 1}, {"DINING TABLE", {4, 5, 6}, 3},
    {"ENTRY HALL", {7, 8, 0}, 2},
}};
struct Scene { const char *name; const char *id; };
inline constexpr std::array<Scene, 18> SCENES{{
    {"BRIGHT", "scene.living_bright"}, {"DIMMED", "scene.living_dimmed"},
    {"NIGHTLIGHT", "scene.living_nightlight"}, {"READ", "scene.living_read"},
    {"CONCENTRATE", "scene.living_concentrate"}, {"ENERGIZE", "scene.living_energize"},
    {"RELAX", "scene.living_relax"}, {"SAVANNA SUNSET", "scene.living_savanna_sunset"},
    {"TROPICAL TWILIGHT", "scene.living_tropical_twilight"},
    {"ARCTIC AURORA", "scene.living_arctic_aurora"},
    {"SPRING BLOSSOM", "scene.living_spring_blossom"}, {"RAINBOW", "scene.living_rainbow"},
    {"HARRY", "scene.living_harry"}, {"BLU", "scene.living_blu"},
    {"LIVING ROOM OFF", "scene.all_lights"}, {"50% BRIGHT", "scene.living_room_50"},
    {"75% BRIGHT", "scene.living_room_75_bright"},
    {"100% BRIGHT", "scene.living_room_75_bright_2"},
}};
constexpr size_t SCENES_PER_PAGE = 6;
constexpr size_t SCENE_PAGES = (SCENES.size() + SCENES_PER_PAGE - 1) / SCENES_PER_PAGE;
}  // namespace lcars
