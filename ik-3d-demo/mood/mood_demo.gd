## Theme demo (mood/): one lantern-lit path through a forest, dressed and lit eight ways —
## spooky, candy, desert, autumn, cyberpunk, solarpunk, medieval, magical. The layout never
## changes; each theme swaps the prop kit (MoodKit), the particles (MoodParticles) and every
## lighting number (MoodTheme), blending from one to the next over 1.5 s (MoodLook).
##   , .   previous / next theme (or the buttons on the panel)
##   1-7   switch a layer off/on: 1 fog  2 colour grade  3 glow  4 lamps  5 particles  6 vignette
##         7 ambient colour — see what each one adds
##   WASD  walk the stand-in character; the camera (MoodCamera) follows with the theme's feel —
##         fov, distance, height, pitch, lag, handheld sway, Dutch roll, depth of field, lens
##         fringing — and three camera zones on the path change it locally:
##         overhead (z 15), vista (z 33: pull back, wide), corridor (z 50: close and low)
##   V     show the camera zones (rings on the ground)     X  camera shake
##   C     free fly camera (fly_camera.gd)      Tab  panel (tune the current theme live)
extends Node3D

const PATH_LENGTH := 70.0
const WALK_SPEED := 4.0
const LAYERS := ["use_fog", "use_grade", "use_glow", "use_lanterns", "use_embers", "use_vignette", "use_ambient"]
const LAYER_NAMES := ["fog", "colour grade", "glow", "lamps", "particles", "vignette", "ambient colour"]

var look: MoodLook
var camera: FlyCamera ## free camera (C)
var rig: MoodCamera ## the follow camera
var zones_visible := false
var character: Node3D
var panel: OrbPanel
var hud: Label
var layout := {}
var _props: Node3D
var _rng := RandomNumberGenerator.new()


func _ready() -> void:
	_rng.seed = 5
	var particles := _add_particles()
	look = MoodLook.new()
	look.name = "Look"
	look.environment = _add_environment()
	look.sky_material = look.environment.sky.sky_material as ProceduralSkyMaterial
	look.sun = _add_sun()
	look.embers = particles
	var layer := CanvasLayer.new()
	add_child(layer)
	var lens := _add_screen_shader(layer, "res://mood/lens.gdshader")
	look.vignette_material = _add_screen_shader(layer, "res://mood/vignette.gdshader")
	add_child(look)
	look.theme_changed.connect(_dress)
	_add_ground()
	layout = _layout()
	_dress(look.theme)
	character = _add_character()
	rig = MoodCamera.new()
	rig.name = "Rig"
	rig.target = character
	rig.look = look
	rig.lens_material = lens
	add_child(rig)
	rig.camera.current = true
	_add_zones()
	camera = FlyCamera.new()
	camera.fov = 55.0
	camera.active = false
	add_child(camera)
	hud = Label.new()
	hud.position = Vector2(16, 12)
	hud.add_theme_font_size_override("font_size", 15)
	layer.add_child(hud)
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
	_walk(delta)
	var eye: Camera3D = camera if camera.active else rig.camera
	look.embers.global_position = eye.global_position - eye.global_basis.z * 6.0
	var off: Array[String] = []
	for i in LAYERS.size():
		if not look.get(LAYERS[i]):
			off.append(LAYER_NAMES[i])
	hud.text = "theme: %s   , . switch   1-7 layers   WASD walk   V camera zones   X shake   C free camera   Tab panel" % look.theme.name.to_upper()
	if rig.active_zone != "":
		hud.text += "\ncamera zone: " + rig.active_zone
	if not off.is_empty():
		hud.text += "\nswitched off: " + ", ".join(off)


func switch(step: int) -> void:
	var index := MoodTheme.NAMES.find(look.theme.name)
	look.switch_to(MoodTheme.NAMES[posmod(index + step, MoodTheme.NAMES.size())])


