#!/bin/sh
# cycles.sh <label> <n> <play secs>: with the engine on its main menu (run.sh <nro> <label> ... --fps in the
# background), start the default skirmish from the menus, play <secs>, abort through Plus > Abort Mission, n times.
# Prints each load time and the [perf] heap figure after every cycle.
R=$(cd "$(dirname "$0")" && pwd); . "$R/common.sh"
log=$L/$1/nxlink.log; n=$2; secs=$3
i=0
while [ $i -lt "$n" ]; do
  i=$((i + 1))
  before=$(grep -c "LOADING done in" "$log")
  sb "touch 1000 328" "sleep 2.5" "touch 1000 455" "sleep 3.5" "touch 1000 392"   # Single Player, Skirmish, Start
  until [ "$(grep -c "LOADING done in" "$log")" -gt "$before" ]; do sleep 2; done
  sleep "$secs"
  sb shot "$L/$1/cycle_$i.jpg"
  sb "click PLUS" "sleep 1.5" "touch 1000 580" "sleep 1.5" "touch 1000 328" "sleep 5"   # Options, Abort Mission, Abort
  echo "cycle $i: $(grep "LOADING done in" "$log" | tail -1 | grep -o 'done in [0-9]* ms') $(grep 'mem used' "$log" | tail -1 | grep -o 'used=.*')"
done
