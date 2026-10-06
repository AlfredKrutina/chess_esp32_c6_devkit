import json
import struct
from collections import Counter
from pathlib import Path

p = Path(__file__).resolve().parent / "board" / "czechmate_v3.glb"
data = p.read_bytes()
chunk_len = struct.unpack_from("<I", data, 12)[0]
js = json.loads(data[20 : 20 + chunk_len])
bin_off = 20 + chunk_len
clen = struct.unpack_from("<I", data, bin_off)[0]
blob = data[bin_off + 8 : bin_off + 8 + clen]

pad = next(m for m in js["meshes"] if m["name"] == "czechmate_pad")
acc = js["accessors"][pad["primitives"][0]["attributes"]["POSITION"]]
bv = js["bufferViews"][acc["bufferView"]]
off = (bv.get("byteOffset") or 0) + (acc.get("byteOffset") or 0)
n = acc["count"]
xs, ys = [], []
for i in range(n):
    x, y, z = struct.unpack_from("<3f", blob, off + i * 12)
    xs.append(x)
    ys.append(y)

def strip(vals, lo, hi):
    return sum(1 for v in vals if lo <= v <= hi)

print("pad x near -0.07", strip(xs, -0.07, -0.055), "near +0.07", strip(xs, 0.055, 0.07))
print("pad y near -0.07", strip(ys, -0.07, -0.055), "near +0.07", strip(ys, 0.055, 0.07))
print("pad x>0.06", strip(xs, 0.06, 0.08), "x<-0.06", strip(xs, -0.08, -0.06))
