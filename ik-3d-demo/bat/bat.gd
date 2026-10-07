## A procedural bat (BatRig: LimbChain arms and legs, no skeleton) and how it flies. Everything is second-order springs (SecondOrder):
## the body chases a goal point, yaws toward where it's going, banks into turns, pitches with
## climbs and dives, and bobs up on every downstroke. The wings run off one flap clock:
##   downstroke (clock 0 → ½)  wing straight out, fingers splayed, sweeping top → bottom
##   upstroke   (clock ½ → 1)  wrist pulled in and back, fingers folded, rising again
## and a "hold" pose (glide / dive tuck) and a "wrap" pose (cloak round the body) blend over it.
##
## States (go() to force one; with `auto` it picks hover / swoop / roost every few seconds):
##   fly    loops round the target on a breathing radius, ~4.5 flaps/s
##   hover  holds in front of the target, body tipped up, fast flaps, head locked on it
##   swoop  windup (rises and backs off, flapping hard) → dive (wings tucked, straight through
##          the target) → climb (flaps out and up) → fly
##   roost  approach the perch → hang (flips upside down with a swing, feet grip, wings wrap)
##          → drop (when the target comes near, or after a while: falls, unfurls) → fly
class_name Bat
extends Node3D

signal state_changed(state: String)

const HANG_DROP := 0.21 ## body centre sits this far below the perch when hanging
const FLY_SPRING := Vector3(1.3, 0.6, 1.0) ## f, ζ, r of the body chasing its goal
const DIVE_SPRING := Vector3(2.4, 0.75, 1.3)

@export var target: Node3D ## what it circles, hovers at and swoops through
@export var perch: Node3D ## where it roosts (hangs below this point)
@export var auto := true ## pick hover / swoop / roost by itself while flying
@export var cruise_radius := 1.8
@export var cruise_height := 1.0 ## above the target
@export var wake_distance := 1.4 ## a hanging bat drops off when the target comes this close
@export var max_hang_time := 9.0 ## …or after this long (auto only)

var state := "fly"
var phase := "" ## sub-step of swoop / roost
var rig: BatRig

var _body: Node3D ## orientation + bob; the rig rides on it
var _time := 0.0
var _state_time := 0.0
var _phase_time := 0.0
var _next_pick := 4.0
var _flap := 0.0 ## flap clock, in cycles
var _orbit := 0.0
var _swoop_dir := Vector3.FORWARD
var _swoop_start := Vector3.ZERO
var _swoop_through := Vector3.ZERO
var _last_velocity := Vector3.ZERO
var _move := SecondOrder.new(FLY_SPRING.x, FLY_SPRING.y, FLY_SPRING.z)
var _heading := SecondOrder.new(2.0, 0.8, 0.0, Vector3.FORWARD)
var _pitch := SecondOrder.new(3.0, 0.5, 0.0)
var _bank := SecondOrder.new(2.5, 0.45, 0.0)
var _hang := SecondOrder.new(1.8, 0.3, 0.0) ## 0 flying … 1 upside down; the overshoot is the swing
var _stroke := SecondOrder.new(5.0, 0.8, 0.0) ## x amplitude, y bias (radians), z flaps per second
var _hold := SecondOrder.new(4.0, 0.9, 0.0) ## x weight of the held pose, y its extension, z its elevation
var _wrap := SecondOrder.new(3.0, 0.6, 0.0)
var _look := SecondOrder.new(4.0, 0.6, 0.0)


func _ready() -> void:
	_body = Node3D.new()
	_body.name = "Body"
	add_child(_body)
	rig = BatRig.new()
	rig.name = "BatRig"
	_body.add_child(rig)
	_move.reset(global_position)
	_stroke.reset(Vector3(0.85, 0.1, 4.5))
	go("fly")


## Switch state now ("fly", "hover", "swoop", "roost").
func go(new_state: String) -> void:
	if new_state == "roost" and perch == null:
		new_state = "fly"
	state = new_state
	_state_time = 0.0
	_move.set_parameters(FLY_SPRING.x, FLY_SPRING.y, FLY_SPRING.z)
	match state:
		"fly":
			_next_pick = randf_range(3.0, 6.0)
			_set_phase("")
		"hover":
			_set_phase("")
		"swoop":
			_swoop_start = global_position
			_swoop_dir = _flat(_centre() - global_position, Vector3.FORWARD)
			_set_phase("windup")
		"roost":
			_set_phase("approach")
	state_changed.emit(state)


