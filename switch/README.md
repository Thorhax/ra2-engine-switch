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
| Left stick | Cursor (hold ZL for precision) |
| Right stick | Scroll the map (over the sidebar: the build list) |
| A / B | Left / right click (hold A + move = selection box) |
| L / R / ZR (hold) | Shift / Ctrl / Alt modifiers |
| D-pad ↑ → ↓ ← | Team 1–4 (R + D-pad: create team, L + D-pad: add to selection) |
| X / Y | Deploy / select same type |
| L3 / R3 | Stop / jump to last radar event |
| + / − | Options menu / center on base |
| Touch screen (handheld) | The Android touch controls |

## Hardware status (2026-10-29, Atmosphère, hbmenu title takeover)

| | Stock Mesa 20.1 | Mesa 26.2.4 (NVK/zink SDK) |
|---|---|---|
| Boot, menus, Bink menu video, music | yes | yes |
| Skirmish (menus → lobby → match), 1280x720 | 60 fps locked; update 1.4 ms, render 3.6 ms | ~59.5 fps; "slow EGL config" |
| Combat, AI, deploy, build, place | yes | yes (quick skirmish) |
| Load time, default 1v1 | 22.5 s | 22.9 s |
| HOME → Close | clean | clean once exit is locked (see below) |

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
