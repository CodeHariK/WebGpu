## Demo world for the crawler (spider × cultist hybrid): a floor with a crate to climb over, a
## pillar to climb up, a low tunnel to crawl into and onto its ceiling, a ramp and a glowing orb
## it watches.
##   W/S forward/back · A/D turn       G chase the orb on/off (it steers itself)
##   Space leap at the orb             K knock it (a blow from the camera's direction)
##   O move the orb to the next spot   R put it back at the start
##   B debug lines on/off              mouse wheel: camera distance
## Right: a panel with every debug layer and the main tuning numbers (crawler_debug.gd draws the
## rays, homes, foothold candidates, step arcs and IK; crawler_chart.gd is the footfall chart).
##
## Test run: make hybrid ARGS=--shot  (or Godot --path . hybrid/hybrid_demo.tscn -- --shot)
##   an autopilot walks it over the crate, up the pillar, knocks it off, sends it up the tunnel
##   wall onto the ceiling, leaps it at the orb and chases the orb; screenshots go to
##   "Claude outputs/hybrid/" and each move prints the crawler's state
extends Node3D

const ORB_SPOTS := [Vector3(0, 0.5, 6), Vector3(0, 1.5, -6), Vector3(6, 0.5, 5), Vector3(-6, 0.5, -6)]
const START := Vector3(0, 0, 2)
const SHOT_DIR := "res://Claude outputs/hybrid"

## Autopilot: [name, place at (or null), facing, ahead, turn, action, seconds, screenshot, camera offset]
## action: "" | "leap" | "knock" | "chase" | "stop"
const MOVES := [
	["standing", START, Vector3.FORWARD, 0.0, 0.0, "", 1.0, true, Vector3(2.0, 1.2, 2.5)],
	["walk to the crate", null, null, 1.0, 0.0, "", 2.6, true, Vector3(2.5, 1.0, 1.0)],
	["climb the crate", null, null, 1.0, 0.0, "", 0.9, true, Vector3(3.0, 1.2, 0.5)],
	["over the top", null, null, 1.0, 0.0, "", 1.0, true, Vector3(3.0, 1.5, 0.5)],
	["down the far side", null, null, 1.0, 0.0, "", 1.4, true, Vector3(3.0, 1.0, -1.0)],
	["walk to the pillar", Vector3(5, 0, 0), Vector3.FORWARD, 1.0, 0.0, "", 1.6, false, Vector3(3.0, 1.2, 1.0)],
	["up the pillar", null, null, 1.0, 0.0, "", 1.0, true, Vector3(3.0, 1.0, 2.5)],
	["knocked off", null, null, 0.0, 0.0, "knock", 0.35, true, Vector3(3.0, 1.0, 3.0)],
	["falling", null, null, 0.0, 0.0, "", 0.6, false, Vector3(3.0, 1.0, 3.0)],
	["landed", null, null, 0.0, 0.0, "", 1.4, true, Vector3(3.0, 1.5, 3.0)],
	["along the roof", Vector3(-6, 1.7, 1.5), Vector3.FORWARD, 1.0, 0.0, "", 2.2, true, Vector3(2.0, 1.4, 1.5)],
	["over the roof's end", null, null, 1.0, 0.0, "", 1.0, true, Vector3(2.5, 0.8, -1.0)],
	["backing over the roof's end", Vector3(-6, 1.7, -1.6), Vector3.BACK, -1.0, 0.0, "", 1.5, true, Vector3(2.5, 0.8, -1.5)],
	["backing off the crate", Vector3(0, 1.0, -6), Vector3.BACK, -1.0, 0.0, "", 1.0, true, Vector3(2.5, 1.2, -2.0)],
	["backed down to the floor", null, null, -1.0, 0.0, "", 1.6, true, Vector3(2.5, 1.0, -2.5)],
	["into the tunnel", Vector3(-5.6, 0, 2), Vector3.LEFT, 1.0, 0.0, "", 0.4, false, Vector3(0.0, 0.6, 3.0)],
	["up the tunnel wall", null, null, 1.0, 0.0, "", 0.6, true, Vector3(0.5, 0.4, 2.5)],
	["onto the ceiling", null, null, 1.0, 0.0, "", 0.7, true, Vector3(0.5, 0.2, 2.5)],
	["along the ceiling", null, null, 1.0, -1.0, "", 0.5, false, Vector3(0.5, 0.2, 2.5)],
	["out along the ceiling", null, null, 1.0, 0.0, "", 1.2, true, Vector3(0.5, 0.2, 2.5)],
	["leap at the orb", Vector3(0, 0, 1), Vector3.BACK, 0.0, 0.0, "leap", 0.45, true, Vector3(3.0, 1.0, 0.0)],
	["mid-air", null, null, 0.0, 0.0, "", 0.25, true, Vector3(3.0, 1.0, 0.0)],
	["after the leap", null, null, 0.0, 0.0, "", 1.5, true, Vector3(3.0, 1.5, 0.0)],
	["chase the orb", null, null, 0.0, 0.0, "chase", 4.0, true, Vector3(3.0, 2.0, 3.0)],
]

