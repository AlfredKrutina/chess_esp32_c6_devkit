import json
from pathlib import Path

root = Path(r"C:\Program Files\Bambu Studio\resources\profiles\BBL")
by_name = {}
for folder in ("machine", "process", "filament"):
    for path in (root / folder).glob("*.json"):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
        if data.get("name"):
            by_name[data["name"]] = data


def resolve(name):
    data = by_name[name]
    parent = data.get("inherits")
    base = resolve(parent) if parent else {}
    merged = dict(base)
    for key, value in data.items():
        if key != "inherits":
            merged[key] = value
    return merged


machine = resolve("Bambu Lab H2D 0.4 nozzle")
for path in (root / "machine").glob("Bambu Lab H2D 0.4 nozzle template *.json"):
    data = json.loads(path.read_text(encoding="utf-8"))
    for key, value in data.items():
        if key not in ("name", "inherits", "from", "instantiation", "type", "version", "setting_id", "description"):
            machine[key] = value

process = resolve("0.20mm Standard @BBL H2D")
filament = resolve("Bambu PLA Basic @BBL H2D")

process["layer_height"] = "0.2"
process["initial_layer_print_height"] = "0.2"
process["adaptive_layer_height"] = "0"
process["enable_support"] = "0"
process["brim_width"] = "0"

# Pause before the layer that roofs the magnet cavity. Head parks clear of the plate.
pause = """
{if layer_z > 2.35 && layer_z < 2.45}
; MAGNET white pawns x8  D10x1  north toward the bed
G91
G1 Z10 F600
G90
G1 X20 Y300 F30000
M400 U1
{endif}
{if layer_z > 3.35 && layer_z < 3.45}
; MAGNET white knights x2  D10x2  north toward the bed
G91
G1 Z10 F600
G90
G1 X20 Y300 F30000
M400 U1
{endif}
{if layer_z > 4.35 && layer_z < 4.45}
; MAGNET white bishops x2  D10x3  north toward the bed
G91
G1 Z10 F600
G90
G1 X20 Y300 F30000
M400 U1
{endif}
{if layer_z > 6.35 && layer_z < 6.45}
; MAGNET white rooks x2  D10x5  north toward the bed
G91
G1 Z10 F600
G90
G1 X20 Y300 F30000
M400 U1
{endif}
{if layer_z > 7.35 && layer_z < 7.45}
; MAGNET white king x1  D12x6  north toward the bed
G91
G1 Z10 F600
G90
G1 X20 Y300 F30000
M400 U1
{endif}
{if layer_z > 9.35 && layer_z < 9.45}
; MAGNET white queen x1  D10x8  north toward the bed
G91
G1 Z10 F600
G90
G1 X20 Y300 F30000
M400 U1
{endif}
"""
machine["layer_change_gcode"] = machine.get("layer_change_gcode", "") + pause

drop = {"description", "force_update", "renamed_from"}
for cfg, name, kind in (
    (machine, "Bambu Lab H2D 0.4 nozzle", "machine"),
    (process, "0.20mm Standard @BBL H2D", "process"),
    (filament, "Bambu PLA Basic @BBL H2D", "filament"),
):
    for key in drop:
        cfg.pop(key, None)
    cfg["name"] = name
    cfg["type"] = kind
    cfg["from"] = "system"
    cfg["instantiation"] = "true"

out = Path(__file__).resolve().parent
(out / "machine.json").write_text(json.dumps(machine, ensure_ascii=False), encoding="utf-8")
(out / "process.json").write_text(json.dumps(process, ensure_ascii=False), encoding="utf-8")
(out / "filament.json").write_text(json.dumps(filament, ensure_ascii=False), encoding="utf-8")
print("machine", len(machine), "process", len(process), "filament", len(filament))