func _walk(delta: float) -> void:
	if camera.flying():
		return
	var turn := float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)) - float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT))
	var forward := float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN))
	character.rotate_y(turn * 2.2 * delta)
	character.global_position += -character.global_basis.z * forward * WALK_SPEED * delta
	character.get_child(0).position.y = absf(sin(Time.get_ticks_msec() * 0.012)) * 0.06 * absf(forward)


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	if key.keycode >= KEY_1 and key.keycode <= KEY_7:
		var layer: String = LAYERS[key.keycode - KEY_1]
		look.set(layer, not look.get(layer))
		return
	match key.keycode:
		KEY_COMMA: switch(-1)
		KEY_PERIOD: switch(1)
		KEY_C: _set_free_camera(not camera.active)
		KEY_V: _show_zones(not zones_visible)
		KEY_X: rig.shake(1.0)
		KEY_TAB: panel.visible = not panel.visible


# --- panel ---------------------------------------------------------------------------------------

func _fill_panel() -> void:
	panel.heading("Theme (, .)")
	for theme_name: String in MoodTheme.NAMES:
		panel.button(theme_name, func() -> void: look.switch_to(theme_name))
	panel.heading("Layers (1-7)")
	for i in LAYERS.size():
		var layer: String = LAYERS[i]
		panel.checkbox("%d  %s" % [i + 1, LAYER_NAMES[i]], func() -> bool: return look.get(layer), func(on: bool) -> void: look.set(layer, on))
	panel.heading("Fog (this theme)")
	_color_slider("fog hue", "fog", 0)
	_color_slider("fog saturation", "fog", 1)
	_color_slider("fog brightness", "fog", 2)
	_slider("fog begins (m)", "fog_begin", 0.0, 60.0, 0.5)
	_slider("fog solid at (m)", "fog_end", 10.0, 200.0, 1.0)
	_slider("ground fog", "fog_height_density", 0.0, 1.0, 0.01)
	panel.heading("Light")
	_color_slider("ambient hue", "ambient", 0)
	_slider("ambient energy", "ambient_energy", 0.0, 2.0, 0.05)
	_slider("sun / moon energy", "sun_energy", 0.0, 3.0, 0.05)
	_slider("sun / moon height (°)", "sun_pitch", -85.0, -5.0, 1.0)
	_slider("sun / moon direction (°)", "sun_yaw", 0.0, 360.0, 1.0)
	_color_slider("lamp hue", "lamp", 0)
	_slider("lamp energy", "lamp_energy", 0.0, 8.0, 0.1)
	_slider("lamp range (m)", "lamp_range", 1.0, 10.0, 0.1)
	_slider("lamp flicker", "lamp_flicker", 0.0, 1.0, 0.05)
	panel.heading("Air")
	_slider("wind (trees, flags)", "wind", 0.0, 1.0, 0.01)
	_slider("wind direction (°)", "wind_dir", 0.0, 360.0, 1.0)
	_slider("heat shimmer", "heat", 0.0, 1.0, 0.01)
	panel.heading("Camera (this theme)")
	_slider("field of view (°)", "cam_fov", 35.0, 90.0, 1.0)
	_slider("distance (m)", "cam_distance", 2.0, 16.0, 0.1)
	_slider("height (m)", "cam_height", 0.5, 10.0, 0.1)
	_slider("look down (°)", "cam_pitch", -10.0, 70.0, 1.0)
	_slider("follow speed (low = floaty)", "cam_lag", 0.5, 10.0, 0.1)
	_slider("handheld sway", "cam_sway", 0.0, 1.5, 0.05)
	_slider("Dutch roll (°)", "cam_roll", 0.0, 10.0, 0.1)
	_slider("blur beyond (m, 0 = off)", "cam_dof_far", 0.0, 60.0, 0.5)
	_slider("blur nearer than (m, 0 = off)", "cam_dof_near", 0.0, 15.0, 0.1)
	_slider("lens fringing", "cam_aberration", 0.0, 1.0, 0.01)
	panel.checkbox("show camera zones (V)", func() -> bool: return zones_visible, _show_zones)
	panel.button("shake (X)", func() -> void: rig.shake(1.0))
	panel.heading("Post")
	_slider("glow (bloom)", "glow", 0.0, 2.5, 0.05)
	_slider("saturation", "saturation", 0.5, 2.0, 0.01)
	_slider("contrast", "contrast", 0.5, 2.0, 0.01)
	_slider("particles", "particle_amount", 0.0, 1.0, 0.05)
	_slider("vignette", "vignette", 0.0, 1.0, 0.01)
	_slider("ground shine (low = wet)", "ground_roughness", 0.05, 1.0, 0.01)
	panel.checkbox("free camera (C)", func() -> bool: return camera.active, _set_free_camera)


