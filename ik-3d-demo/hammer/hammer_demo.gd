## KingClown's hammer: a jump smash onto a robot's head, all procedural (SmashAttack: the attack as
## a pure function of time; ClownBody: Sophia + IK holding the hammer; Robot: what gets smashed).
##
## Controls: WASD walk, the mouse picks the robot nearest it (it glows), left click or Space jumps
## at it and smashes. P replays the last attack from the same spot. Tab: the tuning panel (sliders
## for every number of the attack, a timeline to scrub it, play / pause / speed). C: the debug
## view (the jump arc, the hammer head's path, the contact point, the arms). T: slow motion.
## R: reset.
##
## Because the attack is a pure function of time, pausing, scrubbing and changing a slider all
## just re-sample it: what you see at time t is always exactly what the attack does at t.
class_name HammerDemo
extends Node3D

const ROBOTS := [ ## [where, height of its head's top]
	[Vector3(0, 0, -5), 1.8], [Vector3(-3.5, 0, -3.5), 2.4], [Vector3(3.5, 0, -4.5), 1.4],
	[Vector3(-1.5, 0, -8.5), 2.8], [Vector3(4.5, 0, -8.5), 2.0],
]
const ROBOT_COLORS := [Color(0.45, 0.5, 0.6), Color(0.35, 0.45, 0.55), Color(0.55, 0.5, 0.45), Color(0.4, 0.42, 0.5), Color(0.5, 0.55, 0.6)]
const WALK_SPEED := 4.0
const RANGE := 9.0 ## m: the farthest robot he jumps at
const PICK_RADIUS := 2.5 ## m: the robot nearest the mouse point, within this, is the target
const CAMERA_OFFSET := Vector3(5.5, 2.6, 6.0) ## a three-quarter side view, so the jump shows in profile

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var attack := SmashAttack.new()
var body := ClownBody.new()
var robots: Array[Robot] = []
var feet := Vector3.ZERO
var facing := Vector3.FORWARD
var time := -1.0 ## seconds into the attack (keeps counting after it ends, for the robot's stars); −1: none yet
var paused := false
var speed := 1.0 ## the panel's playback speed
var slow := false
var target := -1 ## the robot of the attack
var picked := -1 ## the robot nearest the mouse

var _start := Vector3.ZERO ## the last attack's start (P replays from here)
var _debug: HammerDebug
var _panel: HammerPanel
var _measured := 0 ## frames until the body's shoulders are measured
var _focus := Vector3.ZERO


func _ready() -> void:
	_add_ground()
	add_child(body)
	for i in ROBOTS.size():
		var robot := Robot.new()
		add_child(robot)
		robot.setup(ROBOTS[i][0], ROBOTS[i][1], ROBOT_COLORS[i])
		robots.append(robot)
	_debug = HammerDebug.new()
	add_child(_debug)
	_panel = HammerPanel.new()
	$Hud.add_child(_panel)
	_panel.setup(self)
	_focus = feet


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		smash(picked)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE: smash(picked)
			KEY_P: replay()
			KEY_C: _debug.enabled = not _debug.enabled
			KEY_T: slow = not slow
			KEY_TAB: _panel.visible = not _panel.visible
			KEY_R: _reset()


## Jump at robot `index` and smash its head (if he's free and it's in range).
func smash(index: int) -> void:
	if index < 0 or attacking() or feet.distance_to(robots[index].global_position) > RANGE:
		return
	target = index
	_start = feet
	_begin()


## The last attack again, from where it started.
func replay() -> void:
	if target < 0:
		return
	feet = _start
	_begin()


## An attack is playing (he isn't free to walk).
func attacking() -> bool:
	return time >= 0.0 and time < attack.length()


## Jump to `t` seconds of the attack and hold there (the panel's timeline).
func scrub(t: float) -> void:
	if target < 0:
		return
	time = t
	paused = true


