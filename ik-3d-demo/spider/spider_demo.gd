## Demo world for the IK spider: bumpy ground on the spider's walking circle, a follow camera,
## and keys to switch each part of the system off and see what it does.
##   G  gait (stepping) on/off      — off: feet glued to their home spots, they slide
##   B  body follows feet on/off    — off: body stays level at a fixed height
##   P  knee poles on/off           — off: the IK picks the knee direction itself
##   T  show foot targets           M  auto walk / manual (arrows walk + turn, A/D strafe)
##   D  debug lines (raycasts, home spots, step circles, arcs, knee poles) — see spider_debug.gd
##   , .  step curve smaller / bigger (step_height)      ; '  step noise less / more (metres)
##   X  motion trails: the last ~4 s of every hip, knee, ankle, foot, knee pole and the body
##      centre, a tick every 3 frames (spider_trails.gd)
##   N  mood on/off (spider_mood.gd)   [ ]  menace down/up (cute ↔ scary)   R  threat + lunge
##   L  next leg layout: 4 legs → 6 (tripod) → 8 → lopsided 5 → crab → monkey → robot → sentry
##      bot → 3-bone machine (clocked square steps, 15° snap turns)   (spider_layout.gd)
##   K  springs on/off (spider_springs.gd): the robots' floating spring heads (watching the orb),
##      anticipated turns and starts (second-order dynamics, second_order.gd). Other layouts
##      have no springs.
##   C  brain on/off (spider_brain.gd): picks behaviours by itself — off = plain circle walk
##   1 charge & back off   2 jump in   3 circle   4 fear   5 freeze   6 wander   (force one now)
##   7 dive: leaps at the orb, legs stretched out, explodes — a new one drops in after a moment
##   8 shoot: head machine gun bursts at the orb, with recoil (sentry bot only — spider_gun.gd)
##   V  camera: follow the spider ↔ player's view (eyes at the orb, watching the spider)
##   W  wave gait on/off: legs ripple back → front on each side (sides half a cycle apart), fast
##      low steps with an ease-out landing, feet planted most of the time — a real spider's walk
##   Y  knee yaw (each leg plane turning about its hip): layout's own → follow the foot → square
##      (snaps ±, mechanical) → noise (living wobble) → fixed (never turns, the old look)
##   J  step path for every leg: arc → circle → ellipse → square → octagon → trapezoid → triangle
##      → hexagon → sawtooth → stab (spider_leg.gd STEP_SHAPES)
##   I  leg IK: Skeleton3D + TwoBoneIK3D ↔ custom LimbChains (spider_chain_rig.gd, limbs/)
##   Mouse: the spider's body by hand, feet staying planted — left-drag moves it (sideways /
##      forward-back), right-drag tilts it (pitch / roll), wheel turns it (yaw), Shift + wheel raises
##      or lowers it, middle click resets
##   Shift + arrows  walk the orb (the player); come within 3 m and the spider freezes, then
##      rushes you (SpiderBrain alert → ambush_behaviour.gd)
## The glowing orb in the middle of the circle is the "player": watched, charged, jumped at, fled.
extends Node3D

const LEGEND := "white root ray   green/red home ray (hit/miss)   yellow home + step circle\ngray/orange foot→home (orange = wants to step)   cyan step arc   magenta knee pole   blue velocity × lead"

const FOLLOW_OFFSET := Vector3(0, 3.0, 5.0) ## follow camera: above and behind, in the spider's frame
const PLAYER_FOV := 70.0 ## player's view: a bit wider, like a first-person camera
const LOOK_SMOOTHING := 10.0 ## player's view: how quickly the gaze catches up with the spider (1/s)

const MOUSE_SENSITIVITY := 0.003 ## metres (or ×2 radians) per pixel dragged
const CONTROL_REACH := 0.35 ## the mouse moves the body at most this far (m)
const CONTROL_LIFT := 0.2 ## …this far up or down (m)
const CONTROL_TILT := 0.5 ## …and tilts it at most this much (radians)
const PLAYER_SPEED := 2.5 ## m/s the orb walks with Shift + arrows

const GROUND_COLOR := Color(0.42, 0.55, 0.38)
const BUMP_COLOR := Color(0.62, 0.5, 0.36)

