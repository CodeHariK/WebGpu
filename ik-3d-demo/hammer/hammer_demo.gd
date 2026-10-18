## KingClown's hammer, all procedural: SmashAttack (each attack as a pure function of time), a body
## holding the hammer (KingClownBody: rigid one-mesh-per-bone parts on LimbChain IK; or ClownBody:
## Sophia + skeleton IK modifiers — B switches), Robot (what gets smashed).
##
## Moves:
##   jump smash   click / Space: jump at the robot nearest the mouse (it glows) and smash its head
##   head-hop     click / Space again while bouncing off a head: hop straight onto the next robot
##                (the one under the mouse, else the nearest other one) — chain as many as you like
##   ground pound right click / G: jump and slam the ground in front; a shockwave ring knocks the
##                robots round it over (they hop, tip away and rock back up, dizzy)
## Other keys: B switch body (KingClown / Sophia), WASD walk, P replay the last attack, Tab the
## tuning panel (HammerPanel: the hammer, its grips, the idle pose, the swing, the timing, the
## body's sizes — saved to HammerTuning.PATH; a timeline to scrub, play / pause / speed), C the
## debug view, T slow motion, R reset.
##
## Because an attack is a pure function of time, pausing, scrubbing and changing a slider all just
## re-sample it: what you see at time t is exactly what the attack does at t. The robots it hits
## react from the same clock; robots hit by earlier attacks finish reacting on the game clock.
class_name HammerDemo
extends Node3D

const ROBOTS := [ ## [where, height of its head's top]
	[Vector3(0, 0, -5), 1.8], [Vector3(-3.5, 0, -3.5), 2.4], [Vector3(3.5, 0, -4.5), 1.4],
	[Vector3(-1.5, 0, -8.5), 2.8], [Vector3(4.5, 0, -8.5), 2.0], [Vector3(1.5, 0, -11.5), 1.6],
]
const ROBOT_COLORS := [Color(0.45, 0.5, 0.6), Color(0.35, 0.45, 0.55), Color(0.55, 0.5, 0.45), Color(0.4, 0.42, 0.5), Color(0.5, 0.55, 0.6), Color(0.45, 0.4, 0.5)]
const WALK_SPEED := 4.0
const RANGE := 9.0 ## m: the farthest robot he jumps at
const PICK_RADIUS := 2.5 ## m: the robot nearest the mouse point, within this, is the target
const POUND_DISTANCE := 1.4 ## m in front of him a ground pound lands
const SHOCK_RADIUS := 5.0 ## m: the shockwave knocks robots this close to the slam
const SHOCK_SPEED := 12.0 ## m/s: how fast the ring spreads
const CAMERA_OFFSET := Vector3(5.5, 2.6, 6.0) ## a three-quarter side view, so the jump shows in profile

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var attack := SmashAttack.new()
var body: HammerBody ## the one showing
var bodies: Array[HammerBody] = [] ## [KingClown, Sophia]
var robots: Array[Robot] = []
var feet := Vector3.ZERO
var facing := Vector3.FORWARD
var time := -1.0 ## seconds into the attack (keeps counting after it ends, for the reactions); −1: none yet
var paused := false
var speed := 1.0 ## the panel's playback speed
var slow := false
var target := -1 ## the robot of the attack (−1: a ground pound, or none)
var picked := -1 ## the robot nearest the mouse
var combo := 0 ## heads hit in a row (head-hops)

var _clock := 0.0 ## game seconds (earlier attacks' robots react on it)
var _hit_clock: Array[float] = [] ## per robot: when its head was last hit (−INF: never)
var _knock_clock: Array[float] = [] ## per robot: when a shockwave last reached it
var _knock_away: Array[Vector3] = []
var _hop_queued := false ## a head-hop was asked for: it starts when the bounce does
var _debug: HammerDebug
var _panel: HammerPanel
var _ring: MeshInstance3D
var _measured := 0 ## frames until the body's shoulders are measured
var _focus := Vector3.ZERO


func _ready() -> void:
	_add_ground()
	for each: HammerBody in [KingClownBody.new(), ClownBody.new()]:
		add_child(each)
		bodies.append(each)
	HammerTuning.remember(attack) # the code's values are the defaults …
	for each in bodies:
		HammerTuning.remember(each)
	HammerTuning.load_into(attack, bodies) # … then the saved ones (if any)
	_use_body(0)
	for i in ROBOTS.size():
		var robot := Robot.new()
		add_child(robot)
		robot.setup(ROBOTS[i][0], ROBOTS[i][1], ROBOT_COLORS[i])
		robots.append(robot)
		_hit_clock.append(-INF)
		_knock_clock.append(-INF)
		_knock_away.append(Vector3.ZERO)
	_ring = _make_ring()
	_debug = HammerDebug.new()
	add_child(_debug)
	_panel = HammerPanel.new()
	$Hud.add_child(_panel)
	_panel.setup(self)
	_focus = feet


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_LEFT:
			smash(picked)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			pound()
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE: smash(picked)
			KEY_G: pound()
			KEY_B: _use_body(1 - bodies.find(body))
			KEY_P: replay()
			KEY_C: _debug.enabled = not _debug.enabled
			KEY_T: slow = not slow
			KEY_TAB: _panel.visible = not _panel.visible
			KEY_R: _reset()


