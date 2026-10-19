# Axisymmetric magnetostatic run in agrossuite.
# Plate is under the magnet with a 6 mm gap. Lengths in the solver are metres.

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from agrossuite import agros

OUT = Path(__file__).resolve().parents[1] / "docs" / "simulace-magnetu" / "agros"
MU0 = 4.0 * np.pi * 1e-7

# H in A/m, B in T. The same curve used in SimScale, plus the origin.
BH = [
    (0.0, 0.0),
    (100.0, 0.40),
    (200.0, 0.80),
    (300.0, 1.05),
    (500.0, 1.25),
    (1000.0, 1.45),
    (2000.0, 1.55),
    (5000.0, 1.70),
    (10000.0, 1.85),
    (20000.0, 1.95),
    (50000.0, 2.05),
    (100000.0, 2.15),
]

PIECES = [
    ("pesec", 5.0e-3, 1.0e-3, 1.30),
    ("jezdec", 5.0e-3, 2.0e-3, 1.30),
    ("strelec", 5.0e-3, 3.0e-3, 1.30),
    ("vez", 5.0e-3, 5.0e-3, 1.24),
    ("dama", 5.0e-3, 8.0e-3, 1.30),
    ("kral", 6.0e-3, 6.0e-3, 1.30),
]

PLATE_R = 5.5e-3
PLATE_H = 1.0e-3
GAP = 6.0e-3
MAGNET_Z = PLATE_H + GAP
PROBE_R = 3.1335e-3
PROBE_Z = -5.0e-3
R_AIR = 40.0e-3
Z_LO = -20.0e-3
Z_HI = 40.0e-3


def steel_permeability():
    b_values = [0.0]
    mur = [BH[1][1] / (MU0 * BH[1][0])]
    for field_h, flux_b in BH[1:]:
        b_values.append(flux_b)
        mur.append(flux_b / (MU0 * field_h))
    return {
        "value": mur[0],
        "x": b_values,
        "y": mur,
        "interpolation": "piecewise_linear",
        "extrapolation": "constant",
        "derivative_at_endpoints": "first",
    }


def magnet_force(solution, radius, height):
    # Closed contour in air. Positive result pulls the magnet down onto the plate.
    pad = 0.4e-3
    z0 = MAGNET_Z - pad
    z1 = MAGNET_Z + height + pad
    outer = radius + pad
    count = 36
    radii = np.linspace(0.3e-3, outer, count)
    heights = np.linspace(z0, z1, count)

    def trap(samples, values):
        step = samples[1] - samples[0]
        return (values[0] + values[-1]) * 0.5 * step + np.sum(values[1:-1]) * step

    bottom = np.empty(count)
    top = np.empty(count)
    for index, radius_m in enumerate(radii):
        local = solution.local_values(float(radius_m), float(z0))
        bottom[index] = (local["Brr"] ** 2 - local["Brz"] ** 2) / (2 * MU0) * 2 * np.pi * radius_m
        local = solution.local_values(float(radius_m), float(z1))
        top[index] = (local["Brz"] ** 2 - local["Brr"] ** 2) / (2 * MU0) * 2 * np.pi * radius_m
    side = np.empty(count)
    for index, height_m in enumerate(heights):
        local = solution.local_values(float(outer), float(height_m))
        side[index] = local["Brr"] * local["Brz"] / MU0 * 2 * np.pi * outer
    return -(trap(radii, bottom) + trap(radii, top) + trap(heights, side))


def solve_piece(name, radius, height, remanence):
    problem = agros.problem(clear=True)
    problem.coordinate_type = "axisymmetric"
    problem.mesh_type = "triangle"
    magnetic = problem.field("magnetic")
    magnetic.analysis_type = "steadystate"
    magnetic.number_of_refinements = 0
    magnetic.polynomial_order = 2
    magnetic.solver = "newton"
    magnetic.solver_parameters["residual"] = 1.0
    magnetic.solver_parameters["damping"] = "automatic"
    magnetic.solver_parameters["damping_factor"] = 0.6
    magnetic.solver_parameters["jacobian_reuse"] = True
    magnetic.solver_parameters["jacobian_reuse_ratio"] = 0.8
    magnetic.solver_parameters["jacobian_reuse_steps"] = 10
    magnetic.add_boundary("A = 0", "magnetic_potential", {"magnetic_potential_real": 0})
    magnetic.add_material("Air", {"magnetic_permeability": 1, "magnetic_remanence": 0, "magnetic_remanence_angle": 0, "magnetic_conductivity": 0, "magnetic_current_density_external_real": 0})
    magnetic.add_material("Steel", {"magnetic_permeability": steel_permeability(), "magnetic_remanence": 0, "magnetic_remanence_angle": 0, "magnetic_conductivity": 0, "magnetic_current_density_external_real": 0})
    magnetic.add_material("Magnet", {"magnetic_permeability": 1.05, "magnetic_remanence": remanence, "magnetic_remanence_angle": 90, "magnetic_conductivity": 0, "magnetic_current_density_external_real": 0})

    top = MAGNET_Z + height
    geometry = problem.geometry()
    geometry.add_edge(0, Z_LO, R_AIR, Z_LO, boundaries={"magnetic": "A = 0"})
    geometry.add_edge(R_AIR, Z_LO, R_AIR, Z_HI, boundaries={"magnetic": "A = 0"})
    geometry.add_edge(R_AIR, Z_HI, 0, Z_HI, boundaries={"magnetic": "A = 0"})
    axis = {"magnetic": "A = 0"}
    geometry.add_edge(0, Z_HI, 0, top, boundaries=axis)
    geometry.add_edge(0, top, 0, MAGNET_Z, boundaries=axis)
    geometry.add_edge(0, MAGNET_Z, 0, PLATE_H, boundaries=axis)
    geometry.add_edge(0, PLATE_H, 0, 0, boundaries=axis)
    geometry.add_edge(0, 0, 0, Z_LO, boundaries=axis)
    geometry.add_edge(0, 0, PLATE_R, 0)
    geometry.add_edge(PLATE_R, 0, PLATE_R, PLATE_H)
    geometry.add_edge(PLATE_R, PLATE_H, 0, PLATE_H)
    geometry.add_edge(0, MAGNET_Z, radius, MAGNET_Z)
    geometry.add_edge(radius, MAGNET_Z, radius, top)
    geometry.add_edge(radius, top, 0, top)

    geometry.add_label(0.02, 0.02, materials={"magnetic": "Air"}, area=2e-5)
    geometry.add_label(PLATE_R * 0.4, PLATE_H * 0.5, materials={"magnetic": "Steel"}, area=2e-8)
    geometry.add_label(radius * 0.4, MAGNET_Z + height * 0.5, materials={"magnetic": "Magnet"}, area=5e-8)

    computation = problem.computation()
    computation.solve()
    solution = computation.solution("magnetic")
    below = solution.local_values(PROBE_R, PROBE_Z)
    gap = solution.local_values(0.002, PLATE_H + GAP * 0.5)
    b_plate = 0.0
    mur_plate = 1.0
    for radius_mm in (0.5, 1.5, 2.5, 3.5, 4.5, 5.2):
        sample = solution.local_values(radius_mm * 1e-3, PLATE_H * 0.5)
        if sample["Br"] > b_plate:
            b_plate = sample["Br"]
            mur_plate = sample["mur"]
    return {
        "name": name,
        "bz_mT": below["Brz"] * 1e3,
        "b_plate": b_plate,
        "mur_plate": mur_plate,
        "b_gap": gap["Br"],
        "force_N": magnet_force(solution, radius, height),
        "solution": solution,
        "radius": radius,
        "height": height,
    }


