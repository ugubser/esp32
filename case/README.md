# FNK0115Q wall case

Status: slim revision 3 was printed and fitted on 2026-09-13. The owner reports
that the case is nearly perfect and has closed it around the working device.
**Known issue: the USB opening is on the opposite side from the required
orientation. The printed case needed a manual cut-out.** The supplied CAD/STLs
retain that geometry; correct or mirror the opening for your assembly before
printing. Retention and closure are still prototype details. No dimensions
from another display were used.

## Current concept — slim revision 3

- [Preview](slim-v3/case-preview.png)
- [Editable STEP assembly](slim-v3/slim-case.step)
- [Front bezel STL — inspection draft](slim-v3/front-draft.stl)
- [Rear shell STL — inspection draft](slim-v3/rear-draft.stl)
- [PCB outline fit gauge STL](slim-v3/pcb-outline-fit-gauge.stl)
- [Measurements transcribed from the user's annotations](measurements.json)

Outside dimensions: **143 × 124 × 18 mm**, excluding adhesive strips. The
lower speaker space now spans the full 143 mm case width. It shares one open
rear cavity with the board, with no holder, pocket walls or close-fitting
speaker enclosure. The speaker is shown in STEP only as a placement reference.

Board envelope: 137 × 84 mm; screen assembly: 120 × 75 mm; total electronics
depth allowance: 14 mm. Speaker: 40 × 30 × 10 mm, shown 3 mm below the PCB.
Its cone faces forward through nine rounded 2.4 × 20 mm slots in the 2 mm front
skin, with 2 mm nominal cone clearance. The rear and bottom have no vents.
The whole back remains flat for adhesive strips.

The full-width lower cavity is also the cable channel. A 6 × 6 mm clear route
is checked from the connector side down and across to the speaker wire exit;
there is no narrow neck or internal divider. Actual cable length and bend fit
still need checking with the parts. Rotation places the speaker bay at the top.

Depth budget: 2 mm screen recess + 14 mm padded assembly allowance + 2 mm back
wall = 18 mm total, only 4 mm beyond the user's measured padded assembly.
The speaker occupies z=4–14 mm, rather than being stacked behind the board.
Previous exports remain in `concept/` and `slim-v2/` for comparison. Use the
`slim-v3/` files for this revision. Archived source files preserve the earlier
34 mm and narrow-pod designs.

The outline gauge is a 2 mm thick frame with a 138.2 × 85.2 mm inside opening.
It checks the board outline only, not hole placement, connector fit or glass
clearance. Do not force it over connectors or against the glass. Full-case
STLs are provided for visual inspection at the user's request. They share
assembly coordinates and align when imported together using millimetres.
The exports remain prototype designs; the successful fit required the USB
cut-out described above.

Nine CAD tests pass: valid single solids, speaker clearance, front/rear
non-overlap, overall dimensions/gauge clearance, and continuous rear lands
for adhesive strips, device depth and cone clearance, front grille openings,
full-width lower space, and an unobstructed cable route. Both new
STL meshes have zero open or non-manifold edges. These tests do not establish
physical fit or load capacity.

## Requested design

- Enclose the Freenove FNK0115Q 5-inch IPS touch display.
- Flat back for adhesive strips, per the user's 2026-09-13 revision. No hook.
- Support the circuit board at its mounting points, without loading the LCD
  glass, ribbon cable or touch panel. Final retention geometry depends on the
  mounting-hole measurements and clearances.
- Include the supplied loose speaker in the full-width lower space, with no
  dedicated holder, as requested. Its cone faces front vents. Provide open
  cable space back to the board's Speak connector.
- Leave access to USB-C power. Keep reset/boot and card access serviceable.
- The System menu's 0/180-degree setting allows either landscape orientation.
  USB clearance is currently an 18 × 8 mm provisional side opening; the
  user's straight plug must be checked against this opening before printing.

## Source inspection on 2026-09-12

Inspected the user-provided Downloads/Freenove_ESP32_S3_Display_FNK0115-main
folder, including text extraction of all 16 PDFs and visual inspection of
the relevant drawing and parts-list pages.

- `Schematic/5.0inch_ESP32-S3_IPS.pdf`: one-page electrical schematic. Includes
  the NS4168 audio amplifier and two-pin P7 speaker connection, but no board
  outline dimensions, mounting-hole pitch, connector positions or stack height.
- `Datasheet/FNK0115Q/`: CH340C USB chip and NS4168 amplifier datasheets only.
  Chip-package dimensions are not case dimensions.
- `Datasheet/FNK0115N/SPEC-4300J-V03 .pdf` and the FNK0115B LCD datasheet:
  dimensional drawings of a 4.3-inch 480×272 LCD, not this 5-inch IPS board.
  The FNK0115N folder placement is misleading; the drawing itself says 4.3-inch.
- `C_Tutorial.pdf`, page 8: shows the supplied speaker in a rounded rectangular
  housing and identifies it as included with 5-inch versions. No speaker sizes.
- `Picture/FNK0115Q_Top.png` and `FNK0115Q_Bottom.png`: useful component layout
  photographs without a metric scale or dimensional tolerances.

These resources cannot establish a printable fit by themselves. Extracted text
is stored in the ignored `logs/case-inspection/` directory.

Also checked the online documentation's model overview, parts list and hardware
preface. The overview explains that the advertised inch measurement is the
screen diagonal; it does not give the assembly's external dimensions. No board
or speaker dimensions were found in the text of these pages. The main download
link points to the same Freenove GitHub source archive inspected above.

- [Model overview](https://docs.freenove.com/projects/fnk0115/en/latest/fnk0115/codes/Tutorial/Freenove_ESP32_S3_Display.html)
- [Parts list](https://docs.freenove.com/projects/fnk0115/en/latest/fnk0115/codes/Tutorial/List.html)
- [Hardware preface](https://docs.freenove.com/projects/fnk0115/en/latest/fnk0115/codes/Tutorial/Preface.html)

The user confirmed that the installed 180-degree rotation works.

## Remaining measurements (millimetres)

The annotated images supplied the main board, screen, depth and speaker
measurements. Still verify mounting-hole pitch and diameter, whether the
120 × 75 mm annotation measures the complete glass outline, and USB plug
clearance. Dimensions are treated as approximate; no mounting-hole spacing
has been inferred by scaling photographs. The screen is provisionally centred.

The finished deliverables should include editable parametric CAD, printable
parts, a fit-check part, and assembly/print notes once these measurements are
known. Adhesive suitability depends on the strips, wall surface and assembled
mass; no load rating has been assumed.
