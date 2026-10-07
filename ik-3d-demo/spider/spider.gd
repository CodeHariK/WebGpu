## A many-legged creature walking purely by IK: no animation at all. The legs come from a
## SpiderLayout (4, 6, 8, a lopsided 5 or a crab), so nothing here assumes a leg count or symmetry.
##
## Every physics frame:
##   1. move   — walk in any direction (forward and/or sideways) + turn: a circle on its own,
##               arrow keys + A/D to strafe, or steered by SpiderBrain's behaviours.
##               launch() jumps: the root flies, feet tuck and reach down, it lands by itself.
##   2. ground — the spider's root follows the ground height under it (raycast)
##   3. gait   — each foot stays planted; when it lags too far behind its home spot it steps.
##               Legs step in groups (from the layout) that take turns, so the other groups
##               always hold the body up.
##   4. body   — each leg says how high the body should be for it to stay relaxed; a plane fitted
##               through those heights gives the body's height, pitch and roll. Uneven ground,
##               and uneven legs, tilt the body by themselves. Then a spring adds weight.
## The legs themselves are bent by SpiderRig's TwoBoneIK3D toward the foot targets — or, with
## `chain_legs`, by SpiderChainRig's LimbChains (custom IK, no leg bones; see limbs/).
##
## Node layout:  Spider (this; moves + turns, never tilts)
##               └ Body (height + tilt) └ Skeleton3D (+ visuals, IK) and the knee poles
##               foot targets are top_level, so they stay in world space while planted.
class_name Spider
extends Node3D

signal rebuilt ## the legs were rebuilt (layout changed): leg indices are new
signal landed ## touched down after launch()

@export_enum("4 legs", "6 legs (tripod)", "8 legs", "lopsided (claw + limp)", "crab (sideways)", "monkey (big arms)", "robot (spring head)", "sentry bot (t3ssel8r)") var layout_preset := 0:
	set(value):
		layout_preset = value
		if is_inside_tree():
			_build()

## Legs as LimbChains (custom IK, one MultiMesh renderer) instead of Skeleton3D bones +
## TwoBoneIK3D + BoneAttachment3Ds (off). Same gait, same look; rebuilds the spider.
@export var chain_legs := true:
	set(value):
		chain_legs = value
		if is_inside_tree():
			_build()

@export_group("Movement")
@export var auto_walk := true ## walk in a circle by itself; off = arrow keys
@export var move_speed := 1.6 ## m/s
@export var turn_speed := 1.4 ## rad/s with the arrow keys
@export var auto_turn := 0.35 ## rad/s when auto walking (circle radius = speed / turn)
@export var walk_sideways := false ## auto walk strafes right instead of going forward (always on for a sideways layout like the crab)

@export_group("Gait")
@export var use_gait := true ## off = feet glued to their home spots (they slide)
@export var step_distance := 0.35 ## how far a foot may lag before it steps
@export var step_time := 0.18 ## seconds per step (each leg can scale it: a limp)
@export var step_height := 0.22 ## arc height
@export var lead_time := 0.25 ## home spot sits this many seconds of velocity ahead
@export var max_lead := 0.45 ## ...but never more than this many metres (else a sudden stop leaves feet out of reach)
@export var fast_step_time := 0.07 ## shortest step: when moving fast, steps get quicker (down to this) so feet keep up
@export_range(0.5, 1.0) var overreach_step := 0.92 ## a planted foot further than this × its reach from its hip steps at once, out of turn

@export_group("Jump")
@export var jump_gravity := 16.0 ## m/s² pulling the spider down while airborne

@export_group("Ground")
## Physics layers the ground raycasts hit. Keep loose things (explosion debris, props) off
## these layers, or feet will try to stand on them.
@export_flags_3d_physics var ground_mask := 1

