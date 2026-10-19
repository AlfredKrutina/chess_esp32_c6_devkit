# Axisymmetric field of one disc at the Hall pair.
# Stack, magnet face toward the board at z = 0, sensors at negative z:
#   1.2 mm piece floor, 4 mm glass, 1 mm diffuser sheet, 5 mm to the Hall plane.
# Glass, plastic and the sheet are non-magnetic, so they only add distance.
# The two DRV5055A1 dies sit at x = ±3.1335 mm, y = 0. Sensitive axis is Bz.

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "simulace-magnetu"

PLASTIC = 1.2
GLASS = 4.0
SHEET = 1.0
AIR = 5.0
GAP = PLASTIC + GLASS + SHEET + AIR
PITCH = 6.267
SENSOR_R = PITCH / 2
COUNTS_PER_MT = 4096.0 * 0.100 / 5.0
CLIP_MT = 21.0

# Middle of the catalog Br window. N42 is 1.28–1.32 T, N38 is 1.22–1.26 T.
PIECES = [
    ("pesec", "pěšec", 10.0, 1.0, 1.30, "N42"),
    ("jezdec", "jezdec", 10.0, 2.0, 1.30, "N42"),
    ("strelec", "střelec", 10.0, 3.0, 1.30, "N42"),
    ("vez", "věž", 10.0, 5.0, 1.24, "N38"),
    ("dama", "dáma", 10.0, 8.0, 1.30, "N42"),
    ("kral", "král", 12.0, 6.0, 1.30, "N42"),
]

BANDS = [
    (100_000, 249_417, "pěšec"),
    (249_418, 481_566, "jezdec"),
    (481_567, 923_502, "střelec"),
    (923_503, 1_687_416, "věž"),
    (1_687_417, 3_107_527, "dáma"),
    (3_107_528, 5_500_000, "král"),
]


def on_axis(z, radius, height, remanence):
    def face(zz):
        return zz / np.sqrt(zz * zz + radius * radius)

    return 0.5 * remanence * (face(z) - face(z - height))


def pole_face(r, z, radius, remanence, n_rho, n_phi):
    """Bz, Br of one disc pole at z = 0. remanence is the pole strength in tesla."""
    rho = np.linspace(0.0, radius, n_rho)
    phi = np.linspace(0.0, 2.0 * np.pi, n_phi, endpoint=False)
    drho = radius / (n_rho - 1)
    dphi = phi[1] - phi[0]
    rr, pp = np.meshgrid(rho, phi, indexing="ij")
    dx = r - rr * np.cos(pp)
    dy = 0.0 - rr * np.sin(pp)
    dist = np.sqrt(dx * dx + dy * dy + z * z)
    coef = (remanence / (4.0 * np.pi)) * rr * drho * dphi
    bz = np.sum(coef * z / dist**3)
    br = np.sum(coef * dx / dist**3)
    return float(bz), float(br)


def cylinder(r, z, radius, height, remanence, n_rho=480, n_phi=960):
    """Field of a disc from z = 0 to z = height, magnetization +z. Lengths in mm."""
    if r < 1e-9:
        bz = float(on_axis(z, radius, height, remanence))
        return bz, 0.0
    bz_n, br_n = pole_face(r, z - height, radius, remanence, n_rho, n_phi)
    bz_s, br_s = pole_face(r, z - 0.0, radius, remanence, n_rho, n_phi)
    return bz_n - bz_s, br_n - br_s


def check_integral():
    bz, _ = cylinder(0.0, -GAP, 5.0, 1.0, 1.30, 200, 400)
    exact = float(on_axis(-GAP, 5.0, 1.0, 1.30))
    if abs(bz - exact) > 0.005 * abs(exact):
        raise SystemExit(f"integral off axis check failed: {bz} vs {exact}")
    coarse = cylinder(SENSOR_R, -GAP, 5.0, 1.0, 1.30, 160, 320)[0]
    fine = cylinder(SENSOR_R, -GAP, 5.0, 1.0, 1.30, 480, 960)[0]
    if abs(coarse - fine) > 0.005 * abs(fine):
        raise SystemExit(f"off-axis integral not settled: {coarse} vs {fine}")


