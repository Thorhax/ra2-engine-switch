#!/usr/bin/env python3
"""put_data.py <RA2 install folder> [--movies]: upload the game's .mix / .csf / .mmx / .yro files (movie archives only
with --movies) to /switch/ra2engine on the SD, skipping files already there at the same size."""
import ftplib, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rigenv import E
src, movies = sys.argv[1], "--movies" in sys.argv
DST = "/switch/ra2engine"
f = ftplib.FTP(); f.connect(E["SWITCH_IP"], int(E.get("FTP_PORT", 5000)), timeout=60); f.login(E["FTP_USER"], E["FTP_PASS"])
have = {}
lines = []
f.retrlines("LIST " + DST, lines.append)
for l in lines:
    p = l.split(None, 8)
    if len(p) == 9 and not l.startswith("d"): have[p[8]] = int(p[4])
names = sorted(n for n in os.listdir(src) if n.lower().endswith((".mix", ".csf", ".mmx", ".yro"))
               and (movies or not n.lower().startswith(("movies", "movmd"))))
for n in names:
    size = os.path.getsize(os.path.join(src, n))
    if have.get(n) == size:
        print(f"skip {n}", flush=True); continue
    t = time.time()
    with open(os.path.join(src, n), "rb") as i: f.storbinary(f"STOR {DST}/{n}", i, blocksize=1 << 20)
    dt = time.time() - t
    print(f"put {n} {size / 1e6:.1f} MB {size / 1e6 / max(dt, 0.001):.1f} MB/s", flush=True)
f.quit()
print("done", flush=True)