func _process(delta: float) -> void:
	if delta <= 0.0:
		return
	_time += delta
	_state_time += delta
	_phase_time += delta
	var plan := _run_state()
	_fly_body(delta, plan)
	_flap_wings(delta, plan)
	_place_feet(delta, plan)
	_turn_head(delta, plan)
	rig.solve() # limbs, fingers, membranes — after everything above has moved


# --- states: each returns a plan for this frame ------------------------------------------------
#   goal    where the body heads          face   direction to face (ZERO = along its flight)
#   stroke  amplitude, bias, flaps/s      hold   weight, extension, elevation of a still pose
#   wrap    0..1 wings cloaked            hang   0..1 upside down     nose   extra pitch up
#   look    0..1 head on the target

func _run_state() -> Dictionary:
	match state:
		"hover": return _hover()
		"swoop": return _swoop()
		"roost": return _roost()
	return _fly()


func _fly() -> Dictionary:
	_orbit += get_process_delta_time() * 0.6
	var radius := cruise_radius + 0.45 * sin(_time * 0.7)
	var height := cruise_height + 0.35 * sin(_time * 1.3)
	var goal := _centre() + Vector3(sin(_orbit) * radius, height, cos(_orbit) * radius)
	if auto and _state_time > _next_pick:
		var choices := ["hover", "swoop", "swoop"]
		if perch != null:
			choices.append("roost")
		go(choices.pick_random())
	return _plan(goal, Vector3(0.85, 0.1, 4.5), 0.35)


func _hover() -> Dictionary:
	var centre := _centre()
	var away := _flat(global_position - centre, Vector3.BACK)
	var bob := Vector3(0.12 * sin(_time * 2.1), 0.2 + 0.08 * sin(_time * 3.3), 0.0)
	if _state_time > 3.5:
		go("fly")
	var plan := _plan(centre + away * 0.85 + bob, Vector3(1.0, 0.3, 7.5), 1.0)
	plan.face = centre - global_position
	plan.nose = 0.75
	return plan


func _swoop() -> Dictionary:
	var centre := _centre()
	match phase:
		"windup": # rise and back off, eyes on the target: the anticipation
			if _phase_time > 0.6:
				_swoop_through = centre + _swoop_dir * 1.4 + Vector3(0, -0.15, 0)
				_move.set_parameters(DIVE_SPRING.x, DIVE_SPRING.y, DIVE_SPRING.z)
				_set_phase("dive")
			var plan := _plan(_swoop_start - _swoop_dir * 0.5 + Vector3(0, 0.5, 0), Vector3(1.0, 0.3, 7.0), 1.0)
			plan.face = centre - global_position
			return plan
		"dive": # wings tucked, straight through
			if _phase_time > 1.1 or global_position.distance_to(_swoop_through) < 0.35:
				_move.set_parameters(FLY_SPRING.x, FLY_SPRING.y, FLY_SPRING.z)
				_set_phase("climb")
			var plan := _plan(_swoop_through, Vector3(1.0, 0.3, 7.0), 1.0)
			plan.hold = Vector3(1.0, 0.15, 0.35)
			return plan
	if _phase_time > 0.9: # climb: flap hard out and up
		go("fly")
	return _plan(_swoop_through + _swoop_dir * 0.6 + Vector3(0, 1.4, 0), Vector3(1.0, 0.25, 8.0), 0.3)


func _roost() -> Dictionary:
	var hang_point := perch.global_position - Vector3(0, HANG_DROP, 0)
	match phase:
		"approach":
			if global_position.distance_to(hang_point) < 0.18:
				_set_phase("hang")
			var plan := _plan(hang_point, Vector3(0.95, 0.25, 6.5), 0.0)
			plan.nose = 0.5
			return plan
		"hang":
			var near := _target_distance() < wake_distance
			if _phase_time > 1.5 and (near or (auto and _phase_time > max_hang_time)):
				_set_phase("drop")
			var plan := _plan(hang_point, Vector3(0.6, 0.0, 3.0), 0.0)
			plan.hold = Vector3(1.0, 0.0, -0.3)
			plan.wrap = 1.0
			plan.hang = 1.0
			return plan
	if _phase_time > 0.7: # drop: fall, unfurl, fly off
		go("fly")
	var plan := _plan(hang_point - Vector3(0, 0.7, 0), Vector3(1.0, 0.2, 7.0), 0.5)
	if _phase_time < 0.25:
		plan.hold = Vector3(1.0, 0.6, 0.6)
	return plan


func _plan(goal: Vector3, stroke: Vector3, look: float) -> Dictionary:
	return {
		"goal": goal, "face": Vector3.ZERO, "stroke": stroke, "hold": Vector3(0.0, 0.9, 0.12),
		"wrap": 0.0, "hang": 0.0, "nose": 0.0, "look": look,
	}


