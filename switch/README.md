# Nintendo Switch port (homebrew .nro)

Native libnx build of the engine through devkitPro's `switch-sdl2`. It needs a
Switch that runs homebrew (Atmosphère + hbmenu) and your own Red Alert 2 /
Yuri's Revenge game files.

## Build

```sh
# once: toolchain image with the unified Mesa 26 SDK (NVK/zink)
docker build -t ra2-switch-dkp -f switch/Dockerfile /path/to/mesa-drivers
switch/build.sh                 # -> build-switch/ra2engine.nro

# fallback: stock switch-mesa 20.1 from the plain devkitPro image
RA2_DKP_IMAGE=devkitpro/devkita64:latest switch/build.sh   # -> build-switch-stock/ra2engine.nro
```

`switch/switch.cmake` picks the Mesa link line by itself (the Mesa 26 `.pc`
files omit its NVK/NAK/NIL archives, so they are listed explicitly).

## Install

```
sdmc:/switch/ra2engine/ra2engine.nro
sdmc:/switch/ra2engine/ra2.mix, ra2md.mix, language.mix, multimd.mix, ...   (every .mix + .csf)
sdmc:/switch/ra2engine/save/        created on first run: RA2.INI, saves, ra2engine.log
```

Launch through **title takeover** (hold R while starting a game, then pick
RA2 Engine). Started from the album, the homebrew only gets a few hundred MB.
The log warns when it runs in applet mode.

## Controls

| Input | Action |
|---|---|
| Left stick | Pan the map (quadratic; about twice as fast with the stick on its rim) |
| Right stick | Cursor (quadratic) |
| A | Select / order (move, attack, harvest, enter...). Twice: every visible unit of that type |
| B | Cancel / deselect; cancels building placement |
| Y | Hold and move the cursor: selection box. Tap: every combat unit |
| X | Deploy / unload / special ability. Over the sidebar: hold or cancel the cameo's production |
| R | Tap: cursor jumps between the battlefield and the build strip (and back to where it was) |
| R + D-pad ↑ → ↓ ← | Select team 1–4; with ZR held too: make the selection team 1–4 |
| ZR + A | Force fire (ground, friendlies) |
| ZL + A | Attack-move. ZL tapped alone: guard |
| L | Tap: scatter. Hold: stop |
| L3 | Jump to the last radar event. Twice: the MCV / Construction Yard |
| R3 | Center on the selection |
| D-pad ↑ / ↓ | Repair / sell mode |
| D-pad ← / → | Previous / next production tab (Structures, Defenses, Infantry, Vehicles) |
| − | Mission objectives (campaign) / pause menu (skirmish) |
| + | Options / save / load menu |
| Touch screen (handheld) | The touch controls (tap, drag, CMD column); the controller's cursor hides them again |

The engine has no escort command (Ctrl+Alt) and no waypoint mode, so ZL + A is attack-move.

## Hardware status (2026-10-30, Atmosphère, hbmenu title takeover)

| | Stock Mesa 20.1 | Mesa 26.2.4 (NVK/zink SDK) |
|---|---|---|
| Boot, menus, Bink menu video, music | yes | yes |
| Skirmish (menus → lobby → match), 1280x720 | 60 fps locked; update 1.4 ms, render 3.2 ms | 60 fps; "slow EGL config" |
| Combat, AI, deploy, build, place | yes | yes |
| Load time, default 1v1 | 11.2 s (was 22.5 s) | 11.7 s |
| First frame of a match | 0.35 s (was 7.9 s) | 0.32 s |
| Campaign: intro movie, briefing, Allied mission 1 | 60 fps (was 45–50); load 12.4 s | not run |
| Campaign: Soviet mission 1 (Washington, 155x155) | 55–57 fps; load 9.0 s (was 13.6 s) | not run |
| HOME → Close | clean | clean once exit is locked (see below) |

Switch-only optimizations (each behind `__SWITCH__`):

- **Terrain atlas** (`engine/iso_renderer.cpp`): creating a texture costs ~2.2 ms in the Switch's GL driver, and a
  map's terrain needs ~3300 of them (7.3 s in the first frame). The tiles and their cliff extras are packed into
  2048x2048 pages instead. It also trims steady render time.
- **Vehicle loading** (`load_vehicle_assets`): `rulesmd.ini` / `artmd.ini` are parsed once instead of once per
  vehicle (7.3 s), and each vehicle's facing sets are built on cores 0–2 (`ra2_switch_parallel`).
- **Vehicle facings**: each of the 32 facings is rasterized once and coloured for both house palettes
  (`build_facing_sets`; only the palette lookup differed), as (part x half) jobs on three cores: 4.1 s → 2.1 s for
  Soviet 1's 65 vehicles. The death explosions (conquer.mix / cache.mix, the SHPs, anim.pal) are read once per data
  dir instead of per vehicle (0.64 s).
- **Voxel prewarm** (per house colour, after loading) runs on cores 1–2: 11 s → 5.7 s.
- **Building anims** are read on the loading screen instead of in the first game tick (a 390 ms hitch).
- **Shroud with allied sight** (`World::step_visibility`, all platforms, same result): campaign maps share sight
  between allied houses, and the merge copied and OR-ed every house's grid byte by byte each tick (3.1 ms of a
  5.4 ms world step on Allied 1). It now reuses the snapshot buffers and ORs a word at a time (0.3 ms). Buildings
  reveal their footprint as one rounded rectangle (`reveal_rect`) instead of one disc per footprint cell, and that
  building sight is cached until a building appears, dies, moves or changes house.
- The `[perf]` log (`--fps`) also splits the logic tick (`tick campaign/world/pres/rest`) and `World::step`
  (`world units/vis/...`) and names frames over 100 ms (`slow frame`).
- Horizon threads stay on their creator's core unless pinned: `ra2_switch_pin_thread` /
  `ra2_switch_parallel` in `switch_platform.cpp`.

Two Mesa 26 problems, both worked around in `switch_platform.cpp`:

- Killed mid-frame by HOME → Close, it left the GPU wedged: a black screen, home menu included, until a reboot.
  `appletLockExit()` turns the close into an exit request, so the engine shuts down normally (under 2 s, also
  mid-match). Both builds lock exit.
- With Mesa's GL thread on, a skirmish crashed after 1–2 minutes (`_mesa_glthread_upload`, null dereference) after
  `nouveau/switch: durable GPU submission error latched: -22 ... bufctx`. `mesa_glthread=false` is set at startup.

The stock build is the safer default.

## Test rig (`switch/rig/`)

Drives a LAN Switch with sys-botbase (buttons, sticks, touch, screenshots), an FTP server on the SD and hbmenu's
netloader. Copy `rig.env.example` to `rig.env` (Git ignores it) and fill it in.

```sh
switch/rig/put_data.py "/path/to/Red Alert 2"            # game files -> sdmc:/switch/ra2engine (skips same-size files)
switch/rig/run.sh build-switch-stock/ra2engine.nro t1 120 --quick-skirmish --fps
switch/rig/step.sh shot1 "touch 1000 328" "sleep 2"      # sys-botbase commands, then a screenshot
```

`run.sh` closes the running title, opens hbmenu through a title takeover, sends the `.nro` with `nxlink -s` and keeps
the engine's stdout/stderr (`<RA2_LOGS>/<label>/nxlink.log`) plus a screenshot every 10 s. `READY=1` skips the
takeover when the netloader is already open. Under nxlink the controller is logged (`[pad]` lines).

## Bug reports

Send `sdmc:/switch/ra2engine/save/ra2engine.log`. Say which build you ran
(Mesa 26 or stock) and whether the console was docked or handheld.