func _slider(text: String, field: String, low: float, high: float, step: float) -> void:
	panel.slider(text, low, high, step, func() -> float: return look.theme.get(field), func(v: float) -> void: look.theme.set(field, v))


# One HSV channel (0 hue, 1 saturation, 2 value) of a colour field of the current theme.
func _color_slider(text: String, field: String, channel: int) -> void:
	var read := func() -> float:
		var c: Color = look.theme.get(field)
		return [c.h, c.s, c.v][channel]
	var write := func(v: float) -> void:
		var c: Color = look.theme.get(field)
		var hsv := [c.h, c.s, c.v]
		hsv[channel] = v
		look.theme.set(field, Color.from_hsv(hsv[0], hsv[1], hsv[2], c.a))
	panel.slider(text, 0.0, 1.0, 0.01, read, write)


func _set_free_camera(on: bool) -> void:
	camera.active = on
	if on:
		camera.global_transform = rig.camera.global_transform
		camera.take_over(8.0)
		camera.current = true
	else:
		rig.camera.current = true


# Three places on the path that ask the camera for something else (see MoodCameraZone).
func _add_zones() -> void:
	rig.zones.append(MoodCameraZone.create(self, "overhead", _path_point(15.0), 4.0, {"pitch": 55.0, "distance": 11.0, "height": 5.0}, Color(0.3, 0.8, 1.0)))
	rig.zones.append(MoodCameraZone.create(self, "vista", _path_point(33.0), 6.0, {"fov": 76.0, "distance": 13.0, "height": 6.0, "pitch": 24.0, "lag": 1.2}, Color(1.0, 0.85, 0.2)))
	rig.zones.append(MoodCameraZone.create(self, "corridor", _path_point(50.0), 5.0, {"fov": 48.0, "distance": 3.8, "height": 1.5, "pitch": 6.0, "sway": 0.6}, Color(1.0, 0.3, 0.4)))


func _show_zones(on: bool) -> void:
	zones_visible = on
	for zone in rig.zones:
		zone.show_ring(on)


# --- the world ----------------------------------------------------------------------------------

# Where the path runs at distance `z` along it (it winds gently, heading −Z).
func _path_point(z: float) -> Vector3:
	return Vector3(sin(z * 0.11) * 4.0 + sin(z * 0.037) * 3.0, 0.0, -z)


func _across(z: float) -> Vector3:
	return (_path_point(z + 0.5) - _path_point(z)).normalized().cross(Vector3.UP)


# The scene's layout, made once (seeded) so every theme dresses identical ground:
#   tiles [at, yaw, index], lamps [at, yaw], fences [a, b], props [at, size, variant],
#   trees [at, height, variant], landmarks [at, height].
func _layout() -> Dictionary:
	var result := {"tiles": [], "lamps": [], "fences": [], "props": [], "trees": [], "landmarks": []}
	var z := 0.0
	var index := 0
	while z < PATH_LENGTH:
		var at := _path_point(z)
		var ahead := _path_point(z + 0.5)
		result.tiles.append([at, atan2(ahead.x - at.x, ahead.z - at.z) + _rng.randf_range(-0.05, 0.05), index])
		z += 0.55
		index += 1
	var side := 1.0
	for i in int(PATH_LENGTH / 7.0):
		var along := 4.0 + i * 7.0
		var toward := -_across(along) * side # the lamp's arm (+X) leans out over the path
		result.lamps.append([_path_point(along) + _across(along) * 1.6 * side, atan2(-toward.z, toward.x)])
		side = -side
	for i in 6:
		var start := 8.0 + i * 11.0
		var s := 1.0 if i % 2 == 0 else -1.0
		result.fences.append([_path_point(start) + _across(start) * 2.6 * s, _path_point(start + 6.0) + _across(start + 6.0) * 2.6 * s])
	for i in 18:
		var along := _rng.randf_range(2.0, PATH_LENGTH)
		var s := 1.0 if _rng.randf() < 0.5 else -1.0
		result.props.append([_path_point(along) + _across(along) * _rng.randf_range(1.6, 3.5) * s, _rng.randf_range(0.18, 0.35), i])
	for i in 200:
		var along := _rng.randf_range(-10.0, PATH_LENGTH + 30.0)
		var out := _rng.randf_range(5.0, 45.0) * (1.0 if _rng.randf() < 0.5 else -1.0)
		result.trees.append([_path_point(along) + _across(along) * out, _rng.randf_range(4.0, 9.0) * (1.0 + absf(out) / 60.0), i])
	for i in 5:
		var along := 20.0 + i * 13.0
		var s := 1.0 if i % 2 == 0 else -1.0
		result.landmarks.append([_path_point(along) + _across(along) * _rng.randf_range(9.0, 16.0) * s, _rng.randf_range(8.0, 14.0)])
	return result


