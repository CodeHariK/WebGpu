## Orb Warden boss demo (orbwarden/): the warden stalks the player orb with a laser, a stomp,
## missile volleys and a gun turret. Shoot its weak spots to take its weapons apart.
##   WASD / arrows  walk the player orb (relative to the camera)     Space  jump (over shockwaves)
##   left click (hold)  shoot where the mouse points (orb_parts.gd, orb_armory.gd):
##      power cores (4, green)   each one lost slows it down; all gone = it shuts down and collapses
##      missile pods (2)         each one lost halves the volley; both gone = no missiles
##      gun turret (top)         stops the bursts
##      laser emitter (the eye)  only open while the laser charges / fires; breaking it ends the laser
##      leg joints               stagger points: enough hits on one leg make it buckle — it reels
##   F  laser sweep now   V  stomp now   M  missile volley   N  gun burst   K  repair everything
##   T  auto attacks on/off (stomp when you're close, laser otherwise)
##   [ ]  calm mood: cute ↔ menacing (it always turns red while attacking)
##   R  clear scorch marks    P  pause the warden's walking (stand still, still attacks)
##   - =  warden walk speed slower / faster (0.25× … 10×)
##   Tab  tuning panel (orb_panel.gd: speed, gait timing, step size, body spring, debug switches)
##   B  debug gait lines (spider_debug.gd: rays, home spots + step circles, foot → home, step arcs)
##   O  debug knee poles (magenta: knee → pole)      H  footfall chart (orb_gait_chart.gd)
##   C  camera: follow the player ↔ free fly camera (fly_camera.gd: right-drag orbit, middle /
##      Shift + right-drag pan, wheel zoom, hold right mouse + WASD QE fly; trackpad works too)
##   Z  (free camera) focus on the warden
##   G  gait: phased wave (pairs l/r opposite, each pair 1/3 cycle behind) ↔ alternating tripods
## Get out of the red line before the beam comes, and jump the shockwave ring: a hit flashes the
## orb and knocks it back.
extends Node3D

const PLAYER_SPEED := 4.0
const PLAYER_HEIGHT := 0.6
const HIT_RADIUS := 0.8
const KNOCK := 6.0
const CAMERA_BACK := 6.5
const CAMERA_UP := 2.4
const GROUND := Color(0.2, 0.22, 0.25)
const ROCK := Color(0.3, 0.29, 0.3)
const SPEEDS := [0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 5.0, 7.5, 10.0] ## - = step the warden's walk speed
const MAX_SPEED := 10.0
const JUMP_SPEED := 8.0
const GRAVITY := 24.0
const AIRBORNE := 0.25 ## metres off the ground that count as "in the air" (shockwaves miss)
const FIRE_GAP := 0.12 ## seconds between shots while the button is held
const DAMAGE := 1.0
const TRACER_TIME := 0.07
const SHAKE_FADE := 2.5 ## 1/s
const SHAKE_SIZE := 0.35 ## camera offset (m) at full shake

var warden: OrbWarden
var player: MeshInstance3D
var camera: FlyCamera
var hud: Label
var debug: SpiderDebug
var chart: OrbGaitChart
var panel: OrbPanel
var hits := 0
var _player_material: StandardMaterial3D
var _flash := 0.0
var _knock := Vector3.ZERO
var _height := 0.0 ## jump height above the hover
var _vertical := 0.0 ## m/s up
var _firing := false
var _fire_wait := 0.0
var _tracers: Array = [] ## [MeshInstance3D, age]
var _shake := 0.0
var shockwave: OrbShockwave


