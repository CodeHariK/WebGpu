"""The cast: Pip (the walk-cycle character, built by build_walker as-is) and the Grunt, a bigger,
meaner body of spheres, cylinders and cones on the same skeleton (red eyes, horns, spiked shoulders)."""
import build_walker
from build_walker import _cylinder as cyl, _sphere as sph
from finisher_rig import make_rig, palette_material, part, place

GRUNT_COLORS = {
    "skin": (0.42, 0.36, 0.55), "vest": (0.2, 0.17, 0.26), "pants": (0.13, 0.11, 0.16),
    "boot": (0.08, 0.07, 0.09), "wrap": (0.6, 0.12, 0.14), "horn": (0.9, 0.86, 0.75),
    "eye": (1.0, 0.15, 0.1), "brow": (0.25, 0.2, 0.32),
}
GRUNT_SCALE = 1.2


def make_pip(location, turn=0.0):
    arm = build_walker.make_armature()
    build_walker.make_body(arm)
    place(arm, location, turn)
    return arm


def make_grunt(location, turn=0.0):
    arm = make_rig("Grunt")

    def mat(name, glow=0.0):
        return palette_material("Grunt", name, GRUNT_COLORS[name], glow)

    def add(name, bone, material, build):
        part(arm, f"Grunt.{name}", bone, material, build)

    add("Pants", "hips", mat("pants"), lambda bm: (cyl(bm, (0, 0, 0.76), (0, 0, 0.98), 0.2, 0.21), sph(bm, (0, 0, 0.8), 0.2, (1, 0.9, 0.6))))
    add("Torso", "spine", mat("vest"), lambda bm: (cyl(bm, (0, 0, 0.95), (0, 0, 1.25), 0.22, 0.24), sph(bm, (0, -0.03, 1.1), 0.25, (1.1, 0.95, 1))))
    add("Head", "head", mat("skin"), lambda bm: (sph(bm, (0, -0.02, 1.48), 0.24, (1.05, 1, 0.92)), sph(bm, (0, -0.12, 1.4), 0.14, (1.3, 1, 0.7))))
    add("Brow", "head", mat("brow"), lambda bm: cyl(bm, (-0.15, -0.23, 1.56), (0.15, -0.23, 1.56), 0.045, 0.045))
    add("Eyes", "head", mat("eye", 4.0), lambda bm: [sph(bm, (x, -0.235, 1.5), 0.035) for x in (0.08, -0.08)])
    add("Horns", "head", mat("horn"), lambda bm: [cyl(bm, (0.13 * x, -0.02, 1.64), (0.26 * x, 0.03, 1.86), 0.05, 0.008, 10) for x in (1, -1)])
    for side, x in (("L", 1), ("R", -1)):
        add(f"Shoulder.{side}", f"upperarm.{side}", mat("vest"), lambda bm, x=x: (sph(bm, (0.27 * x, 0, 1.24), 0.12), cyl(bm, (0.3 * x, 0, 1.32), (0.36 * x, 0, 1.46), 0.04, 0.006, 8)))
        add(f"UpperArm.{side}", f"upperarm.{side}", mat("skin"), lambda bm, x=x: cyl(bm, (0.25 * x, 0, 1.2), (0.29 * x, 0, 0.98), 0.085, 0.075))
        add(f"Forearm.{side}", f"forearm.{side}", mat("skin"), lambda bm, x=x: cyl(bm, (0.29 * x, 0, 0.98), (0.31 * x, -0.02, 0.78), 0.08, 0.07))
        add(f"Fist.{side}", f"forearm.{side}", mat("wrap"), lambda bm, x=x: sph(bm, (0.31 * x, -0.02, 0.73), 0.1))
        add(f"Thigh.{side}", f"thigh.{side}", mat("pants"), lambda bm, x=x: (cyl(bm, (0.12 * x, 0, 0.84), (0.12 * x, 0, 0.47), 0.1, 0.085), sph(bm, (0.12 * x, 0, 0.47), 0.085)))
        add(f"Shin.{side}", f"shin.{side}", mat("pants"), lambda bm, x=x: cyl(bm, (0.12 * x, 0, 0.47), (0.12 * x, 0, 0.12), 0.08, 0.07))
        add(f"Boot.{side}", f"foot.{side}", mat("boot"), lambda bm, x=x: sph(bm, (0.12 * x, -0.05, 0.08), 0.115, (1.1, 1.6, 0.8)))
    place(arm, location, turn, GRUNT_SCALE)
    return arm
