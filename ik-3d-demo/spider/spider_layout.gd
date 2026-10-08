## Leg layouts: where each leg attaches, how long its two segments are, which step group it is
## in, and how fast it steps. The rig, gait, body, debug and mood all read a layout, so any
## number of legs in any (asymmetric) arrangement works.
##
## Leg directions are angles around the body: 0° = straight ahead (-Z), +90° = right (+X).
## Legs in the same `group` lift together; groups take turns, so the other groups always hold
## the body up (4 legs: diagonal pairs, 6: alternating tripods, 8: alternating tetrapods).
## A leg with `walks = false` is an arm (a crab's claw): it is in no group, never steps, doesn't
## hold the body up, and its tip rests at `hold` in front of the body (gestures can still grab it).
##
## Optional extras (all off by default, so the spider presets don't use or notice them):
##   free limbs   _add_free(): any hip spot (with height) and any 3D segment directions, plus a
##                `bend` direction the knee/elbow points (a mammal: elbows back, knees forward)
##   run groups   `run_group` per leg, used above `run_speed` (walk diagonally, bound when running)
##   body parts   `body_parts` replaces the plain body sphere (torso, head, shoulders…), with the
##                eyes at `eye_center`
##   robot        `head` (a detached head, optionally on a spring coil), second-order springs for
##                the head's position / yaw / pitch / roll, rigid legs' hips (`hip_spring`), the
##                body's turns and starts (`turn_spring`, `move_spring`); per leg `servo` (snappy
##                overshooting feet), `box_steps` (up, across, down), `rigid` (one bone)
class_name SpiderLayout
extends RefCounted

const PRESET_NAMES: Array[String] = ["4 legs", "6 legs (tripod)", "8 legs", "lopsided (claw + limp)", "crab (sideways)", "monkey (big arms)", "robot (spring head)", "sentry bot (t3ssel8r)", "3-bone legs (machine)"]