var crawler: Crawler
var debug := CrawlerDebug.new()
var chart := CrawlerChart.new()
var panel := OrbPanel.new()
var orb := MeshInstance3D.new()
var camera := Camera3D.new()
var hud := Label.new()
var cam_offset := Vector3(2.5, 1.6, 3.0)
var _orb_spot := 0
var _auto := false
var _move := -1
var _time := -0.5
var _shot := 0
var _skip_frame := false


func _ready() -> void:
	_environment()
	_arena()
	_make_orb()
	crawler = Crawler.new()
	crawler.name = "Crawler"
	crawler.position = START
	crawler.look_target = orb
	add_child(crawler)
	add_child(camera)
	var layer := CanvasLayer.new()
	hud.position = Vector2(12, 8)
	hud.add_theme_color_override("font_outline_color", Color.BLACK)
	hud.add_theme_constant_override("outline_size", 4)
	layer.add_child(hud)
	add_child(layer)
	debug.crawler = crawler
	add_child(debug)
	chart.crawler = crawler
	chart.position = Vector2(12, 600)
	layer.add_child(chart)
	_build_panel(layer)
	_auto = "--shot" in OS.get_cmdline_user_args()
	await get_tree().physics_frame
	crawler.place(START, Vector3.FORWARD)


func _physics_process(delta: float) -> void:
	if _auto:
		_autopilot(delta)
	else:
		crawler.ahead = Input.get_axis("ui_down", "ui_up") if not Input.is_physical_key_pressed(KEY_W) and not Input.is_physical_key_pressed(KEY_S) else (1.0 if Input.is_physical_key_pressed(KEY_W) else -1.0)
		crawler.turn = (1.0 if Input.is_physical_key_pressed(KEY_A) else 0.0) - (1.0 if Input.is_physical_key_pressed(KEY_D) else 0.0)


func _process(_delta: float) -> void:
	var body := crawler.pose.transform.origin
	camera.global_position = camera.global_position.lerp(body + cam_offset, 0.15)
	camera.look_at(body, Vector3.UP)
	hud.text = _status()
	_process_chart()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			cam_offset *= 0.9
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			cam_offset *= 1.1
	if not (event is InputEventKey and event.pressed and not event.echo):
		return
	match event.physical_keycode:
		KEY_SPACE: crawler.leap(orb.global_position)
		KEY_K: _knock()
		KEY_G: crawler.goal = null if crawler.goal is Vector3 else orb.global_position
		KEY_O: _next_orb_spot()
		KEY_R: crawler.place(START, Vector3.FORWARD)
		KEY_B: debug.visible = not debug.visible


func _process_chart() -> void:
	chart.position = Vector2(12, get_viewport().get_visible_rect().size.y - chart.custom_minimum_size.y - 12)


