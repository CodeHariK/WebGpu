## The side of the grass plateau, as a 2D profile: for s = 0 (foot) … 1 (top edge) it gives
## (how far out from the plateau outline in metres, how high as a fraction of the bank).
## Changing only this function changes the cliff from a straight wall to a slope, a round bulge
## or an overhang — the outline, the beach and the water don't change.
class_name BankProfile
extends RefCounted

enum Style { WALL, SLOPE, ROUND, OVERHANG }

const NAMES := ["straight wall", "slope", "round (spherical)", "overhang"]


## Sample positions s along the profile. A wall only needs its foot and top (plus a tiny bevel
## ring so the top edge catches light); curved profiles need more rings to look round.
static func samples(style: Style) -> PackedFloat32Array:
	match style:
		Style.WALL:
			return PackedFloat32Array([0.0, 0.92, 1.0])
		Style.SLOPE:
			return PackedFloat32Array([0.0, 1.0])
		_:
			return PackedFloat32Array([0.0, 0.15, 0.3, 0.45, 0.6, 0.75, 0.88, 1.0])


## (radial offset in metres, height fraction 0..1) at position s.
static func at(style: Style, s: float) -> Vector2:
	match style:
		Style.WALL:
			return Vector2(-0.06 * smoothstep(0.92, 1.0, s), s)   # vertical, top edge bevelled in a little
		Style.SLOPE:
			return Vector2(0.6 * (1.0 - s), s)                     # leans back 0.6 m (steeper than ~45° so the sand shader paints it as earth)
		Style.ROUND:
			var a := lerpf(-PI / 2.0, PI / 2.0, s)                 # half a circle from foot to top
			return Vector2(0.7 * cos(a) - 0.25 * s, (sin(a) + 1.0) / 2.0)
		Style.OVERHANG:
			return Vector2(0.9 * s * s - 0.2, s)                   # the top juts out over the beach
	return Vector2(0.0, s)


## Hard edges (no smoothing across) at these rings: wall foot and top, slope foot and top, overhang lip.
static func crease_at(style: Style, s: float) -> bool:
	match style:
		Style.WALL:
			return s == 0.0 or s == 0.92
		Style.SLOPE:  # a 2-ring face: without creases its normals blend into the flat beach and top
			return true
		Style.OVERHANG:
			return s == 1.0
	return false