@export_group("Body")
@export var follow_feet := true ## body height + tilt follow the feet
@export var ride_height := 0.0 ## extra height above "legs relaxed" (negative = crouch)
@export var body_stiffness := 140.0 ## body spring: how hard it pulls to its target pose
@export var body_damping := 16.0 ## body spring: lower = more bounce / overshoot
@export var lean := 0.05 ## lean into speed changes and turns; 0 = none
@export var use_knee_poles := true:
	set(value):
		use_knee_poles = value
		if rig != null:
			rig.set_poles_enabled(value)
@export var show_targets := true:
	set(value):
		show_targets = value
		for marker in _target_markers:
			marker.visible = value

var rig: SpiderRig
var legs: Array[SpiderLeg] = []
var velocity := Vector3.ZERO

# Hooks for a behaviour layer (SpiderMood). Neutral values = plain walking.
var speed_scale := 1.0 ## multiplies walking speed and auto-turn; 0 = stand still
var extra_turn := 0.0 ## rad/s added on top of the walking turn (e.g. to face something)
var body_height_offset := 0.0 ## added to the body's target height
var body_pitch_offset := 0.0 ## added to the body's target pitch (+ = nose up)
var body_roll_offset := 0.0 ## added to the body's target roll (+ = right side up)
# Purely visual body offsets (SpiderSprings): the body swings away from where the spider really
# is and faces; the gait, homes and ground following don't move. The legs' IK absorbs it.
var body_yaw_offset := 0.0 ## radians the body is twisted from the walking heading
var body_offset := Vector3.ZERO ## metres the body is shifted (x, z in the spider's own frame)

# Steering hooks for a behaviour that moves the spider itself (SpiderBrain). While `steering` is
# on, auto walk and the keys are ignored.
var steering := false
var steer := Vector2.ZERO ## (right, forward) in units of move_speed; still scaled by speed_scale
var steer_turn := 0.0 ## rad/s; not scaled by speed_scale, so it keeps facing even when frozen
var action_pose := Vector3.ZERO ## (height, pitch, roll) added by the current behaviour: crouch, recoil, squash

## In the air after launch(): the root flies under jump_gravity, feet tuck in and reach down
## for the landing, the body stops following the feet. Lands by itself on the ground below.
var airborne := false
var air_splay := 0.0 ## in the air: 0 = tuck and reach down … 1 = every leg stretched straight out and forward (a dive)

# Last frame's gait data, read by SpiderDebug.
var homes: Array[Vector3] = [] ## each foot's home spot (on the ground)
var home_rays: Array[Dictionary] = [] ## the raycast that found each home: {from, to, hit}
var ground_ray: Dictionary = {} ## the raycast under the spider's root

var _body: Node3D
var _groups: Array = [] ## leg indices per step group (the walk or run set, see _choose_groups)
var _walk_groups: Array = []
var _run_groups: Array = []
var _running := false
var _last_position: Vector3
var _next_group := 0
var _planted_once := false
var _target_markers: Array[MeshInstance3D] = []
var _forward_accel := 0.0 ## m/s² along the facing direction (body pitches with it)
var _side_accel := 0.0 ## m/s² to the right: strafing changes and turns (body rolls with it)
var _body_pose := Vector3.ZERO ## spring state: (height, pitch, roll)
var _body_pose_velocity := Vector3.ZERO
var _vertical_speed := 0.0 ## m/s up while airborne
var _turn_rate := 0.0 ## rad/s turned this frame
var _foot_radius := 1.0 ## how far the furthest relaxed foot is from the centre (spinning moves it at ω · this)


func _ready() -> void:
	_build()
	_last_position = global_position


func _physics_process(delta: float) -> void:
	_move(delta)
	_follow_ground(delta)
	_update_homes()
	if not _planted_once: # first frame: put every foot on the ground under its home spot
		for leg in legs.size():
			if rig.layout.legs[leg].walks:
				legs[leg].snap_to(homes[leg])
		_planted_once = true
	_update_gait(delta)
	_update_body(delta)
	rig.update_rigid_limbs() # one-bone legs aim at their feet (no-op for jointed legs)


