## Personality layer on top of the walking spider. It never touches bones or the gait logic; it
## only drives the Spider's hooks (speed_scale, extra_turn, body offsets, gait timing) and takes
## over single legs for gestures. Switch it off and the spider is back to plain walking.
##
##   rhythm   WALK → DART (fast, low skitter) → FREEZE (dead still, breathing) → … at random
##   gesture  while frozen, a front leg taps the ground (scary) or waves (cute)
##   threat   R: rears up, front legs raised and waving, stares at `watch` … then LUNGES
##   eyes     track `watch`, blink; glow red and shrink pupils as menace rises
##   menace   0 = cute (steady, bouncy, big pupils, waves) … 1 = scary (darts, freezes, glows)
class_name SpiderMood
extends Node

enum State { WALK, DART, FREEZE, THREAT, LUNGE }

@export var spider: Spider
@export var watch: Node3D ## what it pays attention to (eyes, staring, threat, lunge)
@export_range(0.0, 1.0, 0.01) var menace := 0.6
## The random WALK/DART/FREEZE rhythm. Off when a SpiderBrain decides the movement: the mood
## then stays in WALK (eyes, bob, gait style) apart from the threat display (R).
@export var rhythm := true
@export var active := true:
	set(value):
		active = value
		if not value and spider != null:
			_reset_spider()

var state := State.WALK

var _state_left := 0.0 ## seconds until the next state change
var _time := 0.0
var _speed := 1.0 ## smoothed speed_scale, so darts accelerate instead of teleporting
var _gesture_leg := -1 ## front leg tapping / waving during a freeze, -1 = none
var _blink_left := 2.0
var _blinking := 0.0
var _base_step_time := 0.0
var _base_step_height := 0.0


func _ready() -> void:
	_base_step_time = spider.step_time
	_base_step_height = spider.step_height
	spider.rebuilt.connect(_on_spider_rebuilt)


# New legs (layout changed): old leg indices mean nothing now, start over calmly.
func _on_spider_rebuilt() -> void:
	state = State.WALK
	_state_left = 1.0
	_gesture_leg = -1


func _physics_process(delta: float) -> void:
	if not active or spider == null or spider.legs.is_empty():
		return
	_time += delta
	_state_left -= delta
	if _state_left <= 0.0:
		_next_state()
	_apply_state(delta)
	_update_eyes(delta)


## Start the threat display now (it ends in a lunge).
func trigger_threat() -> void:
	_enter(State.THREAT, 1.6)


func state_name() -> String:
	return State.keys()[state]


# --- rhythm ----------------------------------------------------------------------------------

func _next_state() -> void:
	if not rhythm and state != State.THREAT:
		if state == State.WALK:
			_state_left = 1.0 # stay put: re-entering would release the legs (bad mid-jump)
		else:
			_enter(State.WALK, 1.0)
		return
	match state:
		State.THREAT:
			_enter(State.LUNGE, 0.4)
		State.LUNGE, State.DART: # after a dash: freeze if menacing, else just walk on
			if randf() < 0.3 + 0.6 * menace:
				_enter(State.FREEZE, randf_range(0.5, 1.0 + 1.5 * menace))
			else:
				_enter(State.WALK, randf_range(1.0, 2.5))
		_:
			var roll := randf()
			if roll < 0.1 + 0.5 * menace:
				_enter(State.DART, randf_range(0.35, 0.8))
			elif roll < 0.25 + 0.6 * menace and state != State.FREEZE:
				_enter(State.FREEZE, randf_range(0.6, 1.5 + 1.5 * menace))
			else:
				_enter(State.WALK, randf_range(1.5, 3.5) * (1.5 - menace))


func _enter(new_state: State, duration: float) -> void:
	for leg in spider.legs:
		leg.release_override()
	state = new_state
	_state_left = duration
	_gesture_leg = -1
	if new_state == State.FREEZE and randf() < 0.6:
		_gesture_leg = spider.rig.layout.front_legs()[randi() % 2]


# --- what each state does to the spider ------------------------------------------------------

func _apply_state(delta: float) -> void:
	var cute := 1.0 - menace
	var speed := 1.0
	var step_time := _base_step_time * lerpf(1.15, 0.8, menace) # cute = relaxed, scary = quick
	var step_height := _base_step_height * lerpf(1.4, 0.6, menace) # cute = bouncy, scary = low
	var height := -0.12 * menace # scary rides low
	var pitch := 0.0
	var turn := 0.0
	match state:
		State.WALK:
			height += sin(_time * 9.0) * 0.035 * cute # happy bob
		State.DART:
			speed = 2.6
			step_time *= 0.55
			step_height *= 0.6
			pitch = -0.08
		State.FREEZE:
			speed = 0.0
			height += sin(_time * 2.2) * 0.015 # breathing
			turn = _turn_toward_watch(1.5 * menace)
			if _gesture_leg >= 0:
				_gesture(_gesture_leg)
		State.THREAT:
			speed = 0.0
			height += 0.3
			pitch = 0.5 # rear up
			turn = _turn_toward_watch(4.0)
			_raise_front_legs()
		State.LUNGE:
			speed = 3.5
			step_time *= 0.5
			step_height *= 0.5
			pitch = -0.2
			turn = _turn_toward_watch(4.0)
	_speed = lerpf(_speed, speed, 1.0 - exp(-12.0 * delta))
	spider.speed_scale = _speed
	spider.extra_turn = turn
	spider.step_time = step_time
	spider.step_height = step_height
	spider.body_height_offset = height
	spider.body_pitch_offset = pitch