@onready var spider: Spider = $Spider
@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var debug: SpiderDebug
var trails: SpiderTrails
var mood: SpiderMood
var brain: SpiderBrain
var springs: SpiderSprings
var gun: SpiderGun
var bait: Node3D
var player_view := false ## V: camera at the orb (the player) looking at the spider
var _follow_fov := 75.0
var _look_point := Vector3.ZERO ## where the player's-view camera is looking (smoothed)


func _ready() -> void:
	debug = SpiderDebug.new()
	debug.name = "SpiderDebug"
	debug.spider = spider
	add_child(debug)
	trails = SpiderTrails.new()
	trails.name = "SpiderTrails"
	trails.spider = spider
	trails.visible = false
	add_child(trails)
	bait = _add_bait()
	mood = SpiderMood.new()
	mood.name = "SpiderMood"
	mood.spider = spider
	mood.watch = bait
	add_child(mood)
	var radius := spider.move_speed / spider.auto_turn # the auto-walk circle
	springs = SpiderSprings.new()
	springs.name = "SpiderSprings"
	springs.spider = spider
	springs.look_target = bait # spring heads turn to watch the orb
	gun = SpiderGun.new()
	gun.name = "SpiderGun"
	gun.spider = spider
	gun.springs = springs # recoil kicks the head
	gun.target = bait
	brain = SpiderBrain.new()
	brain.name = "SpiderBrain"
	brain.spider = spider
	brain.target = bait
	brain.gun = gun
	add_child(brain) # before the gun: the brain holds the trigger, then the gun fires that frame
	mood.rhythm = not brain.active # the brain decides the movement
	add_child(springs)
	add_child(gun)
	_add_box(Vector3(0, -0.5, 0), Vector3(40, 1, 40), Vector3.ZERO, GROUND_COLOR)
	for i in 8:
		var angle := TAU * i / 8.0
		var spot := Vector3(cos(angle), 0, sin(angle)) * radius
		match i % 4:
			0: _add_box(spot + Vector3(0, 0.1, 0), Vector3(1.4, 0.4, 1.4), Vector3(0, angle, 0), BUMP_COLOR) # step
			1: _add_box(spot, Vector3(1.8, 0.3, 1.2), Vector3(0, -angle, 0.25), BUMP_COLOR) # ramp
			2: _add_hump(spot + Vector3(0, -0.9, 0), 1.25) # round hump
			3: _add_box(spot + Vector3(0, 0.15, 0), Vector3(2.4, 0.3, 2.4), Vector3(0, angle, 0), BUMP_COLOR) # platform
	spider.global_position = Vector3(radius, 0, 0)
	_follow_fov = camera.fov
	_look_point = spider.global_position


func _process(delta: float) -> void:
	bait.position.y = 0.7 + sin(Time.get_ticks_msec() * 0.002) * 0.1
	_move_player(delta)
	if player_view:
		_player_camera(delta)
	else:
		_follow_camera(delta)
	_update_hud()


# Shift + arrows walk the orb (the player) around, relative to where the camera looks — walk it
# up to the spider to startle it (SpiderBrain alert → ambush).
func _move_player(delta: float) -> void:
	if not Input.is_key_pressed(KEY_SHIFT):
		return
	var input := Vector2(
		float(Input.is_key_pressed(KEY_RIGHT)) - float(Input.is_key_pressed(KEY_LEFT)),
		float(Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_DOWN)))
	if input == Vector2.ZERO:
		return
	var forward := -camera.global_basis.z * Vector3(1, 0, 1)
	var right := camera.global_basis.x * Vector3(1, 0, 1)
	var move := (right.normalized() * input.x + forward.normalized() * input.y).normalized()
	bait.position += move * PLAYER_SPEED * delta


# Above and behind the spider, easing after it.
func _follow_camera(delta: float) -> void:
	camera.fov = _follow_fov
	var behind := spider.global_position + spider.global_basis * FOLLOW_OFFSET
	camera.global_position = camera.global_position.lerp(behind, 1.0 - exp(-3.0 * delta))
	camera.look_at(spider.global_position + Vector3.UP * 0.4)