def field_map(result):
    radius = result["radius"]
    height = result["height"]
    r = np.linspace(0.0004, 0.018, 36)
    z = np.linspace(-0.012, MAGNET_Z + height + 0.008, 50)
    rr, zz = np.meshgrid(r, z)
    br = np.zeros_like(rr)
    bz = np.zeros_like(rr)
    solution = result["solution"]
    for i in range(rr.shape[0]):
        for j in range(rr.shape[1]):
            local = solution.local_values(float(rr[i, j]), float(zz[i, j]))
            br[i, j] = local["Brr"]
            bz[i, j] = local["Brz"]
    return r * 1e3, z * 1e3, br, bz


def draw(rows, field):
    fig, axes = plt.subplots(1, 3, figsize=(12.4, 4.4))
    r, z, br, bz = field
    _, radius, height, _ = next(item for item in PIECES if item[0] == "dama")
    span = 0.03
    axes[0].pcolormesh(r, z, bz, cmap="RdBu_r", vmin=-span, vmax=span, shading="auto")
    axes[0].streamplot(r, z, br, bz, color="#1a3a5c", density=1.1, linewidth=0.8, arrowsize=0.7)
    axes[0].add_patch(plt.Rectangle((0, 0), PLATE_R * 1e3, PLATE_H * 1e3, color="#555", zorder=3))
    axes[0].add_patch(plt.Rectangle((0, MAGNET_Z * 1e3), radius * 1e3, height * 1e3, color="#c23b2e", zorder=3))
    axes[0].plot([PROBE_R * 1e3], [PROBE_Z * 1e3], "o", color="black", ms=4)
    axes[0].set_aspect("equal")
    axes[0].set_xlabel("r (mm)")
    axes[0].set_ylabel("z (mm)")
    axes[0].set_title("dáma, řez, Bz do ±30 mT")
    names = [row["name"] for row in rows]
    x = np.arange(len(rows))
    axes[1].bar(x, [row["b_plate"] for row in rows], color="#555")
    axes[1].axhline(1.6, color="#c23b2e", lw=0.8)
    axes[1].set_xticks(x, names)
    axes[1].set_ylabel("max |B| v plechu (T)")
    axes[1].set_ylim(0, 2.0)
    axes[1].set_title("koleno oceli je 1,6 T")
    axes[2].bar(x - 0.18, [row["force_N"] for row in rows], width=0.36, color="#c23b2e")
    ax2 = axes[2].twinx()
    ax2.bar(x + 0.18, [row["bz_mT"] for row in rows], width=0.36, color="#1f4e79")
    axes[2].set_xticks(x, names)
    axes[2].set_ylabel("tah magnetu k plechu (N)")
    ax2.set_ylabel("Bz 5 mm pod plechem (mT)")
    axes[2].set_title("přitažení a pole pod plechem")
    fig.tight_layout()
    fig.savefig(OUT / "agros.png", dpi=140)
    plt.close(fig)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    queen_field = None
    for name, radius, height, remanence in PIECES:
        print(f"solving {name}", flush=True)
        row = solve_piece(name, radius, height, remanence)
        print(f"{name}: plate {row['b_plate']:.2f} T, mur {row['mur_plate']:.0f}, Bz {row['bz_mT']:.2f} mT, Fz {row['force_N']:.4f} N", flush=True)
        if name == "dama":
            queen_field = field_map(row)
        rows.append({key: row[key] for key in ("name", "bz_mT", "b_plate", "mur_plate", "b_gap", "force_N")})
    with (OUT / "agros.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    draw(rows, queen_field)


if __name__ == "__main__":
    main()