## (Re)build body, skeleton, legs and IK from the current layout preset.
func _build() -> void:
	if _body != null:
		remove_child(_body)
		_body.queue_free()
	legs.clear()
	_target_markers.clear()
	homes.clear()
	home_rays.clear()
	var layout := SpiderLayout.preset(layout_preset)
	_walk_groups = layout.groups()
	_run_groups = layout.groups(true)
	_groups = _walk_groups
	_running = false
	_next_group = 0
	_body = Node3D.new()
	_body.name = "Body"
	_body.position.y = layout.relaxed_height()
	add_child(_body)
	rig = SpiderRig.build(_body, layout, SpiderChainRig.new() if chain_legs else null)
	rig.set_poles_enabled(use_knee_poles)
	for leg in rig.targets.size():
		var target := rig.targets[leg]
		legs.append(SpiderLeg.new(target))
		var definition := layout.legs[leg]
		legs[leg].roundness = definition.roundness
		legs[leg].box_steps = definition.box_steps
		if definition.servo != Vector3.ZERO:
			legs[leg].servo = SecondOrder.new(definition.servo.x, definition.servo.y, definition.servo.z, target.global_position)
		_target_markers.append(_add_target_marker(target))
	_body_pose = Vector3(layout.relaxed_height(), 0.0, 0.0)
	_body_pose_velocity = Vector3.ZERO
	_planted_once = false
	airborne = false
	_foot_radius = 0.0
	for definition in layout.legs:
		if definition.walks:
			_foot_radius = maxf(_foot_radius, Vector2(definition.rest_foot().x, definition.rest_foot().z).length())
	rebuilt.emit()


## Put the spider somewhere new (e.g. a respawn), facing `yaw`, standing still, with every foot
## planted fresh under it on the next physics frame.
func teleport(where: Vector3, yaw: float) -> void:
	global_position = where
	rotation = Vector3(0.0, yaw, 0.0)
	velocity = Vector3.ZERO
	_last_position = where
	airborne = false
	_vertical_speed = 0.0
	for leg in legs:
		leg.override = false
		leg.needs_return = false
		leg.stepping = false
	_body_pose = Vector3(rig.layout.relaxed_height(), 0.0, 0.0)
	_body_pose_velocity = Vector3.ZERO
	_planted_once = false


## Jump straight up at `up_speed` m/s (horizontal motion still comes from walking / steering).
## Flight time is 2 · up_speed / jump_gravity.
func launch(up_speed: float) -> void:
	if airborne:
		return
	airborne = true
	_vertical_speed = up_speed
	for leg in legs.size():
		if rig.layout.legs[leg].walks:
			legs[leg].begin_override()


# --- 1. move ---------------------------------------------------------------------------------

## `move` is (right, forward) in units of move_speed, so (1, 0) scuttles sideways like a crab.
## The gait doesn't care which way: home spots lead along the actual velocity.
func _move(delta: float) -> void:
	var move := Vector2(0.0, 1.0)
	var turn := auto_turn
	if steering:
		move = steer
		turn = 0.0
	elif auto_walk:
		if walk_sideways or rig.layout.sideways:
			move = Vector2(1.0, 0.0) # strafe right while turning left: circles facing the centre
	else:
		move = Vector2(
			float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)),
			float(Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_DOWN)))
		turn = (float(Input.is_key_pressed(KEY_LEFT)) - float(Input.is_key_pressed(KEY_RIGHT))) * turn_speed
	_turn_rate = turn * speed_scale + extra_turn + (steer_turn if steering else 0.0)
	rotate_y(_turn_rate * delta)
	global_position += global_basis * Vector3(move.x, 0.0, -move.y) * move_speed * speed_scale * delta
	var new_velocity := (global_position - _last_position) / delta
	new_velocity.y = 0.0
	# Acceleration in the spider's own frame. Turning while moving shows up here as a sideways
	# (centripetal) acceleration, so one value covers both strafe changes and turns.
	var accel := global_basis.inverse() * ((new_velocity - velocity) / delta)
	_forward_accel = -accel.z
	_side_accel = accel.x
	velocity = new_velocity
	_last_position = global_position