func _ready() -> void:
	_add_environment()
	_add_box(Vector3(0, -0.5, 0), Vector3(90, 1, 90), Vector3.ZERO, GROUND)
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	for i in 14: # low rocks and slabs to walk over
		var spot := Vector3(rng.randf_range(-22, 22), 0, rng.randf_range(-22, 22))
		var size := Vector3(rng.randf_range(1.2, 3.5), rng.randf_range(0.3, 0.9), rng.randf_range(1.2, 3.5))
		_add_box(spot + Vector3.UP * size.y * 0.3, size, Vector3(rng.randf_range(-0.15, 0.15), rng.randf() * TAU, rng.randf_range(-0.15, 0.15)), ROCK)
	player = _add_player()
	player.position = Vector3(0, PLAYER_HEIGHT, 5)
	warden = OrbWarden.new()
	warden.name = "OrbWarden"
	warden.target = player
	add_child(warden)
	warden.spider.teleport(Vector3(0, 0, -9), PI)
	warden.laser.hit.connect(_on_burn)
	warden.stomped.connect(_on_stomp)
	warden.missile_blast.connect(_on_missile_blast)
	warden.gun_hit.connect(_on_gun_hit)
	shockwave = OrbShockwave.new()
	shockwave.name = "Shockwave"
	shockwave.sparks = warden.sparks
	add_child(shockwave)
	camera = FlyCamera.new()
	camera.fov = 60.0
	camera.active = false # follow camera until C
	add_child(camera)
	camera.position = player.position + Vector3(0, CAMERA_UP, CAMERA_BACK)
	camera.look_at(Vector3(0, 2, -4))
	var layer := CanvasLayer.new()
	add_child(layer)
	hud = Label.new()
	hud.position = Vector2(16, 12)
	hud.add_theme_font_size_override("font_size", 15)
	layer.add_child(hud)
	debug = SpiderDebug.new()
	debug.name = "SpiderDebug"
	debug.spider = warden.spider
	debug.cross_size = 0.25
	debug.visible = false
	add_child(debug)
	chart = OrbGaitChart.new()
	chart.spider = warden.spider
	chart.anchor_top = 1.0
	chart.anchor_bottom = 1.0
	chart.offset_left = 12.0
	chart.offset_top = -120.0
	layer.add_child(chart)
	panel = OrbPanel.new()
	panel.anchor_left = 1.0
	panel.anchor_right = 1.0
	panel.anchor_bottom = 1.0
	panel.offset_left = -OrbPanel.WIDTH - 10.0
	panel.offset_right = -10.0
	panel.offset_top = 10.0
	panel.offset_bottom = -10.0
	layer.add_child(panel)
	_fill_panel()


func _process(delta: float) -> void:
	_move_player(delta)
	_shoot(delta)
	_check_shockwave()
	if not camera.active:
		_follow_camera(delta)
	_shake = maxf(_shake - SHAKE_FADE * delta, 0.0)
	camera.h_offset = randf_range(-1.0, 1.0) * _shake * _shake * SHAKE_SIZE
	camera.v_offset = randf_range(-1.0, 1.0) * _shake * _shake * SHAKE_SIZE
	_age_tracers(delta)
	_flash = maxf(_flash - delta * 2.5, 0.0)
	var color := Color(0.4, 0.8, 1.0).lerp(Color(1, 0.15, 0.05), _flash)
	_player_material.albedo_color = color
	_player_material.emission = color
	hud.text = "WASD move   Space jump   click shoot   F laser   V stomp   M missiles   N gun   K repair   R clear scorch   - = speed %.2f×   P pause   G gait   C camera: %s   Tab panel   B O H debug" % [
		warden.walk_speed, "free" if camera.active else "follow"]
	hud.text += "\n%s   hits taken %d" % [warden.status(), hits]
	if camera.active:
		hud.text += "\nfree camera: right-drag orbit   middle / Shift + right-drag pan   wheel zoom   hold right + WASD QE fly   Z focus warden"


func _move_player(delta: float) -> void:
	if camera.flying(): # WASD flies the camera while the right mouse is held
		player.position += _knock * delta
		_knock = _knock.move_toward(Vector3.ZERO, 14.0 * delta)
		return
	_vertical -= GRAVITY * delta
	_height = maxf(_height + _vertical * delta, 0.0)
	if _height <= 0.0:
		_vertical = 0.0
	var input := Vector2(
		float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT)) - float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)),
		float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN)))
	var forward := (-camera.global_basis.z * Vector3(1, 0, 1)).normalized()
	var right := (camera.global_basis.x * Vector3(1, 0, 1)).normalized()
	var move := (right * input.x + forward * input.y).limit_length(1.0) * PLAYER_SPEED
	player.position += (move + _knock) * delta
	_knock = _knock.move_toward(Vector3.ZERO, 14.0 * delta)
	player.position.y = PLAYER_HEIGHT + _height + sin(Time.get_ticks_msec() * 0.003) * 0.08


func _airborne() -> bool:
	return _height > AIRBORNE


func _jump() -> void:
	if not _airborne():
		_vertical = JUMP_SPEED