func _set_phase(new_phase: String) -> void:
	phase = new_phase
	_phase_time = 0.0


# --- body: position, heading, pitch, bank, the flip upside down, the flap bob ------------------

func _fly_body(delta: float, plan: Dictionary) -> void:
	global_position = _move.update(delta, plan.goal)
	var velocity := _move.velocity
	var acceleration := (velocity - _last_velocity) / delta
	_last_velocity = velocity
	var wanted: Vector3 = _heading.value # keep facing the same way when barely moving (hanging)
	if plan.face != Vector3.ZERO:
		wanted = plan.face
	elif velocity.length() > 0.3:
		wanted = velocity
	var heading := _flat(_heading.update(delta, _flat(wanted, _heading.value)), Vector3.FORWARD)
	var yaw := Basis.looking_at(heading)
	var flat_speed := Vector2(velocity.x, velocity.z).length()
	var climb := clampf(atan2(velocity.y, maxf(flat_speed, 0.5)) * 0.6, -0.7, 0.5)
	var pitch := _pitch.update(delta, Vector3(climb + plan.nose, 0, 0)).x
	var sideways := (yaw.inverse() * acceleration).x
	var roll := _bank.update(delta, Vector3(clampf(-sideways * 0.1, -1.0, 1.0), 0, 0)).x
	var flying := yaw * Basis(Vector3.RIGHT, pitch) * Basis(Vector3.BACK, roll)
	var hang := _hang.update(delta, Vector3(plan.hang, 0, 0)).x
	var weight := clampf(hang, 0.0, 1.0)
	var upside_down := Basis(heading.cross(Vector3.UP), heading, Vector3.UP) # head down, feet up
	var swing := (hang - weight) * 1.2 + 0.06 * sin(_time * 1.4) * weight
	_body.basis = flying.slerp(upside_down, weight) * Basis(Vector3.RIGHT, swing)
	var lift := 0.03 * sin(TAU * _flap) * (1.0 - _hold.value.x) * (1.0 - weight)
	_body.position = Vector3(0, lift, 0)


func _flap_wings(delta: float, plan: Dictionary) -> void:
	var stroke := _stroke.update(delta, plan.stroke)
	var hold := _hold.update(delta, plan.hold)
	var wrap := clampf(_wrap.update(delta, Vector3(plan.wrap, 0, 0)).x, 0.0, 1.0)
	_flap = fmod(_flap + delta * stroke.z, 1.0)
	var held := clampf(hold.x, 0.0, 1.0)
	var extension := lerpf(0.5 + 0.5 * sin(TAU * _flap), hold.y, held)
	var elevation := lerpf(stroke.y + stroke.x * cos(TAU * _flap), hold.z, held)
	var wrist := BatWing.wrist_offset(clampf(extension, 0.0, 1.0), elevation)
	var cloak := Vector3(-0.01, -0.13, -0.02) # wrist tucked under the belly
	rig.pose_wings(wrist.lerp(cloak, wrap), lerpf(extension, 0.0, wrap))


func _place_feet(_delta: float, _plan: Dictionary) -> void:
	var gripping := state == "roost" and phase == "hang" # let go the moment it drops
	var grip := clampf(_hang.value.x, 0.0, 1.0) if gripping else 0.0
	for i in rig.feet.size():
		var side := -1.0 if i == 0 else 1.0
		var hold := perch.global_position + _body.global_basis.x * side * 0.035 if perch != null else Vector3.ZERO
		rig.feet[i].global_position = rig.tucked_foot(i).lerp(hold, grip)


func _turn_head(delta: float, plan: Dictionary) -> void:
	var weight := clampf(_look.update(delta, Vector3(plan.look, 0, 0)).x, 0.0, 1.0)
	var rest := _body.global_basis.orthonormalized()
	var aim := rest
	if target != null:
		var to_target := target.global_position - rig.head.global_position
		if to_target.length() > 0.05 and absf(to_target.normalized().y) < 0.98:
			aim = Basis.looking_at(to_target.normalized(), Vector3.UP)
	rig.head.global_basis = rest.slerp(aim, weight * 0.8)


# --- helpers ------------------------------------------------------------------------------------

func _centre() -> Vector3:
	return target.global_position if target != null else Vector3.ZERO


func _target_distance() -> float:
	return global_position.distance_to(target.global_position) if target != null else INF


static func _flat(v: Vector3, fallback: Vector3) -> Vector3:
	v.y = 0.0
	return v.normalized() if v.length() > 0.05 else fallback