func _begin() -> void:
	attack.aim(_start, robots[target].head_top())
	robots[target].face(_start)
	time = 0.0
	paused = false


func _reset() -> void:
	feet = Vector3.ZERO
	facing = Vector3.FORWARD
	time = -1.0
	target = -1
	for robot in robots:
		robot.react(-1.0)


func _process(delta: float) -> void:
	if _measured < 5: # her idle pose has settled: measure her shoulders once
		_measured += 1
		if _measured == 5:
			attack.shoulder_height = body.shoulder_height()
	var step := delta * speed * (0.25 if slow else 1.0)
	var moving := false
	if time >= 0.0 and not paused:
		var was := attacking()
		time += step
		if was and not attacking(): # landed: he walks on from where he landed
			feet = attack.landing()
			facing = attack.sample(attack.length()).forward
	if not attacking():
		moving = _walk(delta)
	_pick()
	var pose := attack.sample(time) if attacking() or (paused and time >= 0.0 and time <= attack.length()) else _stand()
	var look := robots[target].head_top() if target >= 0 and attacking() else (robots[picked].head_top() if picked >= 0 else pose.feet + pose.forward * 5.0 + Vector3.UP * 1.5)
	body.apply(pose, attack, look, moving)
	for i in robots.size():
		robots[i].highlight = 1.0 if i == picked and not attacking() else 0.0
		robots[i].react(time - attack.contact_time() if i == target and time >= attack.contact_time() else -1.0)
	_follow(pose, delta)
	_debug.draw(self, pose)
	hud.text = "WASD walk   mouse: pick a robot   click / Space: jump smash   P replay   Tab tuning   C debug   T slow%s   R reset\n%s  %.2f s" % [
		" (on)" if slow else "", pose.phase, maxf(time, 0.0)]


# Free: WASD moves him; he faces the way he walks, else the picked robot.
func _walk(delta: float) -> bool:
	var input := Vector3(float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)), 0,
			float(Input.is_key_pressed(KEY_S)) - float(Input.is_key_pressed(KEY_W)))
	if input.length() > 0.01:
		feet += input.limit_length(1.0) * WALK_SPEED * delta
		facing = facing.slerp(input.normalized(), 1.0 - exp(-12.0 * delta)).normalized()
		return true
	if picked >= 0:
		var to := robots[picked].global_position - feet
		to.y = 0.0
		if to.length() > 0.3:
			facing = facing.slerp(to.normalized(), 1.0 - exp(-8.0 * delta)).normalized()
	return false


func _stand() -> SmashAttack.Pose:
	return attack.stand(feet, facing)


# The robot nearest the mouse point on the ground (within PICK_RADIUS); −1: none.
func _pick() -> void:
	var mouse := get_viewport().get_mouse_position()
	var hit = Plane(Vector3.UP, 0.0).intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	picked = -1
	if hit == null:
		return
	var best := PICK_RADIUS
	for i in robots.size():
		var d := (hit as Vector3).distance_to(robots[i].global_position)
		if d < best:
			best = d
			picked = i


# The camera eases after him; a hit shakes it (a pure function of the time since the hit).
func _follow(
		pose: SmashAttack.Pose,
		delta: float
) -> void:
	_focus = _focus.lerp(Vector3(pose.feet.x, pose.feet.y * 0.5, pose.feet.z), 1.0 - exp(-4.0 * delta)) # half his height: the jump stays in view
	var shake := Vector3.ZERO
	if pose.since_hit >= 0.0 and pose.since_hit < 0.8:
		var amount := 0.3 * exp(-pose.since_hit * 6.0)
		var s := pose.since_hit
		shake = Vector3(sin(s * 91.0), sin(s * 73.0 + 1.0), 0.0) * amount
	camera.position = _focus + CAMERA_OFFSET + shake
	camera.look_at(_focus + Vector3(0, 1.4, 0) + shake * 0.5)


func _add_ground() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.36, 0.5, 0.36)
	ground.material_override = material
	add_child(ground)