## Jump at robot `index` and smash its head — or, bouncing off a head, hop on to the next one.
func smash(index: int) -> void:
	if attacking():
		if target >= 0 and not attack.is_ground() and time >= attack.contact_time() - 0.15:
			_hop_queued = true # (a press just before the hit counts too)
		return
	if index < 0 or feet.distance_to(robots[index].global_position) > RANGE:
		return
	combo = 0
	target = index
	attack.aim(feet, robots[index].head_top())
	_begin()


## Ground pound: jump and slam the ground in front of him (toward the mouse).
func pound() -> void:
	if attacking():
		return
	combo = 0
	target = -1
	var aim := _mouse_ground() - feet
	aim.y = 0.0
	var forward := aim.normalized() if aim.length() > 0.3 else facing
	attack.aim_ground(feet, feet + forward * POUND_DISTANCE)
	_begin()


## The last attack again, from its start (same snapshot).
func replay() -> void:
	if time < 0.0:
		return
	_begin()


## An attack is playing (he isn't free to walk).
func attacking() -> bool:
	return time >= 0.0 and time < attack.length()


## Jump to `t` seconds of the attack and hold there (the panel's timeline).
func scrub(t: float) -> void:
	if time < 0.0:
		return
	time = t
	paused = true


func _begin() -> void:
	if target >= 0:
		robots[target].face(attack.contact_feet())
	time = 0.0
	paused = false
	_hop_queued = false


# Bouncing off a head with a hop asked for: carry straight on into a jump smash on the next robot.
func _hop_on(pose: SmashAttack.Pose) -> void:
	_hop_queued = false
	var next := _next_robot(pose.feet)
	if next < 0:
		return
	combo += 1
	target = next
	attack.chain(pose, robots[next].head_top())
	_begin()


# The robot to hop to: the one under the mouse (not the one just hit), else the nearest other one.
func _next_robot(from: Vector3) -> int:
	if picked >= 0 and picked != target and from.distance_to(robots[picked].head_top()) < RANGE:
		return picked
	var best := -1
	var best_distance := RANGE
	for i in robots.size():
		var d := from.distance_to(robots[i].head_top())
		if i != target and d < best_distance:
			best = i
			best_distance = d
	return best


# Show body `index`, hide the other; the attack is worked out from its shoulders.
func _use_body(index: int) -> void:
	body = bodies[index]
	for each in bodies:
		each.visible = each == body
		each.hammer.visible = each == body
	_measured = 0
	if _panel != null:
		_panel.rebuild() # its Body tab


func _reset() -> void:
	feet = Vector3.ZERO
	facing = Vector3.FORWARD
	time = -1.0
	target = -1
	combo = 0
	for i in robots.size():
		_hit_clock[i] = -INF
		_knock_clock[i] = -INF
		robots[i].react(-1.0)


func _process(delta: float) -> void:
	if body.live_shoulders(): # KingClown: his shoulders are where his sliders say, always
		attack.shoulder_height = body.shoulder_height()
	elif _measured < 5: # Sophia: once her idle pose has settled, measure her shoulders
		_measured += 1
		if _measured == 5:
			attack.shoulder_height = body.shoulder_height()
	var step := delta * speed * (0.25 if slow else 1.0)
	if not paused:
		_clock += step
	var moving := false
	if time >= 0.0 and not paused:
		var was := attacking()
		var before := time
		time += step
		if before < attack.contact_time() and time >= attack.contact_time():
			_record_hit(time - attack.contact_time())
		if _hop_queued and attack.sample(time).phase == "recover":
			_hop_on(attack.sample(time))
		elif was and not attacking(): # landed: he walks on from where he landed
			feet = attack.landing()
			facing = attack.sample(attack.length()).forward
	if not attacking():
		moving = _walk(delta)
	_pick()
	var showing := time >= 0.0 and (attacking() or (paused and time <= attack.length()))
	var pose := attack.sample(time) if showing else attack.stand(feet, facing)
	body.apply(pose, attack, _look_point(pose, showing), moving)
	_react_robots()
	_show_ring()
	_follow(pose, delta)
	_debug.draw(self, pose)
	hud.text = "B body   WASD walk   mouse: pick a robot   click / Space: jump smash (again while bouncing: head-hop)   right click / G: ground pound\nP replay   Tab tuning   C debug   T slow%s   R reset\n%s  %.2f s%s" % [
		" (on)" if slow else "", pose.phase, maxf(time, 0.0), ("   combo x%d" % (combo + 1)) if combo > 0 else ""]