class LegDef:
	var name: String
	var hip: Vector3 ## attach point on the body (body space)
	var out: Vector3 ## flat outward direction
	var upper: Vector2 ## hip → knee: (outward, up)
	var lower: Vector2 ## knee → foot: (outward, down) — knee → ankle when the leg has a `foot`
	## Third bone, ankle → foot: (outward, down). Zero = a two-bone leg (every preset but one).
	## A three-bone leg needs SpiderChainRig (Spider switches to it by itself).
	var foot := Vector2.ZERO
	var group: int ## legs in the same group step together
	var step_time_scale := 1.0 ## > 1 = slower steps on this leg (a limp)
	var radius := 0.05 ## visual thickness
	var walks := true ## false = an arm: never steps, its tip rests at `hold`
	var hold := Vector3.ZERO ## arms only: tip position at rest (body space)
	# How this limb walks, relative to the spider's settings (1 / 0 = like every other leg).
	# A monkey's big arms: longer, higher, rounder, quicker swings that shove the body along.
	var stride_scale := 1.0 ## × step_distance: lets the foot lag further, so each step covers more ground
	var lead_scale := 1.0 ## × the velocity lead: lands further ahead (a reaching hand)
	var lift_scale := 1.0 ## × step_height
	var roundness := 0.0 ## 0 = lift-and-place hop … 1 = a round windmill swing: pulls back, over the top, reaches past and plants
	var push := 0.0 ## on planting while moving, shoves the body up (m/s) and tips it away from this end: the limb does the work
	# Free limbs (set by _add_free; zero = use the flat upper/lower + out above, like a spider).
	var upper_free := Vector3.ZERO ## hip → knee in body space, any direction
	var lower_free := Vector3.ZERO ## knee → foot in body space, any direction
	var bend := Vector3.ZERO ## body-space direction the knee/elbow points (pole); zero = up and out
	var run_group := -1 ## step group above the layout's run_speed; -1 = same as `group`
	# Robot limbs (zero / false = off).
	var servo := Vector3.ZERO ## (f, ζ, r) second-order filter on the foot: snaps, overshoots, settles like a servo
	var box_steps := false ## step path: straight up, straight across, straight down (instead of an arc)
	var square := false ## draw the segments as square blocks instead of capsules
	var segment_gap := 0.0 ## (square) metres cut off each block end: pieces float apart, a detached limb
	## One bone instead of two: the whole leg keeps its rest shape (both segments) as one rigid
	## piece that swings at the hip to point at the foot and stretches to reach it — no knee.
	var rigid := false
	## How the leg's plane (and so its knee) yaws about the hip (the hip's yaw joint). Every style
	## but "fixed" starts from the foot's yaw — the plane turns to contain the foot, so the knee
	## is always over the line hip → foot — and adds a swing driven by the gait (± knee_yaw_amount,
	## + = toward the walking direction, fading out when standing still):
	##   "follow"  nothing added: just the geometry (small, ± a few degrees at short strides)
	##   "sine"    swings smoothly forward while the foot is up, sweeps back while it's down
	##   "square"  +amount while up, −amount while down, snapping between (a mechanical joint)
	##   "noise"   a smooth wander (a living leg's small corrections)
	##   "fixed"   never turns: the knee always points its rest way (the old behaviour)
	var knee_yaw := "sine"
	var knee_yaw_amount := 0.2 ## radians (~11°)
	var step_shape := "arc" ## step path, a SpiderLeg.STEP_SHAPES key: arc (smooth), circle, ellipse, square, octagon, trapezoid, triangle, hexagon, sawtooth, stab
	var corner_pause := 0.0 ## (polygon step paths) share of the step spent dead still at each corner
	var max_stretch := 0.15 ## (rigid) at most ±this fraction longer/shorter than at rest; past it the foot falls short (pair with short strides)

	func upper_vec() -> Vector3:
		if upper_free != Vector3.ZERO:
			return upper_free
		return out * upper.x + Vector3.UP * upper.y

	func lower_vec() -> Vector3:
		if lower_free != Vector3.ZERO:
			return lower_free
		return out * lower.x + Vector3.DOWN * lower.y

	func foot_vec() -> Vector3:
		return out * foot.x + Vector3.DOWN * foot.y

	func has_foot() -> bool:
		return foot != Vector2.ZERO

	## Full length of the limb (every segment straight).
	func reach() -> float:
		return upper_vec().length() + lower_vec().length() + foot_vec().length()

	## Where the foot rests (body space) when this leg is relaxed.
	func rest_foot() -> Vector3:
		return hip + upper_vec() + lower_vec() + foot_vec()