# --- 2. ground -------------------------------------------------------------------------------

func _follow_ground(delta: float) -> void:
	ground_ray = _cast_down(global_position)
	if airborne:
		_fly(delta)
	elif ground_ray.hit != null:
		global_position.y = lerpf(global_position.y, ground_ray.hit.y, 1.0 - exp(-12.0 * delta))


# Ballistic flight; on touching the ground, every foot plants at its home spot at once.
func _fly(delta: float) -> void:
	_vertical_speed -= jump_gravity * delta
	global_position.y += _vertical_speed * delta
	if ground_ray.hit == null or _vertical_speed > 0.0 or global_position.y > ground_ray.hit.y:
		return
	global_position.y = ground_ray.hit.y
	airborne = false
	velocity = Vector3.ZERO # plant under the body, not where the flight speed would lead
	_update_homes()
	for leg in legs.size():
		if rig.layout.legs[leg].walks:
			legs[leg].override = false
			legs[leg].needs_return = false
			legs[leg].snap_to(homes[leg])
	landed.emit()


## Raycast straight down through `point` (from 2 m above to 4 m below).
## Returns {from, to, hit} — hit is the ground point, or null if nothing was hit.
func _cast_down(point: Vector3) -> Dictionary:
	var from := point + Vector3.UP * 2.0
	var to := point + Vector3.DOWN * 4.0
	var result := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, ground_mask))
	return {"from": from, "to": to, "hit": result.position if not result.is_empty() else null}


# --- 3. gait ---------------------------------------------------------------------------------

## Where each foot should rest right now: its relaxed spot under the body, pushed ahead by the
## walking velocity (so steps land in front, not behind), dropped onto the ground.
## Computed once per frame (one raycast per leg) and kept for the debug drawing.
func _update_homes() -> void:
	homes.resize(legs.size())
	home_rays.resize(legs.size())
	for leg in legs.size():
		var definition := rig.layout.legs[leg]
		var rest := definition.rest_foot()
		var lead := (velocity * lead_time).limit_length(max_lead) * definition.lead_scale
		var spot := global_transform * Vector3(rest.x, 0.0, rest.z) + lead
		home_rays[leg] = _cast_down(spot)
		homes[leg] = home_rays[leg].hit if home_rays[leg].hit != null else spot


func _update_gait(delta: float) -> void:
	if airborne:
		_tuck_legs()
	elif not use_gait:
		for leg in legs.size():
			if not legs[leg].override and rig.layout.legs[leg].walks:
				legs[leg].snap_to(homes[leg])
	else:
		for leg in legs.size(): # a leg just released by a gesture steps straight back down
			if legs[leg].needs_return and not legs[leg].stepping and rig.layout.legs[leg].walks:
				legs[leg].start_step(homes[leg])
		_choose_groups()
		_maybe_start_group()
		_rescue_overstretched()
	for leg in legs.size():
		var definition := rig.layout.legs[leg]
		if definition.walks or legs[leg].override:
			legs[leg].update(delta, leg_step_time(leg), step_height * definition.lift_scale)
			if legs[leg].just_planted:
				_push_off(leg)
		else:
			_hold_arm(leg)


## How far this leg's foot may lag behind its home before it steps (its stride).
func leg_step_distance(leg: int) -> float:
	return step_distance * rig.layout.legs[leg].stride_scale


## Seconds this leg's step takes right now. A foot must finish its step before the body has
## moved about a stride, or it falls behind and the leg overstretches: when running or spinning
## fast, steps get quicker (never below fast_step_time). Feet move with the walk plus
## ω · radius from turning. A long stride may take longer; step_time_scale (a limp, a
## heavy arm) applies on top.
func leg_step_time(leg: int) -> float:
	var foot_speed := velocity.length() + absf(_turn_rate) * _foot_radius
	var keep_up := leg_step_distance(leg) * 1.2 / maxf(foot_speed, 0.01)
	return minf(step_time, maxf(keep_up, fast_step_time)) * rig.layout.legs[leg].step_time_scale


