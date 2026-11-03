#!/usr/bin/env bash
# Package a built .nro as an SD-card zip (no game files).
#   switch/package.sh [build-switch-stock|build-switch]   -> dist/ra2engine-switch-<version>[-mesa26].zip
# The zip unpacks onto the SD root: switch/ra2engine/ra2engine.nro + INSTALL.txt + LICENSE.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-build-switch-stock}"
nro="$root/$build/ra2engine.nro"
[ -f "$nro" ] || { echo "no $nro (run switch/build.sh first)" >&2; exit 1; }
version="$(git -C "$root" describe --tags --always 2>/dev/null || cat "$root/VERSION")"
version="${version#switch-}"   # tag switch-v1.0.0 -> ra2engine-switch-v1.0.0.zip
suffix=""
[ "$build" = build-switch ] && suffix="-mesa26"
name="ra2engine-switch-${version}${suffix}"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/switch/ra2engine"
cp "$nro" "$stage/switch/ra2engine/ra2engine.nro"
cp "$root/LICENSE" "$stage/switch/ra2engine/LICENSE"
cp "$root/switch/keymap.keys.example" "$root/switch/switch.ini.example" "$stage/switch/ra2engine/"
cat > "$stage/switch/ra2engine/INSTALL.txt" <<EOF
RA2 Engine for Nintendo Switch ($version${suffix:+, Mesa 26 build})

1. Unzip onto the root of the SD card (this creates switch/ra2engine/).
2. Copy your own Red Alert 2 / Yuri's Revenge files into sdmc:/switch/ra2engine/:
   every .mix, .mmx and .yro file from the game folder (ra2.mix, ra2md.mix,
   language.mix, langmd.mix, multi.mix, multimd.mix, theme.mix, ...).
   The movies (movies01.mix, movies02.mix, movmd03.mix) are optional.
3. Start it through title takeover: hold R while launching any game, then pick
   RA2 Engine in the homebrew menu. From the album it only gets a few hundred MB.

Optional: keymap.keys.example (controller remapping) and switch.ini.example
(bigger sidebar / UI) explain themselves; rename them without ".example" to use them.
Keyboard hotkeys: in game, Options > Game Controls > Keyboard.

Settings and saves go to sdmc:/switch/ra2engine/save/ (RA2.INI, Saved Games/),
and the log to save/ra2engine.log. Controls: see switch/README.md in the source.
EOF
mkdir -p "$root/dist"
rm -f "$root/dist/$name.zip"
(cd "$stage" && zip -qr "$root/dist/$name.zip" switch)
echo "$root/dist/$name.zip"