# The player's eyes: inside the orb (its faces point outward, so from inside it is invisible),
# gaze easing onto the spider's body. While the spider is gone (exploded) it keeps looking at
# where it was.
func _player_camera(delta: float) -> void:
	camera.fov = PLAYER_FOV
	camera.global_position = bait.global_position
	if spider.visible:
		var body := spider.rig.skeleton.global_position
		_look_point = _look_point.lerp(body, 1.0 - exp(-LOOK_SMOOTHING * delta))
	if camera.global_position.distance_to(_look_point) > 0.05:
		camera.look_at(_look_point)


func _update_hud() -> void:
	hud.text = "G gait %s   B body follows feet %s   P knee poles %s   T targets %s   D debug %s   M %s" % [
		_on(spider.use_gait), _on(spider.follow_feet), _on(spider.use_knee_poles),
		_on(spider.show_targets), _on(debug.visible), "auto walk" if spider.auto_walk else "manual (arrows, A/D strafe)",
	]
	hud.text += "   C brain %s   1-8 force   doing: %s" % [_on(brain.active), brain.status()]
	hud.text += "\nL layout: %s   N mood %s   [ ] menace %.2f (%s)   R threat   state: %s   K springs %s" % [
		spider.rig.layout.name, _on(mood.active), mood.menace, "scary" if mood.menace >= 0.5 else "cute",
		mood.state_name() if mood.active else "-", _on(springs.active),
	]
	var knee := spider.knee_yaw if spider.knee_yaw != "layout" else "layout (%s)" % spider.rig.layout.legs[0].knee_yaw
	var curve := spider.step_curve if spider.step_curve != "layout" else "layout (%s)" % spider.rig.layout.legs[0].step_shape
	hud.text += "   , . step height %.2f   ; ' step noise %.2f" % [spider.step_height, spider.step_noise]
	hud.text += "   X trails %s" % _on(trails.visible)
	hud.text += "   W wave gait %s   J steps: %s   Y knee yaw: %s" % [_on(spider.wave_gait), curve, knee]
	hud.text += "   mouse: drag body / tilt / wheel turn   V camera: %s   I legs: %s" % ["player's view" if player_view else "follow", "LimbChains" if spider.rig is SpiderChainRig else "Skeleton3D"]
	if debug.visible:
		hud.text += "\n" + LEGEND


# Y / J cycle the spider's inspector options (Knee yaw → knee_yaw, Step curve → step_curve).
func _next_knee_yaw() -> void:
	var styles: Array[String] = ["layout"]
	styles.append_array(SpiderRig.KNEE_YAW_STYLES)
	spider.knee_yaw = styles[(styles.find(spider.knee_yaw) + 1) % styles.size()]


func _next_step_shape() -> void:
	var shapes: Array = ["layout"]
	shapes.append_array(SpiderLeg.STEP_SHAPES.keys())
	spider.step_curve = shapes[(shapes.find(spider.step_curve) + 1) % shapes.size()]