var name: String
var legs: Array[LegDef] = []
var body_radius := 0.42
var body_scale := Vector3.ONE ## stretch of the body sphere
var body_color := Color(0.25, 0.22, 0.3)
var leg_color := Color(0.35, 0.3, 0.4)
var sideways := false ## natural walk is sideways (a crab): auto walk strafes instead of going forward
var run_speed := 0.0 ## m/s above which legs step in their `run_group`s; 0 = never
## Replaces the body sphere when not empty. Each part: {"shape": "sphere" | "capsule" | "box",
## "radius" (sphere, capsule), "height" (capsule), "size": Vector3 (box), "at": Vector3,
## "rotation": Vector3 (euler, radians), "scale": Vector3, "color": Color} — shape and its
## size keys required, the rest optional.
var body_parts: Array[Dictionary] = []
var eye_center := Vector3.ZERO ## with body_parts: midpoint between the eyes (body space; head space with a head)
var eye_spacing := 0.13 ## half the distance between the eyes
var body_visible := true ## false = draw no body at all (the legs and head float on their own)
## A detached head floating above the body: {"size": Vector3 (box), "at": Vector3 (rest spot,
## body space), "color": Color, "coil": bool (a spring coil to the body; default true), "neck":
## Vector3 (where the coil leaves the body), "eyes": bool (googly eyes on its face at eye_center;
## default true), "parts": Array (more parts in head space, same format as body_parts — a part
## may also have "emission": Color and "glow": float, e.g. a visor), "gun": Dictionary (a head
## gun: {"barrels": Array of Vector3 mounts in head space, "length", "width", "color"} — fired
## by SpiderGun, barrels point forward)}. Empty = no head.
## SpiderSprings moves it.
var head: Dictionary = {}
# Second-order (f, ζ, r) springs, applied by SpiderSprings. Zero = off.
var head_spring := Vector3.ZERO ## the head's position following its rest spot (r > 1: overshoots, bobs)
var head_yaw_spring := Vector3.ZERO ## head heading: the body's, turned to watch SpiderSprings.look_target (zero = head_spring)
var head_pitch_spring := Vector3.ZERO ## head nod: looking up/down + tipping back when it lags (zero = head_spring)
var head_roll_spring := Vector3.ZERO ## head roll: swaying sideways + banking into fast turns (zero = head_spring)
var hip_spring := Vector3.ZERO ## rigid legs: each leg's start point (hip) chases its spot on the body — legs float after it
var turn_spring := Vector3.ZERO ## body heading vs the walking heading (r < 0: twists the wrong way first)
var move_spring := Vector3.ZERO ## body position vs the walking position (r < 0: rocks back before setting off)
## Bob added on top of the head's springs — or the body's, with no separate head: (amplitude in
## metres, cycles per second). A pure sine up and down; it doesn't feed the springs, so it never
## builds up or fights them.
var head_bob := Vector2.ZERO
## Radians the body leans per m/s of walking: nose down going forward, side down going sideways.
var motion_lean := 0.0
## × the spider's move_speed and auto_turn for this layout (same circle, slower): a slow walker.
var walk_speed := 1.0
## Seconds per gait cycle when the steps are clocked: the step groups take turns on a metronome,
## one half-cycle each, stepping even when standing still — so while one pair is down (minimum)
## the other is at the top of its step (maximum). 0 = the normal gait (step when a foot lags).
var step_period := 0.0
## Clocked steps: share of its half-cycle a step takes; the rest the foot waits planted. Low =
## quick, snappy steps with a hold between them (a machine). Only with step_period.
var step_fill := 0.95
## Extra visual body motion on top of the body spring (zero = off):
var body_sway := Vector2.ZERO ## sine forward/back: (amplitude in metres, cycles per second)
var body_noise := Vector3.ZERO ## smooth noise: (position metres, rotation radians, roughly cycles per second)
var eyes := true ## googly eyes on the body (with body_parts and no head)
## Body noise held and snapped instead of flowing: a fresh value every this many seconds, reached
## almost at once — servo twitches rather than a drift. 0 = smooth noise.
var noise_hold := 0.0
## The shown body heading moves in steps of this many radians (through turn_spring, so each step
## can wind up and snap). 0 = smooth.
var yaw_step := 0.0
## Body spring for this layout: (stiffness, damping); zero = the Spider's body_stiffness / body_damping.
var body_spring := Vector2.ZERO


static func preset(index: int) -> SpiderLayout:
	match index % PRESET_NAMES.size():
		1: return hexa()
		2: return octo()
		3: return lopsided()
		4: return crab()
		5: return monkey()
		6: return robot()
		7: return sentry()
		8: return three_bone()
		_: return quad()


## Indices of the walking legs in each step group, e.g. [[0, 3], [1, 2]]. Arms are left out.
## `running` = the groups used above run_speed (each leg's run_group, or its group if unset).
func groups(running := false) -> Array:
	var result: Array = []
	for leg in legs.size():
		if not legs[leg].walks:
			continue
		var group := legs[leg].group
		if running and legs[leg].run_group >= 0:
			group = legs[leg].run_group
		while result.size() <= group:
			result.append([])
		result[group].append(leg)
	return result


## Body height above the ground when every walking leg is relaxed (average over them).
func relaxed_height() -> float:
	var total := 0.0
	var count := 0
	for leg in legs:
		if leg.walks:
			total -= leg.rest_foot().y
			count += 1
	return total / maxi(count, 1)


## The front-most leg on each side: [left, right] (used for gestures and the threat pose).
func front_legs() -> Array[int]:
	var left := -1
	var right := -1
	for leg in legs.size():
		var hip := legs[leg].hip
		if hip.x < 0.0 and (left < 0 or hip.z < legs[left].hip.z):
			left = leg
		elif hip.x > 0.0 and (right < 0 or hip.z < legs[right].hip.z):
			right = leg
	return [left, right]


