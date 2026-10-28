"""Shaded preview pictures of the case (run with FreeCAD's Python, which has
matplotlib):  /Applications/FreeCAD.app/Contents/Resources/bin/python case/render_preview.py"""
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

HERE = os.path.dirname(os.path.abspath(__file__))


def load_stl(path):
    data = open(path, "rb").read()
    if data[:5] == b"solid" and b"facet" in data[:400]:
        tris = []
        for line in data.decode(errors="ignore").splitlines():
            line = line.strip()
            if line.startswith("vertex"):
                tris.append([float(v) for v in line.split()[1:4]])
        return np.array(tris).reshape(-1, 3, 3)
    n = int.from_bytes(data[80:84], "little")
    rec = np.frombuffer(data[84:84 + n * 50], dtype=np.dtype([("n", "<3f4"), ("v", "<9f4"), ("a", "<u2")]))
    return rec["v"].reshape(-1, 3, 3).astype(float)


def render(stl, out, elev, azim, color=(0.80, 0.82, 0.86), title=None, size=(1000, 800)):
    """A small z-buffer renderer: correct hidden surfaces (matplotlib's 3D
    painter's algorithm can't sort a mesh like this)."""
    tris = load_stl(os.path.join(HERE, "stl", stl))
    e, az = np.radians(elev), np.radians(azim)
    # Camera: looking from (azimuth, elevation) toward the centre.
    view = np.array([np.cos(e) * np.cos(az), np.cos(e) * np.sin(az), np.sin(e)])
    right = np.cross([0, 0, 1], view); right /= np.linalg.norm(right)
    up = np.cross(view, right)
    pts = tris.reshape(-1, 3)
    c = (pts.min(0) + pts.max(0)) / 2
    p = pts - c
    sx, sy, depth = p @ right, p @ up, p @ view
    W, H = size
    scale = 0.86 * min(W / (sx.max() - sx.min()), H / (sy.max() - sy.min()))
    X = (W / 2 + sx * scale).reshape(-1, 3)
    Y = (H / 2 - sy * scale).reshape(-1, 3)
    Z = depth.reshape(-1, 3)
    normals = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True) + 1e-12
    light = 0.55 * view + 0.45 * up + 0.25 * right
    light /= np.linalg.norm(light)
    shade = 0.28 + 0.72 * np.abs(normals @ light)
    zbuf = np.full((H, W), -np.inf)
    img = np.ones((H, W, 3))
    col = np.array(color)
    for i in range(len(tris)):
        x, y, z = X[i], Y[i], Z[i]
        x0, x1 = int(max(0, np.floor(x.min()))), int(min(W - 1, np.ceil(x.max())))
        y0, y1 = int(max(0, np.floor(y.min()))), int(min(H - 1, np.ceil(y.max())))
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(d) < 1e-12:
            continue
        w0 = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / d
        w1 = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / d
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            continue
        zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
        sub = zbuf[y0:y1 + 1, x0:x1 + 1]
        nearer = inside & (zz > sub)
        sub[nearer] = zz[nearer]
        img[y0:y1 + 1, x0:x1 + 1][nearer] = col * shade[i]
    fig = plt.figure(figsize=(W / 100, H / 100), dpi=100)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.imshow(np.clip(img, 0, 1))
    ax.set_axis_off()
    if title:
        ax.text(W / 2, 30, title, ha="center", fontsize=15)
    fig.savefig(out, facecolor="white")
    plt.close(fig)


out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "preview")
os.makedirs(out_dir, exist_ok=True)
render("assembly-preview.stl", os.path.join(out_dir, "case-front.png"), 20, 215, title="SkyScope Satellite desk console")
render("assembly-preview.stl", os.path.join(out_dir, "case-side.png"), 5, 270, title="Side: 65 degree screen, hood and desk ledge")
render("assembly-preview.stl", os.path.join(out_dir, "case-back.png"), 25, 35, title="Back: vented cover with cable notch")
render("bezel.stl", os.path.join(out_dir, "bezel-back.png"), 60, -60, color=(0.55, 0.75, 0.95), title="Bezel (back): standoffs and corner guides")
render("console.stl", os.path.join(out_dir, "console-inside.png"), 35, 20, title="Console from behind, cover off")
print("previews in", out_dir)