# Hold the left button to keep firing where the mouse points (through the camera): the first
# unbroken joint near that line takes the hit; otherwise the bolt hits the ground (or flies off).
func _shoot(delta: float) -> void:
	_fire_wait -= delta
	if not _firing or _fire_wait > 0.0:
		return
	_fire_wait = FIRE_GAP
	var mouse := get_viewport().get_mouse_position()
	var from := camera.project_ray_origin(mouse)
	var direction := camera.project_ray_normal(mouse)
	var muzzle := player.position + Vector3.UP * 0.2
	var end := from + direction * 80.0
	var joint := warden.limbs.pick(from, direction)
	var joint_along := (warden.limbs.joint_position(joint) - from).dot(direction) if joint >= 0 else INF
	var part := warden.parts.pick(from, direction)
	var part_along: float = part.along if not part.is_empty() else INF
	if not part.is_empty() and part_along <= joint_along: # whichever is in front
		var hit_part: OrbParts.Part = part.part
		end = hit_part.position()
		warden.parts.hit(hit_part, DAMAGE, (end - muzzle).normalized())
	elif joint >= 0:
		end = warden.limbs.joint_position(joint)
		warden.limbs.hit(joint, DAMAGE, (end - muzzle).normalized())
	else:
		var query := PhysicsRayQueryParameters3D.create(from, end, 1)
		var ground := get_world_3d().direct_space_state.intersect_ray(query)
		if not ground.is_empty():
			end = ground.position
			warden.sparks.burst(end, ground.normal, 5, 4.0, 0.7, 0.25, 0.025, end.y)
	_add_tracer(muzzle, end)


func _add_tracer(from: Vector3, to: Vector3) -> void:
	var length := from.distance_to(to)
	if length < 0.1:
		return
	var mesh := CylinderMesh.new()
	mesh.top_radius = 0.03
	mesh.bottom_radius = 0.03
	mesh.height = 1.0
	mesh.radial_segments = 6
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.albedo_color = Color(0.6, 0.95, 1.0)
	var tracer := MeshInstance3D.new()
	tracer.mesh = mesh
	tracer.material_override = material
	tracer.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(tracer)
	var y := (to - from) / length
	var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
	tracer.global_transform = Transform3D(Basis(x, y * length, x.cross(y)), (from + to) * 0.5)
	_tracers.append([tracer, 0.0])


func _age_tracers(delta: float) -> void:
	for i in range(_tracers.size() - 1, -1, -1):
		_tracers[i][1] += delta
		var tracer: MeshInstance3D = _tracers[i][0]
		var t: float = _tracers[i][1] / TRACER_TIME
		if t >= 1.0:
			tracer.queue_free()
			_tracers.remove_at(i)
		else:
			var material := tracer.material_override as StandardMaterial3D
			material.albedo_color = Color(material.albedo_color, 1.0 - t)


# A shockwave front crossing the grounded player: knocked away and popped up.
func _check_shockwave() -> void:
	var push := shockwave.check_hit(player.position, _airborne())
	if push != Vector3.ZERO:
		hits += 1
		_flash = 1.0
		_knock = push * KNOCK * 1.5
		_vertical = 5.0


func _hurt(push: Vector3, pop := 0.0) -> void:
	hits += 1
	_flash = 1.0
	_knock = push
	_vertical = maxf(_vertical, pop)


func _on_missile_blast(at: Vector3, radius: float) -> void:
	var offset := (player.position - at) * Vector3(1, 0, 1)
	_shake = maxf(_shake, 0.35)
	if offset.length() < radius and _height < 1.5:
		_hurt((offset.normalized() if offset.length() > 0.05 else Vector3.BACK) * KNOCK, 4.0)


func _on_gun_hit(direction: Vector3) -> void:
	_hurt((direction * Vector3(1, 0, 1)).normalized() * KNOCK * 0.4)


func _on_stomp(at: Vector3) -> void:
	shockwave.spawn(at)
	var near := clampf(1.0 - camera.global_position.distance_to(at) / 40.0, 0.3, 1.0)
	_shake = maxf(_shake, near)
	warden.sparks.burst(at, Vector3.UP, 40, 8.0, 1.0, 0.6, 0.05, at.y, Color(1.0, 0.8, 0.5))
	warden.sparks.embers(at, 8, 4.0, at.y)


# Behind the player, on the far side from the warden, looking between them.
func _follow_camera(delta: float) -> void:
	var boss := warden.spider.rig.skeleton.global_position
	var away := (player.position - boss) * Vector3(1, 0, 1)
	away = away.normalized() if away.length() > 0.1 else Vector3.BACK
	var spot := player.position + away * CAMERA_BACK + Vector3.UP * CAMERA_UP
	camera.global_position = camera.global_position.lerp(spot, 1.0 - exp(-2.5 * delta))
	camera.look_at(player.position.lerp(boss, 0.5) + Vector3.UP * 1.4)


