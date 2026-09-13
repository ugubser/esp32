# Transit icons

The tram, train and bus SVGs are copied unchanged from the user's existing
`transit-board/public/` assets. The corresponding white transparent PNGs are
28×28 renders matching the web app's dark-theme CSS colour treatment.
`../transit_icons.h` embeds the same renders as LVGL ARGB8888 (BGRA byte order)
for both firmware and native UI tests. Source paths retain their existing
geometry; the SVG path fill was changed to white only during rasterization.