## True if some leg has three bones (only SpiderChainRig can draw and solve those).
func needs_chains() -> bool:
	for leg in legs:
		if leg.has_foot():
			return true
	return false


# --- presets ---------------------------------------------------------------------------------

## 4 short legs of three bones each, square to each other (front, right, back, left) — hip up to
## a high knee, out to an ankle, then a thin foot that plants near-vertically (THREE_BONE chains,
## tapering to a point) — under one gunmetal capsule with a red sensor eye: no head. A machine:
##   steps   clocked (front+back and left+right take turns on a metronome), each step a fast
##           square path walked at constant speed, stopping dead at every corner, then a hold
##           (J cycles the shapes)
##   body    a stiff, critically damped body spring (no bounce); its heading turns in 15° steps,
##           each one wound up the wrong way first and snapped (turn_spring, r < 0)
##   twitch  noise held and snapped every 0.18 s: servo micro-corrections, not a drift
static func three_bone() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[8]
	layout.body_radius = 0.24
	layout.body_scale = Vector3(1.0, 1.0, 1.6) # hips spread along the capsule
	layout.body_color = Color(0.16, 0.17, 0.2)
	layout.leg_color = Color(0.42, 0.44, 0.48)
	var red := Color(1.0, 0.08, 0.05)
	layout.body_parts = [
		{"shape": "capsule", "radius": 0.24, "height": 0.95, "rotation": Vector3(PI * 0.5, 0, 0)},
		{"shape": "box", "size": Vector3(0.3, 0.035, 0.06), "at": Vector3(0, 0.07, -0.42), "rotation": Vector3(-0.5, 0, 0), "color": red, "emission": red, "glow": 4.0}, # sensor slit
		{"shape": "sphere", "radius": 0.045, "at": Vector3(0, 0.07, -0.45), "color": red, "emission": red, "glow": 6.0}, # the eye
		{"shape": "box", "size": Vector3(0.5, 0.02, 0.6), "at": Vector3(0, 0.235, 0.05), "color": Color(0.1, 0.1, 0.12)}, # spine plate
	]
	layout.eyes = false
	layout.walk_speed = 0.85
	layout.step_period = 0.4 # a pair steps every 0.2 s: short strides that fit these short legs
	layout.step_fill = 0.6 # each step is a fast 0.12 s snap, then the foot holds
	layout.motion_lean = 0.1
	layout.body_noise = Vector3(0.08, 0.15, 0.4)
	layout.noise_hold = 0.18
	layout.yaw_step = deg_to_rad(15.0)
	layout.turn_spring = Vector3(6.0, 0.75, -1.5) # winds up the wrong way, then snaps to the next step
	layout.body_spring = Vector2(500.0, 45.0) # stiff, just under critical: lands, barely settles
	var upper := Vector2(0.24, 0.15) # out and a little up to the knee (low enough that the body clears the ground)
	var lower := Vector2(0.2, 0.1) # out and a little down to the ankle
	var foot := Vector2(0.025, 0.46) # a thin foot, nearly straight down
	for leg_def: Array in [["front", 0, 0], ["right", 90, 1], ["back", 180, 0], ["left", -90, 1]]:
		var leg := layout._add(leg_def[0], leg_def[1], upper, lower, leg_def[2])
		leg.foot = foot
		leg.radius = 0.032
		leg.square = true # angular metal plates; three-bone legs taper to a point (SpiderChainRig)
		leg.lift_scale = 1.2 # high steps for these short legs (the knee has to lift the hanging foot)
		leg.step_shape = "square"
		leg.corner_pause = 0.2
		leg.knee_yaw = "square" # the leg plane snaps between ±0.3 rad, like a geared hip
		leg.knee_yaw_amount = 0.3
	return layout


## The original: 4 legs at the corners, diagonal pairs.
static func quad() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[0]
	var upper := Vector2(0.55, 0.35)
	var lower := Vector2(0.35, 0.8)
	layout._add("front_left", -45, upper, lower, 0)
	layout._add("front_right", 45, upper, lower, 1)
	layout._add("back_left", -135, upper, lower, 1)
	layout._add("back_right", 135, upper, lower, 0)
	return layout