func _on_burn(at: Vector3) -> void:
	var offset := (player.position - at) * Vector3(1, 0, 1)
	if offset.length() < HIT_RADIUS and _flash < 0.5:
		hits += 1
		_flash = 1.0
		_knock = (offset.normalized() if offset.length() > 0.05 else Vector3.BACK) * KNOCK


func _unhandled_input(event: InputEvent) -> void:
	var button := event as InputEventMouseButton
	if button != null and button.button_index == MOUSE_BUTTON_LEFT:
		_firing = button.pressed
		if button.pressed:
			_fire_wait = 0.0
		return
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_F: warden.attack()
		KEY_V: warden.stomp_now()
		KEY_M: warden.missiles_now()
		KEY_N: warden.gun_now()
		KEY_K: warden.repair()
		KEY_SPACE: _jump()
		KEY_T: warden.auto_attack = not warden.auto_attack
		KEY_BRACKETLEFT: warden.calm_menace = maxf(warden.calm_menace - 0.25, 0.0)
		KEY_BRACKETRIGHT: warden.calm_menace = minf(warden.calm_menace + 0.25, 1.0)
		KEY_R:
			warden.laser.clear()
			shockwave.clear()
		KEY_G: warden.spider.wave_gait = not warden.spider.wave_gait
		KEY_P: warden.paused = not warden.paused
		KEY_MINUS: _step_speed(-1)
		KEY_EQUAL: _step_speed(1)
		KEY_C: _set_free_camera(not camera.active)
		KEY_TAB: panel.visible = not panel.visible
		KEY_H: chart.visible = not chart.visible
		KEY_B: _set_debug(not (debug.visible and debug.show_gait), debug.visible and debug.show_poles)
		KEY_O: _set_debug(debug.visible and debug.show_gait, not (debug.visible and debug.show_poles))
		KEY_Z:
			if camera.active:
				camera.focus(warden.spider.rig.skeleton.global_position)


# The next listed speed above / below the current one (the slider can leave it between two).
func _step_speed(direction: int) -> void:
	var speed := warden.walk_speed
	if direction > 0:
		for listed: float in SPEEDS:
			if listed > speed + 0.001:
				warden.walk_speed = listed
				return
	else:
		for i in range(SPEEDS.size() - 1, -1, -1):
			if SPEEDS[i] < speed - 0.001:
				warden.walk_speed = SPEEDS[i]
				return


func _fill_panel() -> void:
	var spider := warden.spider
	panel.heading("Warden")
	panel.slider("walk speed ×", 0.0, MAX_SPEED, 0.05, func() -> float: return warden.walk_speed, func(v: float) -> void: warden.walk_speed = v)
	panel.checkbox("paused (P)", func() -> bool: return warden.paused, func(on: bool) -> void: warden.paused = on)
	panel.checkbox("auto attack (T)", func() -> bool: return warden.auto_attack, func(on: bool) -> void: warden.auto_attack = on)
	panel.slider("calm mood: cute → menace", 0.0, 1.0, 0.05, func() -> float: return warden.calm_menace, func(v: float) -> void: warden.calm_menace = v)
	panel.heading("Attacks")
	panel.button("laser sweep (F)", func() -> void: warden.attack())
	panel.button("stomp (V)", func() -> void: warden.stomp_now())
	panel.button("missile volley (M)", func() -> void: warden.missiles_now())
	panel.button("gun burst (N)", func() -> void: warden.gun_now())
	panel.checkbox("gun fires on its own", func() -> bool: return warden.gun_auto, func(on: bool) -> void: warden.gun_auto = on)
	panel.button("repair everything (K)", func() -> void: warden.repair())
	panel.checkbox("legs break off (old design)", func() -> bool: return warden.limbs.breakable, func(on: bool) -> void: warden.limbs.breakable = on)
	panel.heading("Gait")
	panel.checkbox("wave gait (G) — off = tripods", func() -> bool: return spider.wave_gait, func(on: bool) -> void: spider.wave_gait = on)
	panel.slider("pair delay (cycles)", 0.0, 1.0, 0.01, func() -> float: return warden.pair_delay, func(v: float) -> void: warden.pair_delay = v)
	panel.slider("right after left (cycles)", 0.0, 1.0, 0.01, func() -> float: return warden.side_offset, func(v: float) -> void: warden.side_offset = v)
	panel.slider("planted share (duty)", 0.3, 0.95, 0.01, func() -> float: return _layout().wave_duty, func(v: float) -> void: _layout().wave_duty = v)
	panel.slider("step height (m)", 0.1, 2.5, 0.05, func() -> float: return spider.step_height, func(v: float) -> void: spider.step_height = v)
	panel.slider("stride (m)", 0.3, 3.0, 0.05, func() -> float: return spider.step_distance, func(v: float) -> void: spider.step_distance = v)
	panel.slider("lead time (s)", 0.0, 1.0, 0.01, func() -> float: return spider.lead_time, func(v: float) -> void: spider.lead_time = v)
	panel.heading("Body")
	panel.slider("spring stiffness", 10.0, 400.0, 5.0, func() -> float: return _layout().body_spring.x, func(v: float) -> void: _set_body_spring(Vector2(v, _layout().body_spring.y)), "%.0f")
	panel.slider("spring damping", 1.0, 60.0, 0.5, func() -> float: return _layout().body_spring.y, func(v: float) -> void: _set_body_spring(Vector2(_layout().body_spring.x, v)), "%.1f")
	panel.heading("Debug")
	panel.checkbox("gait lines (B)", func() -> bool: return debug.visible and debug.show_gait, func(on: bool) -> void: _set_debug(on, debug.show_poles and debug.visible))
	panel.checkbox("knee poles (O)", func() -> bool: return debug.visible and debug.show_poles, func(on: bool) -> void: _set_debug(debug.show_gait and debug.visible, on))
	panel.checkbox("use knee poles in the IK", func() -> bool: return spider.use_knee_poles, func(on: bool) -> void: spider.use_knee_poles = on)
	panel.checkbox("footfall chart (H)", func() -> bool: return chart.visible, func(on: bool) -> void: chart.visible = on)
	panel.checkbox("free camera (C)", func() -> bool: return camera.active, func(on: bool) -> void: _set_free_camera(on))


