"""Measured envelope concept, not a finished load-bearing enclosure.

Run with a Python environment containing CadQuery. All output stays here.
Board retention and case closure await final measurements. The back is flat
for adhesive strips. Full-case STL files are drafts for visual inspection.
"""
from pathlib import Path
import json
import cadquery as cq

ROOT = Path(__file__).resolve().parent
M = json.loads((ROOT / "measurements.json").read_text())
OUT = ROOT / "concept"


def rounded(w, h, d, z=0, x=0, y=0, radius=2):
    return (cq.Workplane("XY").box(w, h, d, centered=(True, True, False))
            .edges("|Z").fillet(radius).translate((x, y, z)))


def build():
    # z grows from the screen face towards the wall. Service seam at z=8.
    outer_w, outer_h = M["pcb_width"] + 10, M["pcb_height"] + 10
    inner_w, inner_h = M["pcb_width"] + 1.2, M["pcb_height"] + 1.2
    depth = 34
    front = rounded(outer_w, outer_h, 8, radius=4)
    aperture = rounded(M["screen_width"] + 1, M["screen_height"] + 1, 10, -1, radius=0.8)
    front = front.cut(aperture)
    rear = rounded(outer_w, outer_h, depth - 8, 8, radius=4)
    rear = rear.cut(rounded(inner_w, inner_h, depth - 11, 7, radius=2))
    # USB opening is on viewer-left when the rear photograph is mirrored
    # into a front-view coordinate system. Its plug clearance is provisional.
    usb = cq.Workplane("XY").box(12, 18, 12).translate((-outer_w/2, 0, 15))
    rear = rear.cut(usb)
    # Speaker sits towards the connector side, away from the ESP antenna.
    sx, sy = -36, -17
    pocket = rounded(44.8, 34.8, 12.5, 17.5, sx, sy, 3)
    pocket = pocket.cut(rounded(40.8, 30.8, 14, 16.8, sx, sy, 2.2))
    cable = cq.Workplane("XY").box(10, 6, 8).translate((sx-20, sy, 20))
    rear = rear.union(pocket.cut(cable))
    # Keep the complete outside back plane flat for adhesive strips. Sound
    # leaves via bottom slots; the speaker cone faces the inside cavity.
    for dx in (-14, -7, 0, 7, 14):
        rear = rear.cut(cq.Workplane("XY").box(2.4, 14, 5).translate((sx+dx, -outer_h/2, 25)))
    pcb = rounded(M["pcb_width"], M["pcb_height"], 1.6, 8, radius=2)
    screen = rounded(M["screen_width"], M["screen_height"], 6, 2, radius=0.5)
    speaker = rounded(40, 30, 10, 19, sx, sy, 3)
    # A thin inexpensive outline gauge checks the handwritten PCB dimensions.
    gauge = rounded(inner_w+5, inner_h+5, 2, radius=3)
    gauge = gauge.cut(rounded(inner_w, inner_h, 4, -1, radius=2))
    return dict(front=front, rear=rear, pcb=pcb, screen=screen, speaker=speaker, gauge=gauge)


def export():
    OUT.mkdir(exist_ok=True)
    parts = build()
    assembly = cq.Assembly(name="FNK0115Q envelope concept")
    colors = {"front": (0.17,0.18,0.20), "rear": (0.25,0.27,0.30),
              "pcb": (0.9,0.56,0.06), "screen": (0.07,0.09,0.13), "speaker": (0.08,0.08,0.09)}
    for name, color in colors.items():
        assembly.add(parts[name], name=name, color=cq.Color(*color))
    assembly.export(str(OUT / "envelope-concept.step"))
    cq.exporters.export(parts["gauge"], str(OUT / "pcb-outline-fit-gauge.stl"), tolerance=0.05)
    # Requested inspection exports: preserve common assembly coordinates so
    # both STLs align if imported together. They are not final print designs.
    for name in ("front", "rear"):
        cq.exporters.export(parts[name], str(OUT / f"{name}-draft.stl"),
                            tolerance=0.05, angularTolerance=0.1)
        cq.exporters.export(parts[name], str(OUT / f"{name}.svg"), opt={
            "width":800,"height":520,"projectionDir":(1,-1,-1.3),
            "showHidden":False,"strokeWidth":0.5,"showAxes":False})
    info = {name: {"valid": p.val().isValid(), "solids":len(p.solids().vals()),
                   "volume_mm3":round(p.val().Volume(),1)} for name,p in parts.items()}
    (OUT / "validation.json").write_text(json.dumps(info, indent=2)+"\n")
    print(json.dumps(info, indent=2))


if __name__ == "__main__":
    export()