## 6 legs, alternating tripods (front + back of one side with the middle of the other).
static func hexa() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[1]
	layout.body_scale = Vector3(0.9, 0.9, 1.25)
	var upper := Vector2(0.5, 0.35)
	var lower := Vector2(0.3, 0.8)
	layout._add("left_1", -40, upper, lower, 0)
	layout._add("right_1", 40, upper, lower, 1)
	layout._add("left_2", -90, upper, lower, 1)
	layout._add("right_2", 90, upper, lower, 0)
	layout._add("left_3", -140, upper, lower, 0)
	layout._add("right_3", 140, upper, lower, 1)
	return layout


## 8 legs, alternating tetrapods, with a longer, high-kneed front pair (reads as scarier).
static func octo() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[2]
	layout.body_scale = Vector3(0.95, 0.85, 1.3)
	var upper := Vector2(0.5, 0.35)
	var lower := Vector2(0.3, 0.8)
	var long_upper := Vector2(0.65, 0.5)
	var long_lower := Vector2(0.4, 0.95)
	layout._add("left_1", -30, long_upper, long_lower, 0)
	layout._add("right_1", 30, long_upper, long_lower, 1)
	layout._add("left_2", -70, upper, lower, 1)
	layout._add("right_2", 70, upper, lower, 0)
	layout._add("left_3", -110, upper, lower, 0)
	layout._add("right_3", 110, upper, lower, 1)
	layout._add("left_4", -150, upper, lower, 1)
	layout._add("right_4", 150, upper, lower, 0)
	return layout


## 5 legs, nothing mirrored: a long high-kneed claw leg front-left, a small middle-right leg,
## and a short injured back-left leg that steps slowly (a limp). The body sags toward the short
## leg and rises over the long one on its own — the body pose is fitted to what each leg needs.
static func lopsided() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[3]
	layout.body_scale = Vector3(1.05, 0.85, 1.1)
	var upper := Vector2(0.55, 0.35)
	var lower := Vector2(0.35, 0.8)
	# Claw: much longer reach and a knee 0.6 high, but the same foot drop (0.45) as a normal leg,
	# so it towers without lifting the front. Injured leg: 0.1 shorter drop, so the body sags there.
	layout._add("claw_left", -35, Vector2(0.85, 0.6), Vector2(0.45, 1.05), 0).radius = 0.07
	layout._add("front_right", 45, upper, lower, 1)
	layout._add("middle_right", 95, Vector2(0.42, 0.28), Vector2(0.28, 0.7), 0).radius = 0.04
	var injured := layout._add("injured_back_left", -140, Vector2(0.48, 0.3), Vector2(0.26, 0.65), 1)
	injured.step_time_scale = 1.9
	injured.radius = 0.04
	layout._add("back_right", 135, upper, lower, 0)
	return layout


## A crab: a wide, flat shell, 8 spiky walking legs pointing out to the sides (±60…±135°) and two
## claws in front that never walk. Its legs reach sideways, so it walks sideways (`sideways`).
static func crab() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[4]
	layout.body_scale = Vector3(1.35, 0.55, 0.85)
	layout.body_color = Color(0.8, 0.32, 0.18)
	layout.leg_color = Color(0.9, 0.45, 0.25)
	layout.sideways = true
	var upper := Vector2(0.45, 0.4) # high, spiky knees
	var lower := Vector2(0.25, 0.75)
	var angles := [60, 85, 110, 135]
	for i in angles.size():
		var group := i % 2
		layout._add("left_%d" % (i + 1), -angles[i], upper, lower, group)
		layout._add("right_%d" % (i + 1), angles[i], upper, lower, 1 - group)
	for side: int in [-1, 1]:
		var claw := layout._add("claw_left" if side < 0 else "claw_right", 25 * side, Vector2(0.35, 0.2), Vector2(0.3, 0.3), -1)
		claw.walks = false
		claw.radius = 0.08
		claw.hold = claw.hip + Vector3(claw.out.x * 0.25, 0.0, -0.68) # held out in front, nearly straight
	return layout


