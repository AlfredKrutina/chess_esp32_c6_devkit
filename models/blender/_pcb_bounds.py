import json
import struct
from pathlib import Path

p = Path(__file__).resolve().parent / "board" / "czechmate_v3.glb"
data = p.read_bytes()
chunk_len, chunk_type = struct.unpack_from("<2I", data, 12)
js = json.loads(data[20 : 20 + chunk_len])
bin_off = 20 + chunk_len
clen, ctype = struct.unpack_from("<2I", data, bin_off)
blob = data[bin_off + 8 : bin_off + 8 + clen]


def acc_minmax(idx):
    acc = js["accessors"][idx]
    return acc.get("min"), acc.get("max"), acc["count"]


for mesh in js["meshes"]:
    prim = mesh["primitives"][0]
    pos = prim["attributes"]["POSITION"]
    mn, mx, n = acc_minmax(pos)
    print(f"{mesh['name']:24} n={n:7d} min={mn} max={mx}")