def energy_of(bz_mt):
    raw = COUNTS_PER_MT * bz_mt
    clipped = float(np.clip(raw, -CLIP_MT * COUNTS_PER_MT, CLIP_MT * COUNTS_PER_MT))
    return 2.0 * raw * raw, 2.0 * clipped * clipped, abs(raw) > CLIP_MT * COUNTS_PER_MT


def band_of(energy):
    if energy < BANDS[0][0]:
        return "pod pěšcem"
    if energy > BANDS[-1][1]:
        return "nad králem"
    for lo, hi, name in BANDS:
        if lo <= energy <= hi:
            return name
    return "mimo pásmo"


def gap_map(radius, height, remanence):
    r = np.linspace(0.0, 16.0, 81)
    z = np.linspace(-GAP - 2.0, 0.4, 70)
    rr, zz = np.meshgrid(r, z)
    bz = np.zeros_like(rr)
    n_rho, n_phi = 90, 160
    rho = np.linspace(0.0, radius, n_rho)
    phi = np.linspace(0.0, 2.0 * np.pi, n_phi, endpoint=False)
    drho = radius / (n_rho - 1)
    dphi = phi[1] - phi[0]
    coef = (remanence / (4.0 * np.pi)) * rho * drho * dphi

    def add(z_pole, sign):
        acc = np.zeros_like(rr)
        for ang in phi:
            dx = rr[:, :, None] - (rho * np.cos(ang))[None, None, :]
            dy = 0.0 - (rho * np.sin(ang))[None, None, :]
            dz = zz[:, :, None] - z_pole
            dist = np.sqrt(dx * dx + dy * dy + dz * dz)
            acc += np.sum(coef * dz / dist**3, axis=-1)
        return sign * acc

    bz = add(height, 1.0) + add(0.0, -1.0)
    bz[:, 0] = [float(on_axis(zi, radius, height, remanence)) for zi in z]
    return r, z, bz * 1e3


def draw(slug, title, radius, height, remanence, sensor_mt):
    r, z, bz = gap_map(radius, height, remanence)
    fig, axes = plt.subplots(1, 2, figsize=(9.2, 4.4), gridspec_kw={"width_ratios": [1.35, 1]})
    band = bz[z < -(PLASTIC + GLASS)]
    span = max(6.0, float(np.max(np.abs(band))))
    im = axes[0].pcolormesh(r, z, bz, cmap="RdBu_r", vmin=-span, vmax=span, shading="auto")
    edges = [0.0, -PLASTIC, -(PLASTIC + GLASS), -(PLASTIC + GLASS + SHEET), -GAP]
    labels = ["čelo magnetu", "plast", "sklo", "plíšek", "sondy"]
    for edge, label in zip(edges, labels):
        axes[0].axhline(edge, color="black", lw=0.6, alpha=0.7)
        axes[0].text(11.6, edge, label, va="center", ha="left", fontsize=7,
                     bbox={"fc": "white", "ec": "none", "alpha": 0.75, "pad": 0.4})
    axes[0].plot([SENSOR_R], [-GAP], "o", color="black", ms=5)
    axes[0].set_xlim(0, 16)
    axes[0].set_ylim(-GAP - 2.0, 0.4)
    axes[0].set_xlabel("vzdálenost od osy (mm)")
    axes[0].set_ylabel("z (mm), čelo magnetu = 0")
    axes[0].set_title(title)
    fig.colorbar(im, ax=axes[0], fraction=0.046, pad=0.08).set_label("Bz (mT)")

    rs = np.linspace(0.0, 16.0, 161)
    profile = [cylinder(ri, -GAP, radius, height, remanence, 120, 240)[0] * 1e3 for ri in rs]
    axes[1].plot(rs, profile, color="#1f4e79", lw=1.8)
    axes[1].axvline(SENSOR_R, color="#b85c38", lw=1.0, label=f"sonda {SENSOR_R:.2f} mm")
    axes[1].plot([SENSOR_R], [sensor_mt], "o", color="#b85c38")
    axes[1].set_xlabel("vzdálenost od osy (mm)")
    axes[1].set_ylabel("Bz v rovině sond (mT)")
    axes[1].set_title("řez rovinou Hall")
    axes[1].legend(frameon=False, fontsize=8)
    axes[1].set_xlim(0, 16)
    fig.tight_layout()
    fig.savefig(OUT / f"{slug}.png", dpi=140)
    plt.close(fig)
    profile_path = OUT / f"{slug}_profil.csv"
    with profile_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["r_mm", "Bz_mT"])
        writer.writerows(zip((round(v, 3) for v in rs), (round(v, 4) for v in profile)))