# Mouse drives Spider.control_offset / control_rotation (see the key list at the top).
func _unhandled_input(event: InputEvent) -> void:
	var motion := event as InputEventMouseMotion
	if motion != null:
		var drag := motion.relative * MOUSE_SENSITIVITY
		if motion.button_mask & MOUSE_BUTTON_MASK_LEFT:
			spider.control_offset.x = clampf(spider.control_offset.x + drag.x, -CONTROL_REACH, CONTROL_REACH)
			spider.control_offset.z = clampf(spider.control_offset.z + drag.y, -CONTROL_REACH, CONTROL_REACH)
		elif motion.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			spider.control_rotation.x = clampf(spider.control_rotation.x - drag.y * 2.0, -CONTROL_TILT, CONTROL_TILT)
			spider.control_rotation.z = clampf(spider.control_rotation.z - drag.x * 2.0, -CONTROL_TILT, CONTROL_TILT)
		return
	var button := event as InputEventMouseButton
	if button == null or not button.pressed:
		return
	var notch := 1.0 if button.button_index == MOUSE_BUTTON_WHEEL_UP else -1.0 if button.button_index == MOUSE_BUTTON_WHEEL_DOWN else 0.0
	if notch != 0.0 and button.shift_pressed:
		spider.control_offset.y = clampf(spider.control_offset.y + notch * 0.03, -CONTROL_LIFT, CONTROL_LIFT)
	elif notch != 0.0:
		spider.control_rotation.y = clampf(spider.control_rotation.y + notch * 0.08, -CONTROL_TILT * 1.5, CONTROL_TILT * 1.5)
	elif button.button_index == MOUSE_BUTTON_MIDDLE:
		spider.control_offset = Vector3.ZERO
		spider.control_rotation = Vector3.ZERO


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_G: spider.use_gait = not spider.use_gait
		KEY_B: spider.follow_feet = not spider.follow_feet
		KEY_P: spider.use_knee_poles = not spider.use_knee_poles
		KEY_T: spider.show_targets = not spider.show_targets
		KEY_M: spider.auto_walk = not spider.auto_walk
		KEY_D: debug.visible = not debug.visible
		KEY_COMMA: spider.step_height = maxf(spider.step_height - 0.04, 0.02)
		KEY_PERIOD: spider.step_height = minf(spider.step_height + 0.04, 0.9)
		KEY_SEMICOLON: spider.step_noise = maxf(spider.step_noise - 0.02, 0.0)
		KEY_APOSTROPHE: spider.step_noise = minf(spider.step_noise + 0.02, 0.4)
		KEY_X:
			trails.visible = not trails.visible
			trails.clear()
		KEY_N: mood.active = not mood.active
		KEY_BRACKETLEFT: mood.menace = maxf(mood.menace - 0.2, 0.0)
		KEY_BRACKETRIGHT: mood.menace = minf(mood.menace + 0.2, 1.0)
		KEY_R: mood.trigger_threat()
		KEY_L: spider.layout_preset = (spider.layout_preset + 1) % SpiderLayout.PRESET_NAMES.size()
		KEY_C:
			brain.active = not brain.active
			mood.rhythm = not brain.active
		KEY_1: brain.force("charge")
		KEY_2: brain.force("jump")
		KEY_3: brain.force("circle")
		KEY_4: brain.force("fear")
		KEY_5: brain.force("freeze")
		KEY_6: brain.force("wander")
		KEY_7: brain.force("dive")
		KEY_8: brain.force("shoot")
		KEY_K: springs.active = not springs.active
		KEY_I: spider.chain_legs = not spider.chain_legs
		KEY_J: _next_step_shape()
		KEY_W: spider.wave_gait = not spider.wave_gait
		KEY_Y: _next_knee_yaw()
		KEY_V:
			player_view = not player_view
			_look_point = spider.rig.skeleton.global_position # start the gaze on the spider


func _on(value: bool) -> String:
	return "ON" if value else "off"


# --- ground pieces: a StaticBody3D (for the spider's raycasts) + a matching mesh -------------

func _add_box(center: Vector3, size: Vector3, rotation_rad: Vector3, color: Color) -> void:
	var shape := BoxShape3D.new()
	shape.size = size
	var mesh := BoxMesh.new()
	mesh.size = size
	_add_static(center, rotation_rad, shape, mesh, color)


func _add_hump(center: Vector3, radius: float) -> void:
	var shape := SphereShape3D.new()
	shape.radius = radius
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	_add_static(center, Vector3.ZERO, shape, mesh, BUMP_COLOR)


func _add_static(center: Vector3, rotation_rad: Vector3, shape: Shape3D, mesh: Mesh, color: Color) -> void:
	var body := StaticBody3D.new()
	body.position = center
	body.rotation = rotation_rad
	var collision := CollisionShape3D.new()
	collision.shape = shape
	body.add_child(collision)
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	body.add_child(visual)
	add_child(body)


# A glowing orb in the middle of the walking circle: the spider's point of attention.
func _add_bait() -> Node3D:
	var sphere := SphereMesh.new()
	sphere.radius = 0.18
	sphere.height = 0.36
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.6, 1.0, 0.7)
	material.emission_enabled = true
	material.emission = Color(0.4, 1.0, 0.6)
	material.emission_energy_multiplier = 2.0
	var orb := MeshInstance3D.new()
	orb.name = "Bait"
	orb.mesh = sphere
	orb.material_override = material
	orb.position = Vector3(0, 0.7, 0)
	add_child(orb)
	return orb
