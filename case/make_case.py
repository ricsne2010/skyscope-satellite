"""SkyScope Satellite desk console: a 3D-printable case for the Sunton
ESP32-8048S043 (4.3" 800x480), styled after an ATC radar console: a sloped
screen panel under a hood, a desk ledge in front, radar rings on the sides.
It fits SkyScope Mini too (same board).

Three parts, written to case/stl/ (and case/step/ for editing in CAD):
  console.stl     the body; prints upright on its base
  bezel.stl       the screen panel the board screws onto; prints face down
  back-cover.stl  vented cover with a cable notch; prints outside face down
  fit-test.stl    a 2 mm slice of the bezel, to check the board fits first

Run with FreeCAD's command-line Python (FreeCAD 1.0 or newer):
  /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd -c \\
      "exec(open('case/make_case.py').read())"

MEASURE YOUR BOARD FIRST. The board's makers publish no mechanical drawing;
the values marked "check" below are best estimates. Measure with calipers,
edit, run again, and print fit-test.stl before the full parts.
"""
import math
import os

import FreeCAD
import MeshPart
import Part
from FreeCAD import Vector as V

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.path.join(os.getcwd(), "case")

# --- the board (mm) -----------------------------------------------------------------
PCB_W, PCB_H = 123.0, 74.0      # circuit board outline (retailer data)
LCD_W, LCD_H = 105.5, 67.2      # display glass outline (check)
LCD_OFF_X, LCD_OFF_Y = 0.0, 0.0  # display centre minus board centre, X right / Y up as you look at it (check)
VIEW_W, VIEW_H = 97.5, 56.0     # window: the 95.0 x 53.9 visible area plus a margin
LCD_T = 4.2                     # front of the glass to the front of the circuit board (check)
HOLE_DX, HOLE_DY = 58.0, 33.5   # mounting holes: centre-to-hole, X and Y (3.5 mm in from the corners; check)
BEHIND = 14.0                   # tallest parts behind the board (ESP32 module, connectors) plus room
USB_ROOM = 12.0                 # room each side of the board for a USB-C plug (a right-angle plug fits best)

# --- the console --------------------------------------------------------------------
TILT = 65.0                     # screen angle from the desk, degrees
WALL = 3.0
PANEL_T = 3.0                   # bezel thickness
SHELF_X, SHELF_Z = 20.0, 12.0   # desk ledge: depth to the foot of the screen, height
FACE_L = 96.0                   # screen panel length up the slope
DEPTH, BACK_H = 102.0, 84.0     # base depth; height of the back wall
CLEAR = 0.3                     # fit clearance per side for printed parts
LEDGE_TEXT = "SKYSCOPE  SATELLITE"   # engraved on the desk ledge ("SKYSCOPE  MINI" for a Mini)

# M3 screws into brass heat-set inserts (M3 x 4 mm long, about 5 mm across;
# most makers ask for a 4.0 mm hole: check yours). To screw straight into the
# plastic instead, set INSERT_D = 2.6.
INSERT_D = 4.0                  # hole for the insert
STANDOFF_D = 8.0                # bezel standoffs: room around a hot insert
CLEARANCE, HEAD = 3.4, 6.4      # screw clearance hole; countersunk head

