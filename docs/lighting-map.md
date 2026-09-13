# Lighting expansion: verified Home Assistant inventory

Read-only discovery on 2026-09-12 against the owner's Home Assistant instance.
Implemented in lighting firmware 1.1.0; version 1.2.0 adds split main-menu
buttons for every room: left toggles power, right opens the submenu.

## Living Room

`light.living` is the Hue Living room group, with brightness support and nine
member lights. The Hue scenes below belong to that same group's device and
Living area. A separate `light.gledopto_livingroom_light` is assigned to the
Living area but is not a member of this Hue group; preserve the requested
existing group scope.

| Label | Entity |
| --- | --- |
| Arctic aurora | `scene.living_arctic_aurora` |
| blu | `scene.living_blu` |
| Bright | `scene.living_bright` |
| Concentrate | `scene.living_concentrate` |
| Dimmed | `scene.living_dimmed` |
| Energize | `scene.living_energize` |
| Harry | `scene.living_harry` |
| Nightlight | `scene.living_nightlight` |
| Rainbow | `scene.living_rainbow` |
| Read | `scene.living_read` |
| Relax | `scene.living_relax` |
| Savanna sunset | `scene.living_savanna_sunset` |
| Spring blossom | `scene.living_spring_blossom` |
| Tropical twilight | `scene.living_tropical_twilight` |
| Living Room off | `scene.all_lights` |
| Living Room 50% Bright | `scene.living_room_50` |
| Living Room 75% Bright | `scene.living_room_75_bright` |
| Living Room 100% Bright | `scene.living_room_75_bright_2` |

The four Home Assistant scene configurations were read and verified to target
only `light.living`. The scene named 50% actually stores brightness 107/255
(about 42%); the other brightness presets store 191 and 255. Do not silently
rewrite the existing scene. Scene timestamps/unknown states are not on/off
states and must not be used as scene availability or active-scene indicators.

Living Room's right-hand MENU button opens its submenu, with all scenes, group
brightness and a clearly visible Back button to the main lights page. The 18
scene choices occupy three pages. Its left-hand button toggles group power.

## Entry Hall

Both entities exist, currently report normal on/off states, and support dimming:

- `light.entry_hall` — Hue entry hall
- `light.signify_netherlands_b_v_lwv001_light` — Hue retro bulb

Existing `scene.entry_on` and `scene.entry_off` target exactly this pair.

## Dining Table

All three exist, belong to the Dinning area and support dimming:

- `light.signify_netherlands_b_v_lct007_light` — Hue dinner side table
- `light.lumi_lumi_light_aqcn02_light_2` — Xiaomi dinner side table
- `light.ikea_matter` — IKEA dinner table lamp

`light.dinning_area` is an unavailable old Hue group, so it does not implement
the requested three-lamp selection. Some old dining scenes target obsolete IDs.

## Kitchen and Hallway

The current registered Kitchen devices are:

- `switch.xiaomi_power_switch_kitchen_switch` — Kitchen
- `switch.xiaomi_power_switch_cooking_switch` — Cooking area

The similarly named unavailable `light.lumi_lumi_switch_n0agl1_light` and
`light.lumi_lumi_switch_n0agl1_light_2` entries share the same respective device
IDs. They are not additional lamps. The working switches offer on/off only.

The user clarified that Kitchen contains just these two circuits.
`switch.xiaomi_power_switch_hallway_switch` remains in its own Hallway submenu.

## Evidence

Ignored local snapshots: `logs/ha-light-scene-states.json`,
`logs/ha-light-scene-registry.json`, `logs/ha-living-scene-config-check.json`.
No lighting service calls were made during this discovery.