## Turn rate (rad/s) that rotates the spider to face `watch`; `strength` = how eagerly.
func _turn_toward_watch(strength: float) -> float:
	if watch == null:
		return 0.0
	var local := spider.global_basis.inverse() * (watch.global_position - spider.global_position)
	return clampf(atan2(-local.x, -local.z) * strength, -3.0, 3.0) # forward is -Z


# Scary: tap the ground just ahead, as if testing it. Cute: lift the leg high and wave.
func _gesture(leg: int) -> void:
	var gait_leg := spider.legs[leg]
	gait_leg.begin_override()
	var forward := -spider.global_basis.z
	var side := spider.global_basis.x * signf(spider.rig.layout.legs[leg].hip.x)
	if menace >= 0.4:
		var lift := 0.04 + 0.14 * maxf(0.0, sin(_time * 14.0))
		gait_leg.override_position = gait_leg.planted + forward * 0.15 + Vector3.UP * lift
	else:
		gait_leg.override_position = gait_leg.planted + forward * 0.2 + Vector3.UP * 0.6 + side * (0.15 * sin(_time * 8.0))


# Threat display: both front legs up and forward, waving out of phase.
func _raise_front_legs() -> void:
	var front := spider.rig.layout.front_legs()
	for i in front.size():
		var leg := front[i]
		if leg < 0:
			continue
		var gait_leg := spider.legs[leg]
		gait_leg.begin_override()
		var definition := spider.rig.layout.legs[leg]
		var relaxed := -definition.rest_foot().y # body height above ground for this leg
		var wave := sin(_time * 9.0 + i * PI) * 0.12
		var local := definition.hip + definition.out * 0.35 + Vector3(0, relaxed + 0.9 + wave, -0.45)
		gait_leg.override_position = spider.global_transform * local


# --- eyes ------------------------------------------------------------------------------------

func _update_eyes(delta: float) -> void:
	_blink_left -= delta
	if _blink_left <= 0.0:
		_blinking = 0.12
		_blink_left = randf_range(1.5, 4.0) * (0.6 + menace) # scary blinks less
	_blinking = maxf(_blinking - delta, 0.0)
	var openness := 0.1 if _blinking > 0.0 else 1.0
	var pupil_size := lerpf(1.3, 0.55, menace) # cute = big pupils, scary = pinpricks
	for i in spider.rig.eyes.size():
		var eye := spider.rig.eyes[i]
		_aim_eye(eye)
		eye.scale = Vector3(1.0, openness, 1.0)
		spider.rig.pupils[i].scale = Vector3.ONE * pupil_size
		var material := spider.rig.eye_materials[i]
		material.albedo_color = Color(0.95, 0.95, 0.9).lerp(Color(1.0, 0.25, 0.15), menace)
		material.emission_enabled = menace > 0.05
		material.emission = Color(1.0, 0.15, 0.05)
		material.emission_energy_multiplier = 3.0 * menace * menace


# Look at `watch`, but only when it's in front of the face — eyes can't turn into the head.
func _aim_eye(eye: Node3D) -> void:
	if watch == null:
		eye.rotation = Vector3.ZERO
		return
	var parent := eye.get_parent() as Node3D
	var local := parent.global_basis.inverse() * (watch.global_position - eye.global_position)
	if local.normalized().z < -0.25:
		eye.look_at(watch.global_position, Vector3.UP)
	else:
		eye.rotation = Vector3.ZERO


# Mood switched off: hand everything back to plain walking.
func _reset_spider() -> void:
	for leg in spider.legs:
		leg.release_override()
	spider.speed_scale = 1.0
	spider.extra_turn = 0.0
	spider.body_height_offset = 0.0
	spider.body_pitch_offset = 0.0
	spider.step_time = _base_step_time
	spider.step_height = _base_step_height
	for i in spider.rig.eyes.size():
		spider.rig.eyes[i].rotation = Vector3.ZERO
		spider.rig.eyes[i].scale = Vector3.ONE
		spider.rig.pupils[i].scale = Vector3.ONE
		spider.rig.eye_materials[i].emission_enabled = false
		spider.rig.eye_materials[i].albedo_color = Color(0.95, 0.95, 0.9)
	state = State.WALK
