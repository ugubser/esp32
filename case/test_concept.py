import unittest
from concept import build, DEPTH


class ConceptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parts = build()

    def test_valid_single_solids(self):
        for name, part in self.parts.items():
            with self.subTest(name=name):
                self.assertTrue(part.val().isValid())
                self.assertEqual(len(part.solids().vals()), 1)

    def test_speaker_clearance_from_pcb_and_case(self):
        speaker = self.parts["speaker"]
        for name in ("pcb", "front", "rear", "screen"):
            self.assertLess(speaker.intersect(self.parts[name]).val().Volume(), 0.001)
        # Speaker is entirely below the PCB with 3 mm gap, not behind it.
        self.assertAlmostEqual(self.parts["pcb"].val().BoundingBox().ymin - speaker.val().BoundingBox().ymax,3)
        self.assertLess(speaker.val().BoundingBox().zmax, DEPTH-2)

    def test_case_parts_do_not_overlap(self):
        self.assertLess(self.parts["front"].intersect(self.parts["rear"]).val().Volume(), 0.001)

    def test_back_is_continuous_for_adhesive_strips(self):
        import cadquery as cq
        # No vents, hook recess or screws interrupt either adhesive strip land.
        for x in (-50,50):
            land=cq.Workplane("XY").box(15,70,1).translate((x,0,17.5))
            self.assertAlmostEqual(land.intersect(self.parts["rear"]).val().Volume(),1050,places=3)

    def test_envelope_and_pcb_gauge_clearance(self):
        box=self.parts["rear"].val().BoundingBox()
        self.assertAlmostEqual(box.xlen,143)
        self.assertAlmostEqual(box.ylen,124)
        self.assertAlmostEqual(box.zmax,18)
        self.assertLessEqual(box.zmax - 14,4)
        gauge=self.parts["gauge"]
        pcb=self.parts["pcb"].translate((0,0,-8))
        self.assertLess(gauge.intersect(pcb).val().Volume(),0.001)

    def test_device_depth_and_speaker_front_clearance(self):
        import cadquery as cq
        # The supplied 14 mm padded allowance starts at the recessed glass face.
        envelope=cq.Workplane("XY").box(137,84,6.4,centered=(True,True,False)).translate((0,0,9.6))
        self.assertLess(envelope.intersect(self.parts["rear"]).val().Volume(),0.001)
        cone_gap=cq.Workplane("XY").box(34,24,1.8,centered=(True,True,False)).translate((0,-60,2.1))
        self.assertLess(cone_gap.intersect(self.parts["front"]).val().Volume(),0.001)

    def test_front_grille_opens_into_speaker_space(self):
        import cadquery as cq
        for x in range(-16,17,4):
            path=cq.Workplane("XY").box(2,16,4,centered=(True,True,False)).translate((x,-60,0))
            self.assertLess(path.intersect(self.parts["front"]).val().Volume(),0.001)
        # The bars between slots remain solid, with a 2 mm front thickness.
        bar=cq.Workplane("XY").box(1.2,16,2,centered=(True,True,False)).translate((2,-60,0))
        self.assertAlmostEqual(bar.intersect(self.parts["front"]).val().Volume(),38.4)

    def test_full_width_bay_and_cable_route_are_open(self):
        import cadquery as cq
        # A full-width empty volume verifies there is no holder or divider.
        bay=cq.Workplane("XY").box(132,29,7,centered=(True,True,False)).translate((0,-60,8.5))
        self.assertLess(bay.intersect(self.parts["rear"]).val().Volume(),0.001)
        # 6 x 6 mm clear cable route from connector side down and across to
        # the speaker's wire exit. This is space, not extra retaining walls.
        across=cq.Workplane("XY").box(40,6,6).translate((-43,-60,12))
        down=cq.Workplane("XY").box(6,24,6).translate((-60,-49,12))
        route=across.union(down)
        for name in ("front","rear","speaker"):
            self.assertLess(route.intersect(self.parts[name]).val().Volume(),0.001)

    def test_lower_bay_is_full_case_width(self):
        import cadquery as cq
        # Flat outside front strips run the full width beside the grille.
        for x in (-60,60):
            land=cq.Workplane("XY").box(10,20,2,centered=(True,True,False)).translate((x,-60,0))
            self.assertAlmostEqual(land.intersect(self.parts["front"]).val().Volume(),400)


if __name__ == "__main__":
    unittest.main()
