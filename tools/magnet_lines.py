# Field lines of one disc in the board gap. Same stack as magnet_field.py.
# Magnetization points +z: lines leave the top face and enter the bottom face.

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image

from magnet_field import AIR, GAP, GLASS, OUT, PIECES, PLASTIC, SENSOR_R, SHEET

X_MAX = 18.0
Z_LO = -14.0
Z_HI = 20.0


def plane_field(radius, height, remanence):
    x = np.linspace(-X_MAX, X_MAX, 161)
    z = np.linspace(Z_LO, Z_HI, 141)
    xx, zz = np.meshgrid(x, z)
    bx = np.zeros_like(xx)
    bz = np.zeros_like(xx)
    n_rho, n_phi = 70, 96
    rho = np.linspace(0.0, radius, n_rho)
    phi = np.linspace(0.0, 2.0 * np.pi, n_phi, endpoint=False)
    drho = radius / (n_rho - 1)
    dphi = phi[1] - phi[0]
    coef = (remanence / (4.0 * np.pi)) * rho * drho * dphi

    def add(z_pole, sign):
        for ang in phi:
            sx = rho * np.cos(ang)
            sy = rho * np.sin(ang)
            dx = xx[:, :, None] - sx[None, None, :]
            dy = 0.0 - sy[None, None, :]
            dz = zz[:, :, None] - z_pole
            dist = np.sqrt(dx * dx + dy * dy + dz * dz)
            dist = np.maximum(dist, 0.35)
            kern = coef / dist**3
            bx_add = np.sum(kern * dx, axis=-1)
            bz_add = np.sum(kern * dz, axis=-1)
            bx[:] += sign * bx_add
            bz[:] += sign * bz_add

    add(height, 1.0)
    add(0.0, -1.0)
    return x, z, bx, bz


def draw_lines(slug, title, radius, height, remanence):
    x, z, bx, bz = plane_field(radius, height, remanence)
    speed = np.hypot(bx, bz)
    fig, ax = plt.subplots(figsize=(7.2, 6.2))
    bands = [
        (0.0, -PLASTIC, "#f4efe6", "plast"),
        (-PLASTIC, -(PLASTIC + GLASS), "#d7e6f2", "sklo"),
        (-(PLASTIC + GLASS), -(PLASTIC + GLASS + SHEET), "#ece3c8", "plíšek"),
        (-(PLASTIC + GLASS + SHEET), -GAP, "#f7f7f7", "vzduch"),
    ]
    for z0, z1, color, label in bands:
        ax.axhspan(z1, z0, color=color, zorder=0)
        ax.text(X_MAX - 0.4, 0.5 * (z0 + z1), label, ha="right", va="center", fontsize=8, color="#444")
    ax.streamplot(
        x, z, bx, bz,
        color="#1a3a5c",
        density=1.35,
        linewidth=0.9,
        arrowsize=0.7,
        minlength=0.15,
        broken_streamlines=True,
    )
    ax.add_patch(
        plt.Rectangle((-radius, 0.0), 2 * radius, height, facecolor="#c23b2e", edgecolor="#6e120c", lw=0.8, zorder=3, alpha=0.92)
    )
    ax.plot([-SENSOR_R, SENSOR_R], [-GAP, -GAP], "o", color="#111", ms=5, zorder=4)
    ax.axhline(-GAP, color="#111", lw=0.4, alpha=0.5)
    ax.set_xlim(-X_MAX, X_MAX)
    ax.set_ylim(Z_LO, max(Z_HI, height + 8))
    ax.set_aspect("equal")
    ax.set_xlabel("mm")
    ax.set_ylabel("mm, čelo magnetu = 0")
    ax.set_title(title)
    fig.tight_layout()
    path = OUT / f"{slug}_silocary.png"
    fig.savefig(path, dpi=140)
    plt.close(fig)
    return path


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    paths = []
    for slug, name, diameter, height, remanence, grade in PIECES:
        path = draw_lines(slug, f"{name}  Ø{diameter:.0f}×{height:.0f} {grade}", diameter / 2.0, height, remanence)
        paths.append(path)
        print(path.name)
    frames = [Image.open(p).convert("RGB") for p in paths]
    frames[0].save(OUT / "silocary.gif", save_all=True, append_images=frames[1:], duration=1600, loop=0)
    w, h = frames[0].size
    sheet = Image.new("RGB", (w * 3, h * 2), "white")
    for i, frame in enumerate(frames):
        sheet.paste(frame, ((i % 3) * w, (i // 3) * h))
    sheet.save(OUT / "silocary_prehled.png")


if __name__ == "__main__":
    main()