# Swap the prop kit for `theme` (the lighting blends on its own in MoodLook).
func _dress(theme: MoodTheme) -> void:
	if _props != null:
		_props.queue_free()
	look.lamps.clear()
	_props = Node3D.new()
	_props.name = "Props"
	add_child(_props)
	seed(theme.name.hash()) # the same theme always gets the same little variations
	MoodKit.build(theme.kit, _props, layout, look)


func _add_ground() -> void:
	var body := StaticBody3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(220, 1, 220)
	var collision := CollisionShape3D.new()
	collision.shape = shape
	collision.position = Vector3(0, -0.5, -40)
	body.add_child(collision)
	MoodProps.box(body, Vector3(220, 1, 220), Vector3(0, -0.5, -40), Color.WHITE).material_override = look.ground_material
	add_child(body)


# A stand-in hero: a coat and a round head with glowing eyes, rim-lit so it stands out of the fog.
func _add_character() -> Node3D:
	var root := Node3D.new()
	root.position = _path_point(1.0)
	add_child(root)
	var body := Node3D.new()
	root.add_child(body)
	var coat := CapsuleMesh.new()
	coat.radius = 0.32
	coat.height = 1.2
	MoodProps.mesh(body, coat, Vector3(0, 0.75, 0), Color.WHITE).material_override = MoodProps.material(Color(0.75, 0.62, 0.45), 0.0, 0.8)
	var head := SphereMesh.new()
	head.radius = 0.3
	head.height = 0.48
	MoodProps.mesh(body, head, Vector3(0, 1.55, 0), Color.WHITE).material_override = MoodProps.material(MoodProps.PUMPKIN, 0.0, 0.8)
	for x: float in [-0.1, 0.1]:
		MoodProps.box(body, Vector3(0.08, 0.06, 0.04), Vector3(x, 1.6, -0.28), Color.WHITE).material_override = MoodProps.material(MoodProps.FLAME, 4.0)
	return root


func _add_environment() -> Environment:
	var sky := Sky.new()
	sky.sky_material = ProceduralSkyMaterial.new()
	var environment := Environment.new()
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.fog_mode = Environment.FOG_MODE_DEPTH
	environment.fog_sun_scatter = 0.3
	environment.fog_height = 1.2
	environment.fog_depth_curve = 1.4
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.glow_hdr_threshold = 1.0
	environment.glow_bloom = 0.05
	environment.adjustment_enabled = true
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)
	return environment


func _add_sun() -> DirectionalLight3D:
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 40.0
	add_child(sun)
	return sun


func _add_particles() -> GPUParticles3D:
	var particles := GPUParticles3D.new()
	particles.top_level = true
	particles.visibility_aabb = AABB(Vector3(-15, -6, -15), Vector3(30, 14, 30))
	add_child(particles)
	return particles


# A full-screen ColorRect running `path`'s shader (lens fringing, vignette).
func _add_screen_shader(layer: CanvasLayer, path: String) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = load(path)
	var rect := ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	rect.material = material
	layer.add_child(rect)
	material.set_meta("rect", rect)
	return material
