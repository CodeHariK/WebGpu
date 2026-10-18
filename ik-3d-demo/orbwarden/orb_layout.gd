## The Orb Warden's legs: six long three-bone legs (hip → high knee → ankle → spike foot) hung
## from the underside of a big sphere, stepping in alternating tripods. Built here rather than as
## a SpiderLayout preset so spider/ stays as it was; Spider.layout_override takes it.
##
## Proportions: the sphere is RADIUS across, hips sit HIP_DROP below its equator, the knees rise
## well above the body's middle (a spider-crane look) and the feet land ~4.5 m out, so the
## body rides ~3.6 m up with its belly ~2 m off the ground.
class_name OrbLayout
extends RefCounted

const RADIUS := 1.5 ## sphere radius (m)
const HIP_DROP := 0.55 ## hips this far below the equator
const UPPER := Vector2(1.5, 1.4) ## hip → knee: (out, up)
const LOWER := Vector2(1.4, 2.1) ## knee → ankle: (out, down)
const FOOT := Vector2(0.25, 2.3) ## ankle → foot: (out, down), a long spike
const LEG_RADIUS := 0.13
const LEG_ANGLES := [-35.0, 35.0, -90.0, 90.0, -145.0, 145.0] ## around the body, 0 = ahead
const LEG_GROUPS := [0, 1, 1, 0, 0, 1] ## alternating tripods (with the wave gait off)
## Wave gait: legs come in pairs front to back (l1 r1, l2 r2, l3 r3). Within a pair the sides are
## opposite (half a cycle apart); each pair runs 1/3 of a cycle behind the one in front of it:
##   l1 0     l2 1/3   l3 2/3
##   r1 1/2   r2 5/6   r3 1/6
## So a foot lifts every 1/6 of a cycle: l1, r3, l2, r1, l3, r2 — never two neighbours together.
const WAVE_DUTY := 0.72 ## share of the cycle each foot is planted (swing ≈ 0.28 · cycle)
const WAVE_LIFT := 1.0 ## × step height: big, high steps
const WAVE_PERIOD := Vector2(1.3, 2.4) ## seconds per cycle: slow, heavy


static func make() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = "Orb Warden"
	layout.body_radius = RADIUS
	layout.body_visible = false # OrbWarden draws the sphere and the eye itself
	layout.eyes = false
	layout.leg_color = Color(0.12, 0.13, 0.15)
	layout.motion_lean = 0.04
	layout.body_spring = Vector2(60.0, 11.0) # heavy: sinks and sways on each tripod
	layout.body_noise = Vector3(0.04, 0.03, 0.3)
	layout.wave_duty = WAVE_DUTY
	layout.wave_lift = WAVE_LIFT
	layout.wave_period = WAVE_PERIOD
	var ring := sqrt(RADIUS * RADIUS - HIP_DROP * HIP_DROP) * 0.92 # hip ring radius on the sphere
	for i in LEG_ANGLES.size():
		var side := "left" if LEG_ANGLES[i] < 0.0 else "right"
		var leg := layout._add("%s_%d" % [side, i / 2 + 1], LEG_ANGLES[i], UPPER, LOWER, LEG_GROUPS[i])
		leg.hip = leg.out * ring + Vector3.DOWN * HIP_DROP
		leg.foot = FOOT
		leg.radius = LEG_RADIUS
		leg.lift_scale = 1.0
		leg.knee_yaw = "sine"
		leg.knee_yaw_amount = 0.12
		layout.wave_phases.append(wave_phase(i / 2, leg_side(i)))
	return layout


## When in the cycle (0..1) leg pair `pair` (0 = front) on `side` (0 left, 1 right) steps: each
## pair `pair_delay` of a cycle behind the one in front, the right side `side_offset` after the left.
static func wave_phase(pair: int, side: int, pair_delay := 1.0 / 3.0, side_offset := 0.5) -> float:
	return fposmod(pair * pair_delay + side * side_offset, 1.0)


## Leg `leg`'s pair (0 = front) and side (0 left, 1 right), in make()'s leg order.
static func leg_pair(leg: int) -> int:
	return leg / 2


static func leg_side(leg: int) -> int:
	return 0 if LEG_ANGLES[leg] < 0.0 else 1
