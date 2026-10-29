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