def write_report(rows):
    lines = [
        "# Pole magnetů v mezeře Hall",
        "",
        "Osově souměrný výpočet kotouče, magnetizace kolmo na desku. Sklo, plast i plíšek jsou nemagnetické, takže pole tvarují jen svou tloušťkou.",
        "",
        "Od čela magnetu směrem k sondám:",
        "",
        "| Vrstva | Tloušťka |",
        "|---|---:|",
        f"| plast dna figurky | {PLASTIC:.1f} mm |",
        f"| sklo | {GLASS:.1f} mm |",
        f"| plíšek (díl Folie) | {SHEET:.1f} mm |",
        f"| vzduch k rovině sond | {AIR:.1f} mm |",
        f"| celkem | {GAP:.1f} mm |",
        "",
        "Sondy nejsou na ose. Pár má rozteč 6,267 mm a střed páru je střed pole, magnet stojí uprostřed. Každá sonda je tedy 3,13 mm od osy. Osa citlivosti DRV5055A1 je kolmá k desce, bere se Bz. Obě sondy vidí stejné Bz.",
        "",
        "Br je střed katalogového okna: N42 = 1,30 T, N38 = 1,24 T. Model kotouče má μr = 1. Energie je dev₀² + dev₁² při 81,92 dílcích ADC na mT (A1, 100 mV/mT při 5 V, poměrné k napájení, 12bit ADC na stejném napájení). Nad ±21 mT senzor končí lineární rozsah.",
        "",
        "Export `Sklo.stl` má tloušťku 2 mm. Tady je sklo 4 mm, jak je zadané. Plíšek 1 mm je výška dílu Folie.",
        "",
        "Ocelový protiplech na zádech magnetu v tomhle poli není. Leží na opačné straně kotouče, ne mezi sklem a sondami.",
        "",
        "| Figurka | Kotouč | Bz na ose | Bz na sondě | Br na sondě | Energie | Po ořezu A1 | Pásmo firmwaru |",
        "|---|---|---:|---:|---:|---:|---:|---|",
    ]
    for row in rows:
        lines.append(
            "| {name} | Ø {d:.0f}×{h:.0f} {grade} | {axis:.1f} mT | {sens:.1f} mT | {rad:.1f} mT | {raw:.0f} | {clip:.0f} | {band} |".format(
                **row
            )
        )
    lines += [
        "",
        "Naměřený pěšec bez plechu je 103 050. Pásma v `hall_i2c_squares.c` jsou z jiného měření (stohy malých kotoučů).",
        "",
        "Obrázek u každé figurky je Bz v řezu mezerou a průběh Bz v rovině sond. Čísla průběhu jsou v `*_profil.csv`.",
        "",
    ]
    (OUT / "vysledky.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    check_integral()
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for slug, name, diameter, height, remanence, grade in PIECES:
        radius = diameter / 2.0
        bz, br = cylinder(SENSOR_R, -GAP, radius, height, remanence)
        axis = float(on_axis(-GAP, radius, height, remanence))
        raw, clipped, limited = energy_of(bz * 1e3)
        row = {
            "slug": slug,
            "name": name,
            "d": diameter,
            "h": height,
            "grade": grade,
            "axis": axis * 1e3,
            "sens": bz * 1e3,
            "rad": br * 1e3,
            "raw": raw,
            "clip": clipped,
            "band": band_of(clipped),
            "limited": limited,
        }
        rows.append(row)
        draw(slug, f"{name}  Ø{diameter:.0f}×{height:.0f} {grade}", radius, height, remanence, bz * 1e3)
        print(
            f"{name}: Bz axis {axis*1e3:.2f} mT, sensor {bz*1e3:.2f} mT, "
            f"energy {clipped:.0f} ({row['band']})"
        )
    with (OUT / "vysledky.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["slug", "name", "d", "h", "grade", "axis", "sens", "rad", "raw", "clip", "band", "limited"],
        )
        writer.writeheader()
        writer.writerows(rows)
    write_report(rows)


if __name__ == "__main__":
    main()