## The panel on the right: debug layers, then tuning, then actions. Every row reads and writes
## the live value, so keys and the panel always agree.
func _build_panel(layer: CanvasLayer) -> void:
	var gait := crawler.gait
	var walker := crawler.walker
	var foothold := gait.foothold
	panel.set_anchors_preset(Control.PRESET_RIGHT_WIDE)
	panel.offset_left = -OrbPanel.WIDTH - 10
	panel.offset_top = 10
	panel.offset_bottom = -10
	panel.offset_right = -10
	layer.add_child(panel)
	panel.heading("Debug")
	panel.checkbox("debug lines (B)", func() -> bool: return debug.visible, func(on: bool) -> void: debug.visible = on)
	panel.checkbox("ALL rays (fading)", func() -> bool: return debug.show_rays, func(on: bool) -> void: debug.show_rays = on)
	for kind in CrawlerRays.NAMES.size():
		panel.checkbox("   %s rays" % CrawlerRays.NAMES[kind], func() -> bool: return debug.show_kind[kind], func(on: bool) -> void: debug.show_kind[kind] = on)
	panel.slider("foothold ray linger (s)", 0.0, 3.0, 0.05, func() -> float: return crawler.rays.linger, func(v: float) -> void: crawler.rays.linger = v)
	panel.checkbox("surface frame + velocity", func() -> bool: return debug.show_walker, func(on: bool) -> void: debug.show_walker = on)
	panel.checkbox("homes + step circles", func() -> bool: return debug.show_homes, func(on: bool) -> void: debug.show_homes = on)
	panel.checkbox("foothold candidates", func() -> bool: return debug.show_footholds, func(on: bool) -> void: debug.show_footholds = on)
	panel.checkbox("step arcs", func() -> bool: return debug.show_arcs, func(on: bool) -> void: debug.show_arcs = on)
	panel.checkbox("IK poles + misses", func() -> bool: return debug.show_ik, func(on: bool) -> void: debug.show_ik = on)
	panel.checkbox("footfall chart", func() -> bool: return chart.visible, func(on: bool) -> void: chart.visible = on)
	panel.heading("Walk")
	panel.slider("speed (m/s)", 0.2, 3.0, 0.1, func() -> float: return walker.speed, func(v: float) -> void: walker.speed = v)
	panel.slider("turn rate (rad/s)", 0.5, 6.0, 0.1, func() -> float: return walker.turn_rate, func(v: float) -> void: walker.turn_rate = v)
	panel.heading("Gait")
	panel.slider("step distance (m)", 0.1, 0.6, 0.01, func() -> float: return gait.step_distance, func(v: float) -> void: gait.step_distance = v)
	panel.slider("lead time (s)", 0.0, 0.8, 0.01, func() -> float: return gait.lead_time, func(v: float) -> void: gait.lead_time = v)
	panel.slider("step time (s)", 0.08, 0.6, 0.01, func() -> float: return gait.step_time, func(v: float) -> void: gait.step_time = v)
	panel.slider("arc lift (m)", 0.02, 0.4, 0.01, func() -> float: return gait.steps[0].lift, _set_lift)
	panel.heading("Footholds")
	panel.checkbox("search candidates (off: 1 ray)", func() -> bool: return foothold.enabled, func(on: bool) -> void: foothold.enabled = on)
	panel.slider("ring radius (m)", 0.05, 0.4, 0.01, func() -> float: return foothold.radius, func(v: float) -> void: foothold.radius = v)
	panel.slider("max reach (share)", 0.6, 1.0, 0.01, func() -> float: return foothold.max_reach, func(v: float) -> void: foothold.max_reach = v)
	panel.slider("tilt cost", 0.0, 1.0, 0.05, func() -> float: return foothold.tilt_cost, func(v: float) -> void: foothold.tilt_cost = v)
	panel.heading("Body")
	panel.slider("dip per step (m)", 0.0, 0.1, 0.005, func() -> float: return crawler.pose.dip_per_step, func(v: float) -> void: crawler.pose.dip_per_step = v, "%.3f")
	panel.slider("max tilt (rad)", 0.0, 1.2, 0.05, func() -> float: return crawler.pose.max_tilt, func(v: float) -> void: crawler.pose.max_tilt = v)
	panel.heading("Actions")
	panel.button("leap at the orb (Space)", func() -> void: crawler.leap(orb.global_position))
	panel.button("knock it (K)", _knock)
	panel.button("chase the orb on/off (G)", func() -> void: crawler.goal = null if crawler.goal is Vector3 else orb.global_position)
	panel.button("move the orb (O)", _next_orb_spot)
	panel.button("reset (R)", func() -> void: crawler.place(START, Vector3.FORWARD))


func _set_lift(value: float) -> void:
	for step in crawler.gait.steps:
		step.lift = value


func _knock() -> void:
	var from := camera.global_position
	var at := crawler.ball.global_position
	crawler.hit(at, (at - from).normalized() * 12.0 + Vector3.UP * 2.0)


func _next_orb_spot() -> void:
	_orb_spot = (_orb_spot + 1) % ORB_SPOTS.size()
	orb.position = ORB_SPOTS[_orb_spot]
	if crawler.goal is Vector3:
		crawler.goal = orb.global_position


func _status() -> String:
	var errors := crawler.rig.errors
	return "Crawler (spider × cultist)   W/S A/D move · G chase · Space leap · K knock · O orb · R reset\n" + \
		"%s · on %s · %.1f m/s · stepping %d · IK miss %.2f %.2f %.2f %.2f m · %d fps%s" % [
		"WALK" if crawler.mode == Crawler.Mode.WALK else "TUMBLE",
		_surface_name(), crawler.walker.velocity.length() if crawler.mode == Crawler.Mode.WALK else crawler.ball.linear_velocity.length(),
		crawler.gait.stepping_count(), errors[0], errors[1], errors[2], errors[3], Engine.get_frames_per_second(),
		"\nautopilot: " + MOVES[_move][0] if _auto and _move >= 0 and _move < MOVES.size() else ""] + "\n" + _ray_counts()


