# common.sh: sourced by the rig scripts after they set R to their own directory. Reads rig.env (rig.env.example).
[ -f "$R/rig.env" ] || { echo "rig: copy switch/rig/rig.env.example to switch/rig/rig.env and fill it in" >&2; exit 1; }
. "$R/rig.env"
P=$(cd "$R/../.." && pwd)
L=${RA2_LOGS:-$(cd "$P/.." && pwd)/ra2-logs}
T=${RIG_TMP:-$L/tmp}
NXL=${NXLINK:-nxlink}
SD_DIR=/switch/ra2engine
mkdir -p "$L" "$T"

sb() { python3 "$R/sb.py" "$@"; }
ftp() { python3 "$R/ftp.py" "$@"; }
title() { timeout 20 python3 "$R/sb.py" title 2>/dev/null; }
# Closes the running title (HOME, X, A) and checks the console still answers. Skipped on an idle home menu (no title),
# where that A would launch the selected title instead.
close_title() {
  [ -n "$(title)" ] || return 0
  sb "click HOME" "sleep 1.5" "click X" "sleep 1" "click A" "sleep 4"
  if ! title > /dev/null; then
    echo "$(date '+%F %T') console not answering after closing the title ($0)" | tee -a "$L/console_lost.log" >&2
    exit 2
  fi
}
# open_hbmenu: hbmenu through a title takeover (R held while launching the selected title; the second A picks the
# user), then its netloader (Y).
open_netloader() {
  sb "press R" "sleep 0.5" "click A" "sleep 2.5" "click A" "sleep 5" "release R" "sleep 2" "click Y" "sleep 3"
}
