# Nintendo Switch port (homebrew .nro)

Native libnx build of the engine through devkitPro's `switch-sdl2`. It needs a
Switch that runs homebrew (Atmosphère + hbmenu) and your own Red Alert 2 /
Yuri's Revenge game files.

## Quick start (release zip)

1. Download `ra2engine-switch-<version>.zip` from the
   [Releases](https://github.com/Thorhax/ra2-engine-switch/releases) page (the `-mesa26` zip is an experimental
   build on a newer GPU driver; the plain one is recommended).
2. Unzip it onto the root of the SD card. That creates `sdmc:/switch/ra2engine/ra2engine.nro`.
3. Copy every `.mix` and `.csf` file from your own Red Alert 2 / Yuri's Revenge install folder into
   `sdmc:/switch/ra2engine/` (about 1 GB; the movie mixes are optional). No game data is included.
4. Hold **R** while launching any game, then pick **RA2 Engine** in the homebrew menu (title takeover, needed for the
   memory; see [Install](#install)).

## What works

- The Allied and Soviet campaigns (movies, briefings, objectives), skirmish against the AI, save / load.
- 1280x720, docked and handheld, at 60 fps in skirmish and most missions (see [Hardware status](#hardware-status)).
- A full controller scheme ([Controls](#controls)) on the Joy-Cons or a Pro Controller, with rumble when your base
  is under attack.
- Touch in handheld mode: the engine's touch controls (tap, drag-select, the CMD column) appear on the first touch
  and hide again when the controller's cursor moves.
- Readable menu and HUD text in DejaVu Sans ([Text](#text)).
- Controller remapping and a right-stick cursor ([`keymap.keys`](#remapping-keymapkeys)), a USB keyboard and
  mouse ([Keyboard and mouse](#keyboard-and-mouse)), and a bigger sidebar / UI ([`switch.ini`](#bigger-sidebar-switchini)).
- A clean exit from HOME → Close, also mid-match. Settings, saves and the log go to `sdmc:/switch/ra2engine/save/`.

Not tested on the Switch yet: LAN multiplayer.

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

## Package

```sh
switch/package.sh build-switch-stock   # -> dist/ra2engine-switch-<git describe>.zip (Mesa 26: build-switch, -mesa26)
```

The zip unpacks onto the SD root (`switch/ra2engine/ra2engine.nro`, `INSTALL.txt`, `LICENSE`) and holds no game
files. `switch/icon.jpg` is the hbmenu icon.

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
| Left stick | Cursor (quadratic; Options → Scroll Rate sets its speed) |
| Right stick | Pan the map (quadratic; about twice as fast with the stick on its rim) |
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
| Touch screen (handheld) | The touch controls (tap, drag, CMD column in the top-left corner); the controller's cursor hides them again |
| One-finger drag | Selection box (the BOX button in the touch column turns it off: then one finger scrolls) |
| Two-finger drag | Scroll the map (Options → Scroll Rate sets the speed, as for the sticks) |
| Three-finger tap | Select every combat unit (P) |
| Rumble | A short pulse when the base is under attack |

The left stick moves the cursor and the right stick pans the map (swapped from the first builds). The engine has no escort command (Ctrl+Alt) and no waypoint mode, so ZL + A is attack-move.

## Remapping (`keymap.keys`)

`sdmc:/switch/ra2engine/keymap.keys` rebinds controller buttons to keys or mouse buttons
(`switch/keymap.keys.example`, also in the release zip, explains every option):

```
RSTICK_MOUSE=TRUE   # right stick = cursor, left stick = map scroll
DPAD_ARROWS=TRUE    # D-pad scrolls the map
ZR=MOUSE_LEFT
ZL=MOUSE_RIGHT
L=CTRL              # held: Ctrl for the other buttons (L + Y = Ctrl+1, make team 1)
Y=1
R=h                 # H: center on the base
B=CTRL+2            # combos work too
PLUS=ESC
```

Buttons: `A B X Y L R ZL ZR PLUS MINUS LSTICK RSTICK DUP DDOWN DLEFT DRIGHT`. A value is `MOUSE_LEFT`,
`MOUSE_RIGHT`, a key (`h`, `2`, `ESC`, `ENTER`, `SPACE`, `HOME`, `F1`...), a combo (`CTRL+1`, `ALT+1`), a held
modifier (`CTRL`, `SHIFT`, `ALT`, `RIGHT CTRL`...), `SIDEBAR` (tap: cursor to the build list and back, like R),
`SIDEBAR_HOLD` (held: the cursor sits on the build list; released: back on the map), `SIDEBAR_UP` / `SIDEBAR_DOWN`
(scroll the build list), `NONE` or `DEFAULT`. Buttons left out keep the built-in action.

Combos: `A+DRIGHT=1` runs while A is held; A alone still does its own action, on release. `CURSOR_SPEED` and
`MAP_SPEED` scale the sticks (Options → Scroll Rate scales them too, 1.25x a notch). A file saved as
`keymap.keys.txt` (Windows hides the extension) or `keymap-anything` is read too when there is no `keymap.keys`.
Keys act like the same key on a keyboard, so they follow the keyboard hotkeys below. The log names the file and
any line it skipped (`[keymap]`).

## Keyboard and mouse

A USB keyboard and mouse (in the dock, or an adapter in handheld) work like on PC. All of RA2's keyboard
hotkeys are there (team select / create / add / center with Shift, Ctrl and Alt, H = base, D = deploy, S = stop,
G = guard, X = scatter, Q W E R = tabs, ...); rebind them in game under Options → Game Controls → Keyboard, which
saves `save/Keyboard.ini`. Home also goes to the base while nothing else is bound to it. The mouse moves the same
cursor as the stick; the wheel over the sidebar scrolls the build list.

switch-sdl2 only reports a keyboard's Ctrl / Shift / Alt as key presses and orders a press's keys by scancode,
so Ctrl+1 arrived as a plain 1; the engine reads the HID modifier state instead. It has no mouse wheel events
either: the wheel is read from the HID mouse state (`switch_platform.cpp`).

## Bigger sidebar (`switch.ini`)

`UIScale` in `sdmc:/switch/ra2engine/switch.ini` (or `switch.ini.txt`) draws the game at a lower resolution, so the sidebar, build
icons, buttons, text and units all grow (the engine's phone canvas; like running the original at 1024x768):

```
UIScale=1.0            ; 1.0 = 1280x720 (default), 1.25 = 1024x576, 1.33 = 960x540, 1.5 = 853x480
UIScaleHandheld=1.25   ; optional per-mode values, read when a match starts
UIScaleDocked=1.0
```

At 1.5 the strip shows four rows of 90x72 icons; a skirmish stays at 60 fps.

## Text

Menu and HUD text is drawn in DejaVu Sans (`switch/romfs/fonts`, SDL2_ttf; `switch/switch_text.cpp`): the game's
8-px bitmap fonts, laid out for 640x480 and scaled 1.5x, blur together on the handheld screen. Each label takes the
largest size its box allows. An empty file `sdmc:/switch/ra2engine/classic_font` keeps the original fonts (drawn
with solid strokes and the HUD at a whole 2x scale).

## Hardware status

Tested 2026-10-30 to 2026-11-01 on Atmosphère through hbmenu title takeover: the timings below ran docked, and
handheld play (touch, controls, sound, text) was checked by hand.

| | Stock Mesa 20.1 | Mesa 26.2.4 (NVK/zink SDK) |
|---|---|---|
| Boot, menus, Bink menu video, music | yes | yes |
| Skirmish (menus → lobby → match), 1280x720 | 60 fps locked; update 1.4 ms, render 3.2 ms | 60 fps; "slow EGL config" |
| Combat, AI, deploy, build, place | yes | yes |
| Load time, default 1v1 | 11.2 s (was 22.5 s) | 11.7 s |
| First frame of a match | 0.35 s (was 7.9 s) | 0.32 s |
| Campaign: intro movie, briefing, Allied mission 1 | 60 fps (was 45–50); load 12.4 s | not run |
| Campaign: Soviet mission 1 (Washington, 155x155) | 55–57 fps; load 9.0 s (was 13.6 s) | not run |
| Campaign: Allied mission 7 (`--campaign all07smd.map`, 270x270, 16 houses) | 60 fps at 1x speed; load 10.9 s; world step 4.3 ms (was 8.1) | not run |
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
- **Voxel prewarm** (per house colour, after loading) runs on cores 1–2, one job per vehicle that rasterizes each
  facing once for every colour in the match: 11 s → 2.0 s.
- **Campaign voxel prewarm**: a mission configures its houses after the scene's prewarm call, so the prewarm found
  no houses and every vehicle's first draw in a house colour rasterized its set on the spot (Dreadnought 241 ms,
  a truck 204 ms). It starts again after the mission setup and also builds the hull shadows. Allied 1's first
  frame: 586 ms → 242 ms (its 105 new textures); the later 170–220 ms hitches are gone.
- **Building anims** are read on the loading screen instead of in the first game tick (a 390 ms hitch).
- **Shroud with allied sight** (`World::step_visibility`, all platforms, same result): campaign maps share sight
  between allied houses, and the merge copied and OR-ed every house's grid byte by byte each tick (3.1 ms of a
  5.4 ms world step on Allied 1). It now reuses the snapshot buffers and ORs a word at a time (0.3 ms). Buildings
  reveal their footprint as one rounded rectangle (`reveal_rect`) instead of one disc per footprint cell, and that
  building sight is cached until a building appears, dies, moves or changes house. The merge only copies and ORs the rows
  each house's buildings and units can see (Allied 7, 68 allied pairs on a 270x270 map: 4.9 ms → 1.1 ms).
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
