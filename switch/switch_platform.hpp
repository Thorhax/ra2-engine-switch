#pragma once
// Nintendo Switch platform glue. switch-sdl2 builds SDL's dummy filesystem
// backend (SDL_GetBasePath / SDL_GetPrefPath return NULL), so main.cpp maps
// both onto these. Both return SDL_malloc'd strings with a trailing '/'.
//
// SD card layout:
//   sdmc:/switch/ra2engine/ra2engine.nro
//   sdmc:/switch/ra2engine/*.mix, *.csf   game files (the data dir)
//   sdmc:/switch/ra2engine/save/          RA2.INI, saves, ra2engine.log
#define RA2_SWITCH_BASE_DIR "sdmc:/switch/ra2engine/"

char* ra2_switch_base_path();
char* ra2_switch_pref_path();

#include <functional>
#include <vector>

// A Horizon thread runs on its creator's default core unless told otherwise,
// so plain std::threads share core 0 with the main thread. These pin the
// calling thread to an application core (0..2; 3 belongs to the system).
void ra2_switch_pin_thread(int core);
// SDL KMOD_* bits for the USB keyboard's Ctrl / Shift / Alt (from the HID modifier field).
int ra2_switch_kbd_mods();
// Mouse wheel notches since the last call (+ = away from the user).
int ra2_switch_mouse_wheel();
// The game canvas height for switch.ini's UIScale (UIScaleHandheld / UIScaleDocked for the
// current mode; 1.0-1.5, default 1.0): 720 / scale, so the sidebar, cameos and text grow by it.
int ra2_switch_game_height();
// Options > Scroll Rate (0 fastest .. 6 slowest), set by the game each frame; the stick speeds follow it.
void ra2_switch_set_scroll_rate(int rate);
int ra2_switch_scroll_rate();
// A short rumble on every open controller (0..1 strength).
void ra2_switch_rumble(float strength, int ms);
// Heap in use / the process's memory, in bytes (the [perf] log line).
unsigned long long ra2_switch_mem_used();
unsigned long long ra2_switch_mem_total();
// Runs `jobs` on the caller plus one pinned worker thread per entry of
// `worker_cores` and returns when all are done. The jobs must be independent.
void ra2_switch_parallel(const std::vector<std::function<void()>>& jobs,
                         const std::vector<int>& worker_cores = {1, 2});
// World::step's stages for the [perf] log (summed per log window; world.cpp).
extern double g_sw_world_ms[10];