func _layout() -> SpiderLayout:
	return warden.spider.rig.layout


func _set_body_spring(spring: Vector2) -> void:
	_layout().body_spring = spring


func _set_debug(gait: bool, poles: bool) -> void:
	debug.show_gait = gait
	debug.show_poles = poles
	debug.visible = gait or poles


func _set_free_camera(on: bool) -> void:
	camera.active = on
	if on:
		camera.take_over(camera.global_position.distance_to(warden.spider.rig.skeleton.global_position))


func _add_player() -> MeshInstance3D:
	var ball := SphereMesh.new()
	ball.radius = 0.35
	ball.height = 0.7
	_player_material = StandardMaterial3D.new()
	_player_material.emission_enabled = true
	_player_material.emission_energy_multiplier = 2.0
	var instance := MeshInstance3D.new()
	instance.mesh = ball
	instance.material_override = _player_material
	add_child(instance)
	var light := OmniLight3D.new()
	light.light_color = Color(0.4, 0.8, 1.0)
	light.omni_range = 3.0
	instance.add_child(light)
	return instance


func _add_environment() -> void:
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color(0.06, 0.07, 0.12)
	sky_material.sky_horizon_color = Color(0.32, 0.22, 0.24)
	sky_material.ground_horizon_color = Color(0.2, 0.16, 0.18)
	sky_material.ground_bottom_color = Color(0.05, 0.05, 0.06)
	var sky := Sky.new()
	sky.sky_material = sky_material
	var environment := Environment.new()
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.ambient_light_energy = 0.6
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.glow_enabled = true
	environment.glow_intensity = 0.9
	environment.glow_bloom = 0.1
	environment.fog_enabled = true
	environment.fog_light_color = Color(0.22, 0.17, 0.2)
	environment.fog_density = 0.012
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)
	var sun := DirectionalLight3D.new()
	sun.rotation = Vector3(deg_to_rad(-50), deg_to_rad(30), 0)
	sun.light_color = Color(1.0, 0.82, 0.7)
	sun.light_energy = 0.9
	sun.shadow_enabled = true
	add_child(sun)


func _add_box(center: Vector3, size: Vector3, rotation_rad: Vector3, color: Color) -> void:
	var body := StaticBody3D.new()
	body.position = center
	body.rotation = rotation_rad
	var shape := BoxShape3D.new()
	shape.size = size
	var collision := CollisionShape3D.new()
	collision.shape = shape
	body.add_child(collision)
	var mesh := BoxMesh.new()
	mesh.size = size
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.95
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	body.add_child(visual)
	add_child(body)