# A limb that does the work (a monkey's arm) shoves the body as it plants: up, and tipped away
# from its end of the body (a front hand lifts the chest). Scales with speed, so a slow walk
# barely bobs while a run bounds.
func _push_off(leg: int) -> void:
	var definition := rig.layout.legs[leg]
	if definition.push <= 0.0:
		return
	var effort := clampf(velocity.length() / move_speed, 0.0, 2.0) * definition.push
	_body_pose_velocity.x += effort
	_body_pose_velocity.y += effort * -definition.hip.z * 1.2 # front (−z) → nose up


# In the air: feet pull in under the body while rising, then stretch back out toward their
# relaxed spots as it falls, ready to land. They ride with the body (body space).
# air_splay blends toward every leg stretched straight out, swept forward (a diving grab).
func _tuck_legs() -> void:
	var reach := clampf(-_vertical_speed / 4.0, 0.0, 1.0)
	for leg in legs.size():
		var definition := rig.layout.legs[leg]
		if not definition.walks:
			continue
		var tucked := definition.hip + (definition.upper_vec() + definition.lower_vec()) * 0.6 + Vector3.UP * 0.15
		var foot := tucked.lerp(definition.rest_foot(), reach)
		if air_splay > 0.0:
			var outward := Vector3(definition.rest_foot().x - definition.hip.x, 0.0, definition.rest_foot().z - definition.hip.z).normalized()
			var direction := (outward + Vector3.FORWARD * 0.6 + Vector3.UP * 0.1).normalized()
			foot = foot.lerp(definition.hip + direction * definition.reach() * 1.05, air_splay) # 1.05: pulled straight
		legs[leg].begin_override() # (again, in case a gesture released it mid-air)
		legs[leg].override_position = _body.global_transform * foot


# An arm (crab claw) rides with the tilting body at its hold spot, with a slow idle sway.
# `planted` follows too, so a gesture grabbing the arm starts from where it is.
func _hold_arm(leg: int) -> void:
	var definition := rig.layout.legs[leg]
	var t := Time.get_ticks_msec() * 0.001
	var sway := Vector3(0.0, sin(t * 1.7 + leg) * 0.03, sin(t * 1.3 + leg * 2.0) * 0.04)
	var gait_leg := legs[leg]
	gait_leg.needs_return = false
	gait_leg.stepping = false
	gait_leg.planted = _body.global_transform * (definition.hold + sway)
	gait_leg.target.global_position = gait_leg.planted


# Walk or run step groups (a layout with run_speed: diagonal walk ↔ bound). Switches only while
# every foot is down, with a little hysteresis so it doesn't flicker around the threshold.
func _choose_groups() -> void:
	var run_speed := rig.layout.run_speed
	if run_speed <= 0.0 or _any_stepping_except(-1):
		return
	var speed := velocity.length()
	var running := speed > run_speed if not _running else speed > run_speed * 0.8
	if running != _running:
		_running = running
		_groups = _run_groups if running else _walk_groups
		_next_group = 0


# A group may only lift while every other group is fully planted; the groups take turns.
func _maybe_start_group() -> void:
	for attempt in _groups.size():
		var index := (_next_group + attempt) % _groups.size()
		if _any_stepping_except(-1):
			return
		var wants := false
		for leg: int in _groups[index]:
			wants = wants or legs[leg].wants_step(homes[leg], leg_step_distance(leg))
		if wants:
			for leg: int in _groups[index]:
				if not legs[leg].override:
					legs[leg].start_step(homes[leg])
			_next_group = (index + 1) % _groups.size()
			return


