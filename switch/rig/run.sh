#!/bin/sh
# run.sh <nro> <label> <secs> [args...]: close the running title, hbmenu takeover, netloader, send the .nro with
# nxlink (installed as sdmc:/switch/ra2engine/ra2engine.nro) and keep its stdout/stderr for <secs> seconds.
# Output in <RA2_LOGS>/<label>/: nxlink.log (the engine's log) and shot_<t>.jpg every 10 s plus one at the end.
# The game is left running; the next run closes it. READY=1 skips the close/takeover when the netloader is open.
R=$(cd "$(dirname "$0")" && pwd); . "$R/common.sh"
nro=$1; label=$2; secs=$3; shift 3
d=$L/$label; mkdir -p "$d"
ftp mkdir "$SD_DIR"   # the netloader does not create the folder (open: err=2)
if [ -z "$READY" ]; then   # READY=1: hbmenu's netloader is already open
  close_title
  open_netloader
fi
send() { timeout "$((secs + 15))" stdbuf -o0 -e0 "$NXL" -s -a "$SWITCH_IP" -p "${SD_DIR#/switch/}/ra2engine.nro" "$nro" ${1:+--args "$*"} > "$d/nxlink.log" 2>&1 & }
send "$@"; pid=$!
sleep 8
if ! grep -q "server active" "$d/nxlink.log" && grep -q -i "failed\|refused" "$d/nxlink.log"; then   # nxlink's, not the engine's   # netloader not ready (seen after a reboot): B, Y, retry
  wait $pid 2>/dev/null; sb "click B" "sleep 2" "click Y" "sleep 4"; send "$@"; pid=$!
fi
t=0
while [ $t -lt "$secs" ]; do
  sleep 10; t=$((t + 10))
  sb shot "$d/shot_$t.jpg" 2>/dev/null
done
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "$d"; ls "$d"; tail -5 "$d/nxlink.log"