## A gorilla knuckle-walker, built like a mammal rather than a spider:
##   body    torso capsule + chest bulk + shoulder balls + head with a muzzle; eyes on the head
##   arms    from wide, high shoulders at the front, swinging front-to-back under them; elbows
##           point back. Long: the shoulders ride higher than the hips (chest up ~10°).
##   legs    from narrow, low hips at the back; knees point forward
##   gait    walking: diagonal pairs (left hand + right foot, then the other two);
##           above run_speed: a bound (both hands, then both feet)
##   arms do the work: long, round, quick windmill swings that land well ahead and shove the
##   chest up as they plant; the legs take short flat steps.
## Limbs keep a good bend at rest (arm ~78% of its length, leg ~75%) so a hand can still reach
## forward along the ground — a straight limb can't, and a nearly straight one keeps hitting
## the spider's overreach_step and stepping out of turn.
static func monkey() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[5]
	layout.body_color = Color(0.3, 0.22, 0.17)
	layout.leg_color = Color(0.36, 0.26, 0.2)
	layout.run_speed = 2.4
	var face := Color(0.55, 0.42, 0.33)
	layout.body_parts = [
		{"shape": "capsule", "radius": 0.3, "height": 1.05, "at": Vector3(0, 0, 0.02), "rotation": Vector3(PI / 2, 0, 0), "scale": Vector3(1.15, 1, 1)},
		{"shape": "sphere", "radius": 0.36, "at": Vector3(0, 0.06, -0.25), "scale": Vector3(1.15, 1, 0.9)}, # chest
		{"shape": "sphere", "radius": 0.16, "at": Vector3(-0.36, 0.12, -0.32)}, # shoulders
		{"shape": "sphere", "radius": 0.16, "at": Vector3(0.36, 0.12, -0.32)},
		{"shape": "sphere", "radius": 0.21, "at": Vector3(0, 0.2, -0.62)}, # head
		{"shape": "sphere", "radius": 0.12, "at": Vector3(0, 0.11, -0.78), "scale": Vector3(1.2, 0.85, 1), "color": face}, # muzzle
	]
	layout.eye_center = Vector3(0, 0.27, -0.77)
	layout.eye_spacing = 0.08
	for side: int in [-1, 1]:
		var arm := layout._add_free("arm_left" if side < 0 else "arm_right",
				Vector3(0.38 * side, 0.12, -0.32), # shoulder
				Vector3(0.06 * side, -0.42, 0.34), # → elbow: down and back
				Vector3(0.0, -0.47, -0.38), # → knuckles: down and forward, under the shoulder
				Vector3(0.35 * side, 0.0, 1.0), # elbow points back (a bit out)
				0 if side < 0 else 1)
		arm.run_group = 0
		arm.radius = 0.11
		arm.stride_scale = 1.5
		arm.lead_scale = 1.0
		arm.lift_scale = 1.3
		arm.roundness = 1.0
		arm.step_time_scale = 1.15
		arm.push = 0.6
	for side: int in [-1, 1]:
		var leg := layout._add_free("leg_left" if side < 0 else "leg_right",
				Vector3(0.2 * side, -0.12, 0.38), # hip
				Vector3(0.06 * side, -0.18, -0.3), # → knee: down and forward
				Vector3(0.03 * side, -0.4, 0.18), # → foot: down and back
				Vector3(0.3 * side, 0.0, -1.0), # knee points forward (a bit out)
				1 if side < 0 else 0) # diagonal: pairs with the opposite hand
		leg.run_group = 1
		leg.radius = 0.08
		leg.lift_scale = 0.7
		leg.push = 0.15
	return layout