## "rays: 9 this frame (walker 2 · home 4 · foothold 3 · …) · 640/s"
func _ray_counts() -> String:
	var rays := crawler.rays
	var parts: PackedStringArray = []
	var per_second := 0.0
	for kind in CrawlerRays.NAMES.size():
		parts.append("%s %d" % [CrawlerRays.NAMES[kind], rays.this_frame[kind]])
		per_second += rays.per_second[kind]
	return "rays: %d this frame (%s) · ~%.0f/s" % [rays.total_this_frame(), " · ".join(parts), per_second]


func _surface_name() -> String:
	var up := crawler.walker.up().dot(Vector3.UP)
	return "floor" if up > 0.7 else ("ceiling" if up < -0.7 else "wall")


# --- autopilot ----------------------------------------------------------------------------------

func _autopilot(delta: float) -> void:
	if _skip_frame:
		_skip_frame = false
		delta = 0.0
	_time += delta
	if _move >= 0 and _time < MOVES[_move][6]:
		return
	if _move >= 0:
		_finish(MOVES[_move])
	_move += 1
	if _move >= MOVES.size():
		get_tree().quit()
		return
	_start(MOVES[_move])


func _start(move: Array) -> void:
	_time = 0.0
	if move[1] != null:
		crawler.goal = null
		crawler.place(move[1], move[2])
	crawler.ahead = move[3]
	crawler.turn = move[4]
	cam_offset = move[8]
	match move[5]:
		"leap": crawler.leap(orb.global_position)
		"knock": crawler.hit(crawler.ball.global_position, Vector3(0, 3, 14))
		"chase":
			_next_orb_spot()
			crawler.goal = orb.global_position


func _finish(move: Array) -> void:
	var at := crawler.pose.transform.origin
	print("%-22s → body (%.2f, %.2f, %.2f)  %s on %s  stepping %d  IK miss max %.2f" % [move[0], at.x, at.y, at.z,
		"WALK" if crawler.mode == Crawler.Mode.WALK else "TUMBLE", _surface_name(), crawler.gait.stepping_count(),
		_max(crawler.rig.errors)])
	if move[7]:
		DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(SHOT_DIR))
		RenderingServer.force_draw()
		get_viewport().get_texture().get_image().save_png("%s/hybrid_%d.png" % [SHOT_DIR, _shot])
		_shot += 1
		_skip_frame = true


static func _max(values: PackedFloat32Array) -> float:
	var top := 0.0
	for value in values:
		top = maxf(top, value)
	return top


# --- world --------------------------------------------------------------------------------------

func _environment() -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.1, 0.11, 0.13)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.55, 0.58, 0.65)
	env.ambient_light_energy = 0.5
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-55), deg_to_rad(35), 0)
	sun.light_energy = 1.2
	sun.shadow_enabled = true
	add_child(sun)


func _arena() -> void:
	_block(Vector3(0, -0.5, 0), Vector3(30, 1, 30), Color(0.36, 0.4, 0.34)) # floor
	_block(Vector3(0, 0.5, -6), Vector3(2, 1, 2), Color(0.55, 0.42, 0.3)) # crate to climb over
	_block(Vector3(5, 1.5, -3), Vector3(1, 3, 1), Color(0.5, 0.5, 0.55)) # pillar to climb
	for x in [-7.2, -4.8]: # tunnel walls (inside x -7.05 .. -4.95), 1.4 m high
		_block(Vector3(x, 0.7, 0), Vector3(0.3, 1.4, 6), Color(0.45, 0.42, 0.5))
	_block(Vector3(-6, 1.55, 0), Vector3(2.7, 0.3, 6), Color(0.4, 0.38, 0.46)) # tunnel roof
	var ramp := _block(Vector3(6, 0.35, 6), Vector3(3, 0.2, 4), Color(0.5, 0.45, 0.35)) # a ramp
	ramp.rotation.x = deg_to_rad(-15)


func _block(at: Vector3, size: Vector3, color: Color) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.position = at
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	body.add_child(shape)
	var mesh := MeshInstance3D.new()
	var visual := BoxMesh.new()
	visual.size = size
	mesh.mesh = visual
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	mesh.material_override = material
	body.add_child(mesh)
	add_child(body)
	return body


func _make_orb() -> void:
	var sphere := SphereMesh.new()
	sphere.radius = 0.18
	sphere.height = 0.36
	orb.mesh = sphere
	var glow := StandardMaterial3D.new()
	glow.albedo_color = Color(1.0, 0.85, 0.5)
	glow.emission_enabled = true
	glow.emission = Color(1.0, 0.75, 0.4)
	glow.emission_energy_multiplier = 2.0
	orb.material_override = glow
	orb.position = ORB_SPOTS[0]
	add_child(orb)
