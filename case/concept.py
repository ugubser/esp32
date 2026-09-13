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
OUT = ROOT / "slim-v3"
DEPTH = 18
SPEAKER_Y = -60


def outline(d, z=0):
    # Full-width lower bay allows the short speaker cable to run directly
    # from the connector side, with no separate pod or internal holder.
    return rounded(M["pcb_width"]+6,124,d,z,y=-17,radius=4)


def rounded(w, h, d, z=0, x=0, y=0, radius=2):
    return (cq.Workplane("XY").box(w, h, d, centered=(True, True, False))
            .edges("|Z").fillet(radius).translate((x, y, z)))


def build():
    # z grows from the screen face towards the wall. Service seam at z=8.
    outer_w = M["pcb_width"] + 6
    inner_w, inner_h = M["pcb_width"] + 1.2, M["pcb_height"] + 1.2
    front = outline(8)
    aperture = rounded(M["screen_width"] + 1, M["screen_height"] + 1, 10, -1, radius=0.8)
    front = front.cut(aperture)
    # The speaker cone faces a 2 mm front skin with a 2 mm air gap.
    front = front.cut(rounded(inner_w,33.2,7,2,y=SPEAKER_Y,radius=2))
    # Front-facing slots open directly into the speaker bay through its skin.
    for x in range(-16,17,4):
        front = front.cut(rounded(2.4,20,4,-1,x,SPEAKER_Y,1.1))
    rear = outline(DEPTH-8,8)
    # One continuous cavity: the lower space doubles as a cable channel.
    # There is no divider between the board and the speaker bay.
    rear = rear.cut(rounded(inner_w,119.2,9,7,y=-17,radius=2))
    # USB opening is on viewer-left when the rear photograph is mirrored
    # into a front-view coordinate system. Its plug clearance is provisional.
    usb = cq.Workplane("XY").box(12, 18, 8).translate((-outer_w/2, 0, 12))
    rear = rear.cut(usb)
    # Speaker and electronics occupy adjacent space, never stacked in depth.
    sx, sy = 0, SPEAKER_Y
    # The loose speaker is shown only as an assembly reference. No close-fit
    # enclosure, clips or walls surround it. The rear stays flat and unvented.
    pcb = rounded(M["pcb_width"], M["pcb_height"], 1.6, 8, radius=2)
    screen = rounded(M["screen_width"], M["screen_height"], 6, 2, radius=0.5)
    speaker = rounded(40, 30, 10, 4, sx, sy, 3)
    # A thin inexpensive outline gauge checks the handwritten PCB dimensions.
    gauge = rounded(inner_w+5, inner_h+5, 2, radius=3)
    gauge = gauge.cut(rounded(inner_w, inner_h, 4, -1, radius=2))
    return dict(front=front, rear=rear, pcb=pcb, screen=screen, speaker=speaker, gauge=gauge)


def export():
    OUT.mkdir(exist_ok=True)
    parts = build()
    assembly = cq.Assembly(name="FNK0115Q slim 18mm concept")
    colors = {"front": (0.17,0.18,0.20), "rear": (0.25,0.27,0.30),
              "pcb": (0.9,0.56,0.06), "screen": (0.07,0.09,0.13), "speaker": (0.08,0.08,0.09)}
    for name, color in colors.items():
        assembly.add(parts[name], name=name, color=cq.Color(*color))
    assembly.export(str(OUT / "slim-case.step"))
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