## A cute spider robot, all second-order springs (SecondOrder, via SpiderSprings):
##   head    detached, floating on a spring coil above the front of a boxy chassis; bobs,
##           overshoots and tilts into every move (head_spring r > 1). Its eyes track the target.
##   turns   anticipated: the chassis first twists the wrong way, then swings round past the
##           heading and settles (turn_spring r < 0)
##   starts  the chassis rocks back before setting off and lurches on stopping (move_spring r < 0)
##   legs    servo feet: boxy steps (straight up, across, straight down) through a fast, springy
##           filter, so each foot snaps to its spot, overshoots a little and settles — mechanical.
static func robot() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[6]
	layout.body_scale = Vector3(0.95, 1.0, 1.05)
	layout.body_color = Color(0.5, 0.56, 0.66)
	layout.leg_color = Color(0.3, 0.33, 0.4)
	var dark := Color(0.22, 0.24, 0.3)
	layout.body_parts = [
		{"shape": "box", "size": Vector3(0.75, 0.24, 0.85)}, # chassis
		{"shape": "box", "size": Vector3(0.52, 0.07, 0.58), "at": Vector3(0, 0.15, 0.06), "color": dark}, # top plate
		{"shape": "sphere", "radius": 0.08, "at": Vector3(0, 0.17, -0.2), "color": dark}, # neck socket
	]
	layout.head = {"size": Vector3(0.36, 0.26, 0.3), "at": Vector3(0, 0.62, -0.2), "neck": Vector3(0, 0.18, -0.2), "color": Color(0.86, 0.89, 0.93)}
	layout.eye_center = Vector3(0, 0.02, -0.16) # on the head's face
	layout.eye_spacing = 0.085
	layout.head_spring = Vector3(2.2, 0.35, 1.8)
	# Turn/move springs also trail the motion while it lasts, by (k1 − k3) × speed: ~0.13 rad per
	# rad/s of turning (0.32 in a fast spin), ~11 cm at walking pace. Raise f to trail less; make
	# r more negative for a bigger wind-up.
	layout.turn_spring = Vector3(2.4, 0.55, -1.5)
	layout.move_spring = Vector3(3.0, 0.5, -0.6)
	var upper := Vector2(0.4, 0.42) # tall, angular knees
	var lower := Vector2(0.22, 0.78)
	for leg_def: Array in [["front_left", -45, 0], ["front_right", 45, 1], ["back_left", -135, 1], ["back_right", 135, 0]]:
		var leg := layout._add(leg_def[0], leg_def[1], upper, lower, leg_def[2])
		leg.radius = 0.035
		leg.servo = Vector3(5.0, 0.4, 2.0)
		leg.box_steps = true
		leg.step_time_scale = 1.2
	return layout