# Safety valve on the turn-taking: a planted foot about to pass the end of its reach steps now,
# even while another group is in the air. Happens when one group's steps are long and slow
# (a monkey's big arm swings, a limp) and the body runs away from the waiting feet.
func _rescue_overstretched() -> void:
	for leg in legs.size():
		var gait_leg := legs[leg]
		var definition := rig.layout.legs[leg]
		if gait_leg.stepping or gait_leg.override or not definition.walks:
			continue
		var hip := _body.global_transform * definition.hip
		if hip.distance_to(gait_leg.planted) > definition.reach() * overreach_step:
			gait_leg.start_step(homes[leg])


func _any_stepping_except(group: int) -> bool:
	for index in _groups.size():
		if index == group:
			continue
		for leg: int in _groups[index]:
			if legs[leg].stepping:
				return true
	return false


# --- 4. body ---------------------------------------------------------------------------------

## Each leg wants the body at (its foot height − its relaxed foot drop) so it can stand relaxed.
## Fit a plane through those wishes over the feet's relaxed spots: its average is the body height,
## its front-back slope the pitch, its left-right slope the roll. Then add the lean into speed
## changes and turns, the behaviour offsets, and follow the result with a spring (weight).
func _update_body(delta: float) -> void:
	var height := ride_height + body_height_offset + action_pose.x
	var pitch := body_pitch_offset + action_pose.y - clampf(_forward_accel * lean, -0.3, 0.3) # nose dips when speeding up
	var roll := body_roll_offset + action_pose.z - clampf(_side_accel * lean * 4.0, -0.15, 0.15) # lean into turns / sidesteps
	if follow_feet and not airborne: # in the air there's nothing to stand on: ride at relaxed height
		var fit := _fit_body_plane()
		height += fit.x
		pitch += fit.y
		roll += fit.z
	else:
		height += rig.layout.relaxed_height()
	var target := Vector3(height, pitch, roll)
	var accel := (target - _body_pose) * body_stiffness - _body_pose_velocity * body_damping
	_body_pose_velocity += accel * delta
	_body_pose += _body_pose_velocity * delta
	_body.position = Vector3(body_offset.x, _body_pose.x, body_offset.z)
	_body.basis = Basis.from_euler(Vector3(_body_pose.y, body_yaw_offset, _body_pose.z))


## Least-squares plane h = a + b·x + c·z through each leg's wished body height, placed at that
## leg's relaxed foot spot (x, z). Using the foot spread (not the hips) makes the tilt follow the
## slope of the ground under the feet; with hips, a short body over a step tilts far too much
## (the crab's hips are only 0.4 m apart front to back). Returns (height, pitch, roll).
## Forward is -Z, so a higher front (h falls as z grows) = nose up.
func _fit_body_plane() -> Vector3:
	var inverse := global_transform.affine_inverse()
	var mean := Vector3.ZERO # (x, z, h)
	var samples: Array[Vector3] = []
	for leg in legs.size():
		var definition := rig.layout.legs[leg]
		if not definition.walks: # arms don't hold the body up
			continue
		var foot := inverse * legs[leg].support_point()
		var rest := definition.rest_foot()
		samples.append(Vector3(rest.x, rest.z, foot.y - rest.y))
	for sample in samples:
		mean += sample / float(samples.size())
	var xx := 0.0
	var zz := 0.0
	var xh := 0.0
	var zh := 0.0
	for sample in samples:
		var d := sample - mean
		xx += d.x * d.x
		zz += d.y * d.y
		xh += d.x * d.z
		zh += d.y * d.z
	var slope_x := xh / xx if xx > 0.0001 else 0.0 # height change per metre to the right
	var slope_z := zh / zz if zz > 0.0001 else 0.0 # height change per metre backwards
	return Vector3(mean.z, atan(-slope_z), atan(slope_x))


func _add_target_marker(target: Marker3D) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = 0.06
	sphere.height = 0.12
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1, 0.3, 0.2)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var marker := MeshInstance3D.new()
	marker.mesh = sphere
	marker.material_override = material
	marker.visible = show_targets
	target.add_child(marker)
	return marker