FONT = next((f for f in ["/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                         "/Applications/FreeCAD.app/Contents/Resources/fonts/DejaVuSans.ttf"]
             if os.path.exists(f)), None)

W = PCB_W + 2 * USB_ROOM + 2 * WALL + 2 * 0.5   # console width
WI = W - 2 * WALL                                # inside width
a = math.radians(TILT)
U = V(math.cos(a), 0, math.sin(a))     # up the screen
N = V(-math.sin(a), 0, math.cos(a))    # out of the screen, toward you
O = V(SHELF_X, 0, SHELF_Z)             # foot of the screen panel, centre line


def panel(shape):
    """Place a shape built in panel coordinates (x across, y up the slope,
    z out of the panel; origin at the panel's foot, centre) onto the console."""
    m = FreeCAD.Matrix(0, U.x, N.x, O.x,
                       -1, U.y, N.y, O.y,   # panel x is your right as you face it: -Y
                       0, U.z, N.z, O.z,
                       0, 0, 0, 1)
    s = shape.copy()
    s.transformShape(m)
    return s


def engrave(solid, xdir, ydir, surface_point):
    """Place an engraving made in the XY plane (z from 0 to its depth, read
    from +z) onto a flat surface: xdir = reading direction, ydir = up, and
    the surface's outward normal is xdir x ydir."""
    zdir = xdir.cross(ydir)
    depth = solid.BoundBox.ZMax
    o = surface_point - zdir * depth
    m = FreeCAD.Matrix(xdir.x, ydir.x, zdir.x, o.x,
                       xdir.y, ydir.y, zdir.y, o.y,
                       xdir.z, ydir.z, zdir.z, o.z,
                       0, 0, 0, 1)
    out = solid.copy()
    out.transformShape(m)
    return out


def box(x0, x1, y0, y1, z0, z1):
    return Part.makeBox(x1 - x0, y1 - y0, z1 - z0, V(x0, y0, z0))


def cyl(r, h, x, y, z, axis=V(0, 0, 1)):
    return Part.makeCylinder(r, h, V(x, y, z), axis)


def text_faces(s, size):
    """Flat faces of a string in the XY plane, left/baseline at the origin."""
    faces = []
    for char in Part.makeWireString(s, os.path.dirname(FONT) + "/", os.path.basename(FONT), size, 0):
        wires = sorted(char, key=lambda w: -Part.Face(w).Area)
        shape = None
        for w in wires:
            f = Part.Face(w)
            if shape is None:
                shape = f
            elif shape.common(f).Area > 0.5 * f.Area:
                shape = shape.cut(f)   # the hole in an O, A, P...
            else:
                shape = shape.fuse(f)
        if shape is not None:
            faces.append(shape)
    return Part.makeCompound(faces)


def text_solid(s, size, depth):
    t = text_faces(s, size)
    bb = t.BoundBox
    t.translate(V(-bb.XMin - bb.XLength / 2, -bb.YMin - bb.YLength / 2, 0))  # centred
    return t.extrude(V(0, 0, depth))


def ring_grooves(radii, width, depth):
    out = None
    for r in radii:
        g = cyl(r + width / 2, depth, 0, 0, 0).cut(cyl(r - width / 2, depth, 0, 0, 0))
        out = g if out is None else out.fuse(g)
    return out


# --- console body -----------------------------------------------------------------------
top = O + U * FACE_L
visor = top + V(-1, 0, 1).normalize() * 14          # hood: 45 degrees underneath, so it prints
hood_back = V(visor.x + 23, 0, visor.z + 3)
profile = [V(0, 0, 0), V(0, 0, SHELF_Z - 2), V(2, 0, SHELF_Z), O, top, visor, hood_back,
           V(DEPTH, 0, BACK_H), V(DEPTH, 0, 0)]
outer = Part.Face(Part.makePolygon(profile + [profile[0]]))
body = outer.extrude(V(0, W, 0))
body.translate(V(0, -W / 2, 0))
inner = outer.makeOffset2D(-WALL, 2)
cavity = inner.extrude(V(0, WI, 0))
cavity.translate(V(0, -WI / 2, 0))
body = body.cut(cavity)

BW = W - 4.0                       # bezel width
BH = FACE_L - 8.0                  # bezel height, from 4 to FACE_L - 4 up the slope
B0 = 4.0
# Recess the bezel sits in (flush), and the opening behind it for the board.
body = body.cut(panel(box(-BW / 2 - CLEAR, BW / 2 + CLEAR, B0 - CLEAR, B0 + BH + CLEAR, -PANEL_T, 1)))
body = body.cut(panel(box(-WI / 2, WI / 2, B0 + 2, B0 + BH - 2, -PANEL_T - 30, 0)))
# Corner pads for the bezel's four screws, joined to the side walls.
PAD_U = [(B0 + 2, B0 + 14), (B0 + BH - 14, B0 + BH - 2)]
SCREWS = []
for side in (-1, 1):
    for u0, u1 in PAD_U:
        x0, x1 = (WI / 2 - 8, WI / 2 + 0.5) if side > 0 else (-WI / 2 - 0.5, -WI / 2 + 8)
        body = body.fuse(panel(box(x0, x1, u0, u1, -PANEL_T - 12, -PANEL_T)))
        # A 45 degree gusset under the pad, so it prints without supports.
        xi, xw = side * (WI / 2 - 8), side * (WI / 2 + 0.5)
        tri = Part.Face(Part.makePolygon([V(xi, u0, 0), V(xw, u0, 0), V(xw, u0 - 9, 0), V(xi, u0, 0)]))
        gusset = tri.extrude(V(0, 0, -12))
        gusset.translate(V(0, 0, -PANEL_T))
        body = body.fuse(panel(gusset))
        SCREWS.append((side * (WI / 2 - 4), (u0 + u1) / 2))
for sx, sy in SCREWS:
    body = body.cut(panel(cyl(INSERT_D / 2, 11, sx, sy, -PANEL_T - 11)))

# Back opening, and bosses for the cover's screws.
OPEN_Z0, OPEN_Z1 = 10.0, BACK_H - 10.0
body = body.cut(box(DEPTH - WALL - 1, DEPTH + 1, -WI / 2 + 9, WI / 2 - 9, OPEN_Z0, OPEN_Z1))
body = body.cut(box(DEPTH - WALL - 1, DEPTH + 1, -8, 8, -1, OPEN_Z0 + 1))   # cable exit, matching the cover's notch
COVER_HOLES = [(sy * (W / 2 - 7.5), z) for sy in (-1, 1) for z in (OPEN_Z0 + 6, OPEN_Z1 - 6)]
for y, z in COVER_HOLES:
    boss = cyl(4.5, 9, DEPTH - WALL, y, z, V(-1, 0, 0))
    body = body.fuse(boss)
    body = body.cut(cyl(INSERT_D / 2, 12, DEPTH + 1, y, z, V(-1, 0, 0)))
body = body.cut(box(DEPTH - WALL - 1, DEPTH + 1, -WI / 2 + 9, WI / 2 - 9, OPEN_Z0, OPEN_Z1))

# Radar rings and a sweep line on both sides; the name on the desk ledge.
emblem = ring_grooves([9, 18, 27, 36], 1.4, 0.9)
sweep = Part.makeBox(38, 1.4, 0.9, V(0, -0.7, 0))
sweep.rotate(V(0, 0, 0), V(0, 0, 1), 40)
cross = Part.makeBox(76, 0.8, 0.9, V(-38, -0.4, 0)).fuse(Part.makeBox(0.8, 76, 0.9, V(-0.4, -38, 0)))
emblem = emblem.fuse(sweep).fuse(cross)
for ang, r in [(28, 20), (14, 31), (62, 12)]:   # blips
    emblem = emblem.fuse(cyl(1.6, 0.9, r * math.cos(math.radians(ang)), r * math.sin(math.radians(ang)), 0))
SIDE_C = (72.0, 46.0)                           # emblem centre on the side: depth, height
body = body.cut(engrave(emblem, V(-1, 0, 0), V(0, 0, 1), V(SIDE_C[0], W / 2, SIDE_C[1])))
body = body.cut(engrave(emblem, V(1, 0, 0), V(0, 0, 1), V(SIDE_C[0], -W / 2, SIDE_C[1])))
if FONT:
    body = body.cut(engrave(text_solid(LEDGE_TEXT, 5.5, 0.7), V(0, -1, 0), V(0, 0, 1),
                            V(0, 0, (SHELF_Z - 2) / 2)))

# --- bezel (panel coordinates; printed face down) -----------------------------------------
cx, cy = LCD_OFF_X, B0 + BH / 2 + LCD_OFF_Y       # display centre
bez = box(-BW / 2, BW / 2, B0, B0 + BH, -PANEL_T, 0)
# Window with a 45 degree chamfer, so the edge doesn't shade the screen.
back_rect = Part.makePolygon([V(cx - VIEW_W / 2, cy - VIEW_H / 2, -PANEL_T - 0.01), V(cx + VIEW_W / 2, cy - VIEW_H / 2, -PANEL_T - 0.01),
                              V(cx + VIEW_W / 2, cy + VIEW_H / 2, -PANEL_T - 0.01), V(cx - VIEW_W / 2, cy + VIEW_H / 2, -PANEL_T - 0.01),
                              V(cx - VIEW_W / 2, cy - VIEW_H / 2, -PANEL_T - 0.01)])
grow = PANEL_T * 0.8
front_rect = Part.makePolygon([V(cx - VIEW_W / 2 - grow, cy - VIEW_H / 2 - grow, 0.01), V(cx + VIEW_W / 2 + grow, cy - VIEW_H / 2 - grow, 0.01),
                               V(cx + VIEW_W / 2 + grow, cy + VIEW_H / 2 + grow, 0.01), V(cx - VIEW_W / 2 - grow, cy + VIEW_H / 2 + grow, 0.01),
                               V(cx - VIEW_W / 2 - grow, cy - VIEW_H / 2 - grow, 0.01)])
bez = bez.cut(Part.makeLoft([back_rect, front_rect], True))
# Guides at the display's corners, so the glass sits square in the window.
for sx in (-1, 1):
    for sy in (-1, 1):
        gx = cx + sx * (LCD_W / 2 + CLEAR + 0.8)
        gy = cy + sy * (LCD_H / 2 + CLEAR + 0.8)
        g = box(gx - 0.8 - (6 if sx > 0 else 0), gx + 0.8 + (6 if sx < 0 else 0), gy - 0.8, gy + 0.8, -PANEL_T - 2, -PANEL_T)
        g = g.fuse(box(gx - 0.8, gx + 0.8, gy - 0.8 - (6 if sy > 0 else 0), gy + 0.8 + (6 if sy < 0 else 0), -PANEL_T - 2, -PANEL_T))
        bez = bez.fuse(g)
# Standoffs for the board's mounting holes (board centre = bezel centre).
bcx, bcy = 0.0, B0 + BH / 2
for sx in (-1, 1):
    for sy in (-1, 1):
        hx, hy = bcx + sx * HOLE_DX, bcy + sy * HOLE_DY
        bez = bez.fuse(cyl(STANDOFF_D / 2, LCD_T, hx, hy, -PANEL_T - LCD_T))
        # Leaves 1.2 mm of panel in front: press the insert in only flush with the standoff's top.
        bez = bez.cut(cyl(INSERT_D / 2, LCD_T + PANEL_T - 1.2, hx, hy, -PANEL_T - LCD_T - 0.01))
# Countersunk screws into the console's corner pads: a console-panel look.
for sx, sy in SCREWS:
    bez = bez.cut(cyl(CLEARANCE / 2, PANEL_T + 1, sx, sy, -PANEL_T - 0.5))
    bez = bez.cut(Part.makeCone(CLEARANCE / 2, HEAD / 2 + 0.2, (HEAD - CLEARANCE) / 2 + 0.2, V(sx, sy, -(HEAD - CLEARANCE) / 2)))
# Panel engraving: the name up the left, a small scope on the right.
side_zone = (BW / 2 + (cx + VIEW_W / 2 + grow)) / 2
if FONT:
    name = text_solid("SKYSCOPE", 7, 0.6)
    name.rotate(V(0, 0, 0), V(0, 0, 1), 90)
    name.translate(V(-side_zone, bcy, -0.6))
    bez = bez.cut(name)
scope = ring_grooves([4, 8, 12], 1.0, 0.6).fuse(Part.makeBox(26, 0.8, 0.6, V(-13, -0.4, 0))).fuse(
    Part.makeBox(0.8, 26, 0.6, V(-0.4, -13, 0)))
scope.translate(V(side_zone, bcy + 18, -0.6))
bez = bez.cut(scope)
for i, (dx, dy) in enumerate([(0, 0), (0, -10), (0, -20)]):   # three "status lamps", raised
    bez = bez.fuse(cyl(2.2, 0.8, side_zone + dx, bcy - 6 + dy, 0))

bezel = panel(bez)                       # in place on the console (for the preview)
# The fit test: the back 2 mm of the bezel with its standoffs and corner guides.
fit = bez.common(box(-BW, BW, -10, FACE_L + 10, -PANEL_T - LCD_T - 3, -PANEL_T + 2.0))

# --- back cover (printed outside face down) ------------------------------------------------
CW, CH, CT = W - 2.0, BACK_H - 2.0, 2.4
cov = box(DEPTH, DEPTH + CT, -CW / 2, CW / 2, 1.0, 1.0 + CH)
for y, z in COVER_HOLES:
    cov = cov.cut(cyl(CLEARANCE / 2, CT + 2, DEPTH - 1, y, z, V(1, 0, 0)))
    cov = cov.cut(Part.makeCone(CLEARANCE / 2, HEAD / 2 + 0.2, (HEAD - CLEARANCE) / 2 + 0.2,
                                V(DEPTH + CT - (HEAD - CLEARANCE) / 2, y, z), V(1, 0, 0)))
cov = cov.cut(box(DEPTH - 1, DEPTH + CT + 1, -8, 8, 0, 10))   # cable notch
for i in range(6):                                            # vents
    z = 30 + i * 7
    for y0 in (-50, 6):
        cov = cov.cut(box(DEPTH - 1, DEPTH + CT + 1, y0, y0 + 44, z, z + 3))
if FONT:
    cov = cov.cut(engrave(text_solid("PROJECT MOON PI", 5, 0.6), V(0, 1, 0), V(0, 0, 1), V(DEPTH + CT, 0, 20)))

# --- write ---------------------------------------------------------------------------------
def on_bed(shape):
    s = shape.copy()
    bb = s.BoundBox
    s.translate(V(-bb.XMin - bb.XLength / 2, -bb.YMin - bb.YLength / 2, -bb.ZMin))
    return s


def face_down(shape):
    """A panel-coordinate part with its front (z = 0) on the bed."""
    s = shape.copy()
    s.rotate(V(0, 0, 0), V(1, 0, 0), 180)
    return on_bed(s)


cover_print = cov.copy()
cover_print.rotate(V(0, 0, 0), V(0, 1, 0), 90)   # outside face (+X) down
parts = {"console": on_bed(body), "bezel": face_down(bez), "back-cover": on_bed(cover_print),
         "fit-test": face_down(fit)}
os.makedirs(os.path.join(HERE, "stl"), exist_ok=True)
os.makedirs(os.path.join(HERE, "step"), exist_ok=True)
for name, shape in parts.items():
    ok = shape.isValid()
    step = os.path.join(HERE, "step", name + ".step")
    shape.exportStep(step)
    # Mesh the STEP round trip: it heals tiny gaps that otherwise leave a few
    # non-manifold edges in the STL.
    healed = Part.read(step)
    mesh = MeshPart.meshFromShape(Shape=healed, LinearDeflection=0.05, AngularDeflection=0.15, Relative=False)
    mesh.write(os.path.join(HERE, "stl", name + ".stl"))
    if name == "fit-test":
        os.remove(step)
    bb = shape.BoundBox
    print(f"{name}: {bb.XLength:.1f} x {bb.YLength:.1f} x {bb.ZLength:.1f} mm, {shape.Volume / 1000:.0f} cm3, "
          f"valid={ok}, solids={len(shape.Solids)}, watertight={mesh.isSolid() and not mesh.hasNonManifolds()}, "
          f"{mesh.CountFacets} facets")
# The assembly, as it sits on the desk, for previews.
assembly = Part.makeCompound([body, bezel, cov])
MeshPart.meshFromShape(Shape=assembly, LinearDeflection=0.08, AngularDeflection=0.2, Relative=False).write(
    os.path.join(HERE, "stl", "assembly-preview.stl"))
print("wrote", os.path.join(HERE, "stl"))