## t3ssel8r-style sentry bot: no body at all — a big cube head floating on a spring where the
## body would be, and four blocky one-bone legs hanging in the air below it in separate pieces
## (each leg a rigid out-block + down-block that swings at the hip and stretches to the foot).
## No eyes: a glowing red square in a dark frame, black vent slits either side.
## Scary-fast: the head snaps round to watch the target with a quick wind-up and a little wobble
## (head_yaw_spring, r < 0) while its roll stays loose; each leg hangs from a sprung hip point
## (hip_spring), turns are anticipated, and the servo feet snap into place.
static func sentry() -> SpiderLayout:
	var layout := SpiderLayout.new()
	layout.name = PRESET_NAMES[7]
	layout.body_visible = false
	layout.body_radius = 0.3
	var pale := Color(0.8, 0.84, 0.92)
	var dark := Color(0.08, 0.08, 0.1)
	layout.leg_color = pale.darkened(0.06)
	layout.head = {
		"size": Vector3(0.5, 0.42, 0.46), "at": Vector3(0, 0.32, 0), "color": pale,
		"coil": false, "eyes": false,
		"parts": [
			{"shape": "box", "size": Vector3(0.21, 0.21, 0.02), "at": Vector3(0, 0, -0.235), "color": dark}, # visor frame
			{"shape": "box", "size": Vector3(0.14, 0.14, 0.02), "at": Vector3(0, 0, -0.243), "color": Color(0.9, 0.05, 0.05), "emission": Color(1, 0.05, 0.02), "glow": 2.5}, # red eye
			{"shape": "box", "size": Vector3(0.11, 0.025, 0.02), "at": Vector3(-0.175, 0.045, -0.235), "color": dark}, # vent slits
			{"shape": "box", "size": Vector3(0.11, 0.025, 0.02), "at": Vector3(-0.175, -0.035, -0.235), "color": dark},
			{"shape": "box", "size": Vector3(0.11, 0.025, 0.02), "at": Vector3(0.175, 0.045, -0.235), "color": dark},
			{"shape": "box", "size": Vector3(0.11, 0.025, 0.02), "at": Vector3(0.175, -0.035, -0.235), "color": dark},
			{"shape": "box", "size": Vector3(0.09, 0.16, 0.26), "at": Vector3(-0.29, 0.04, 0.03)}, # side ears
			{"shape": "box", "size": Vector3(0.09, 0.16, 0.26), "at": Vector3(0.29, 0.04, 0.03)},
			{"shape": "box", "size": Vector3(0.26, 0.08, 0.28), "at": Vector3(0, 0.25, 0.04)}, # top block
			{"shape": "box", "size": Vector3(0.3, 0.09, 0.12), "at": Vector3(0, -0.19, -0.2), "color": dark}, # gun housing
		],
		# Twin machine-gun barrels under the eye, firing alternately (SpiderGun).
		"gun": {"barrels": [Vector3(-0.09, -0.19, -0.26), Vector3(0.09, -0.19, -0.26)], "length": 0.3, "width": 0.05, "color": dark},
	}
	layout.head_spring = Vector3(3.0, 0.55, 1.2) # position: floats close, settles quickly
	# Yaw snaps round: a beat of wind-up, ~0.15 s to arrive, a few degrees of overshoot. (A wind-up
	# only shows when r·ζ/(2πf) × the target's turn speed beats the turn itself; r ≈ −1 is where
	# it starts for a fast-moving target.)
	layout.head_yaw_spring = Vector3(4.0, 0.45, -1.0)
	layout.head_pitch_spring = Vector3(3.5, 0.4, 0.8) # quick nod, small overshoot
	layout.head_roll_spring = Vector3(2.2, 0.35, 1.0) # loose and wobbly: the floaty part
	layout.hip_spring = Vector3(2.5, 0.35, 0.5) # leg start points trail the body ~7 cm and bounce on stops
	layout.turn_spring = Vector3(3.5, 0.6, -1.2)
	layout.move_spring = Vector3(3.5, 0.6, -0.5)
	var upper := Vector2(0.34, 0.2) # out and a little up from under the head…
	var lower := Vector2(0.12, 0.62) # …then down
	for leg_def: Array in [["front_left", -45, 0], ["front_right", 45, 1], ["back_left", -135, 1], ["back_right", 135, 0]]:
		var leg := layout._add(leg_def[0], leg_def[1], upper, lower, leg_def[2])
		leg.radius = 0.065 # half-width of the square blocks
		leg.square = true
		leg.segment_gap = 0.06
		leg.rigid = true # one bone: the out-block and the down-block swing together, no knee
		leg.max_stretch = 0.12
		leg.stride_scale = 0.6 # a knee-less leg can't reach far: short, quick, mechanical steps
		leg.lead_scale = 0.6
		leg.servo = Vector3(6.0, 0.5, 1.5)
		leg.box_steps = true
	return layout


# A mammal-style limb: hip anywhere on the body (with height), segments in any direction, and
# the knee/elbow bending toward `bend`. All vectors in body space.
func _add_free(leg_name: String, hip: Vector3, upper_segment: Vector3, lower_segment: Vector3, bend_direction: Vector3, group: int) -> LegDef:
	var leg := LegDef.new()
	leg.name = leg_name
	leg.hip = hip
	leg.out = Vector3(hip.x, 0.0, hip.z).normalized()
	leg.upper_free = upper_segment
	leg.lower_free = lower_segment
	leg.bend = bend_direction.normalized()
	leg.group = group
	legs.append(leg)
	return leg


# A leg pointing `angle_degrees` around the body (0 = forward, +90 = right), hip on the body's rim.
func _add(leg_name: String, angle_degrees: float, upper: Vector2, lower: Vector2, group: int) -> LegDef:
	var angle := deg_to_rad(angle_degrees)
	var direction := Vector3(sin(angle), 0.0, -cos(angle))
	var leg := LegDef.new()
	leg.name = leg_name
	leg.out = direction
	leg.hip = direction * body_radius * Vector3(body_scale.x, 0.0, body_scale.z) * 0.95
	leg.upper = upper
	leg.lower = lower
	leg.group = group
	legs.append(leg)
	return leg