# The attack reached its contact (`late` seconds ago): note it on the game clock, so the robots
# keep reacting after the next attack starts.
func _record_hit(late: float) -> void:
	var at := _clock - late
	if target >= 0:
		_hit_clock[target] = at
		return
	if not attack.is_ground():
		return
	var slam := attack.contact_point()
	for i in robots.size():
		var d := Vector2(robots[i].global_position.x - slam.x, robots[i].global_position.z - slam.z).length()
		if d < SHOCK_RADIUS:
			_knock_clock[i] = at + d / SHOCK_SPEED
			_knock_away[i] = robots[i].global_position - slam


# Every robot's reaction: the current attack's from its own time (scrub-able), earlier ones from
# the game clock.
func _react_robots() -> void:
	var since := time - attack.contact_time() if time >= attack.contact_time() else -1.0
	var slam := attack.contact_point()
	for i in robots.size():
		var robot := robots[i]
		robot.highlight = 1.0 if i == picked and not attacking() else 0.0
		var hit := _clock - _hit_clock[i]
		var knock := _clock - _knock_clock[i]
		var away := _knock_away[i]
		if i == target and since >= 0.0:
			hit = since
		if attack.is_ground() and since >= 0.0:
			var d := Vector2(robot.global_position.x - slam.x, robot.global_position.z - slam.z).length()
			if d < SHOCK_RADIUS and since >= d / SHOCK_SPEED:
				knock = since - d / SHOCK_SPEED
				away = robot.global_position - slam
		robot.react(hit if hit < 5.0 else -1.0, knock if knock < 5.0 else -1.0, away)


# The ground pound's shockwave: a ring spreading from the slam, fading as it goes.
func _show_ring() -> void:
	var since := time - attack.contact_time()
	var on := attack.is_ground() and time >= 0.0 and since >= 0.0 and since * SHOCK_SPEED < SHOCK_RADIUS * 1.2
	_ring.visible = on
	if not on:
		return
	var radius := maxf(since * SHOCK_SPEED, 0.05)
	_ring.global_position = Vector3(attack.contact_point().x, 0.05, attack.contact_point().z)
	_ring.scale = Vector3(radius, 1.0, radius)
	var material: StandardMaterial3D = _ring.material_override
	material.albedo_color.a = clampf(1.0 - radius / (SHOCK_RADIUS * 1.2), 0.0, 1.0)


# Where his head looks: the robot he's after, the slam spot, the picked robot, else ahead.
func _look_point(
		pose: SmashAttack.Pose,
		showing: bool
) -> Vector3:
	if showing:
		return robots[target].head_top() if target >= 0 else attack.contact_point()
	if picked >= 0:
		return robots[picked].head_top()
	return pose.feet + pose.forward * 5.0 + Vector3.UP * 1.5


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


# The robot nearest the mouse point on the ground (within PICK_RADIUS); −1: none.
func _pick() -> void:
	var hit := _mouse_ground()
	picked = -1
	var best := PICK_RADIUS
	for i in robots.size():
		var d := hit.distance_to(robots[i].global_position)
		if d < best:
			best = d
			picked = i


func _mouse_ground() -> Vector3:
	var mouse := get_viewport().get_mouse_position()
	var hit = Plane(Vector3.UP, 0.0).intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	return hit if hit != null else feet + facing * 3.0


# The camera eases after him; a hit shakes it (a pure function of the time since the hit).
func _follow(
		pose: SmashAttack.Pose,
		delta: float
) -> void:
	_focus = _focus.lerp(Vector3(pose.feet.x, pose.feet.y * 0.5, pose.feet.z), 1.0 - exp(-4.0 * delta)) # half his height: the jump stays in view
	var shake := Vector3.ZERO
	if pose.since_hit >= 0.0 and pose.since_hit < 0.8:
		var amount := (0.45 if attack.is_ground() else 0.3) * exp(-pose.since_hit * 6.0)
		var s := pose.since_hit
		shake = Vector3(sin(s * 91.0), sin(s * 73.0 + 1.0), 0.0) * amount
	camera.h_offset = lerpf(camera.h_offset, 1.6 if _panel.visible else 0.0, 1.0 - exp(-6.0 * delta)) # the panel open: him left of it
	camera.position = _focus + CAMERA_OFFSET + shake
	camera.look_at(_focus + Vector3(0, 1.4, 0) + shake * 0.5)


func _make_ring() -> MeshInstance3D:
	var mesh := TorusMesh.new()
	mesh.inner_radius = 0.92
	mesh.outer_radius = 1.0 # scaled to the ring's radius (its thickness grows a little with it)
	var ring := MeshInstance3D.new()
	ring.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.85, 0.4, 1.0)
	material.emission_enabled = true
	material.emission = Color(1.0, 0.7, 0.3)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	ring.material_override = material
	ring.visible = false
	add_child(ring)
	return ring


func _add_ground() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.36, 0.5, 0.36)
	ground.material_override = material
	add_child(ground)
