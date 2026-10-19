## Pumpkin Jack–style mood demo (mood/): a lantern-lit path through a foggy night forest, built from
## primitives, with every part of the look on the panel (Tab) so you can see what each one adds.
##   WASD  walk the stand-in character (the camera follows behind and above)
##   N     day ↔ night (eases over two seconds)
##   1-7   switch a layer off/on: 1 fog  2 colour grade  3 glow  4 lanterns  5 embers  6 vignette
##         7 purple ambient
##   C     free fly camera (fly_camera.gd)      Tab  panel
## The look itself is MoodLook (mood_look.gd); the props are MoodProps (mood_props.gd).
extends Node3D

const GROUND := Color(0.24, 0.17, 0.2)
const PATH_LENGTH := 70.0
const WALK_SPEED := 4.0
const CAMERA_BACK := Vector3(0, 4.5, 6.5) ## behind and above the character (its yaw)
const LAYERS := ["use_fog", "use_grade", "use_glow", "use_lanterns", "use_embers", "use_vignette", "use_ambient"]
const LAYER_NAMES := ["fog", "colour grade", "glow", "lanterns", "embers", "vignette", "purple ambient"]

var look: MoodLook
var camera: FlyCamera
var character: Node3D
var panel: OrbPanel
var hud: Label
var _rng := RandomNumberGenerator.new()


func _ready() -> void:
	_rng.seed = 5
	seed(5)
	look = MoodLook.new()
	look.name = "Look"
	look.environment = _add_environment()
	look.sun = _add_moon()
	add_child(look)
	_add_ground()
	_add_path()
	_add_forest()
	_add_background()
	character = _add_character()
	camera = FlyCamera.new()
	camera.fov = 55.0
	camera.active = false
	add_child(camera)
	camera.global_position = character.global_position + CAMERA_BACK
	look.embers = _add_embers()
	var layer := CanvasLayer.new()
	add_child(layer)
	look.vignette_material = _add_vignette(layer)
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
	if not camera.active:
		var behind := character.global_transform.basis * CAMERA_BACK
		camera.global_position = camera.global_position.lerp(character.global_position + behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(character.global_position + Vector3.UP * 1.0 - character.global_basis.z * 3.0)
	look.embers.global_position = camera.global_position - camera.global_basis.z * 6.0
	var off: Array[String] = []
	for i in LAYERS.size():
		if not look.get(LAYERS[i]):
			off.append(LAYER_NAMES[i])
	hud.text = "WASD walk   N day/night (%s)   1-7 layers   C free camera   Tab panel" % ("night" if look.night > 0.5 else "day")
	if not off.is_empty():
		hud.text += "\nswitched off: " + ", ".join(off)


func _walk(delta: float) -> void:
	if camera.flying():
		return
	var turn := float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)) - float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT))
	var forward := float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN))
	character.rotate_y(turn * 2.2 * delta)
	character.global_position += -character.global_basis.z * forward * WALK_SPEED * delta
	var bob := absf(sin(Time.get_ticks_msec() * 0.012)) * 0.06 * absf(forward)
	character.get_child(0).position.y = bob


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	if key.keycode >= KEY_1 and key.keycode <= KEY_7:
		var layer: String = LAYERS[key.keycode - KEY_1]
		look.set(layer, not look.get(layer))
		return
	match key.keycode:
		KEY_N: look.blend_to(0.0 if look.night > 0.5 else 1.0)
		KEY_C: _set_free_camera(not camera.active)
		KEY_TAB: panel.visible = not panel.visible


func _fill_panel() -> void:
	panel.button("day ↔ night (N)", func() -> void: look.blend_to(0.0 if look.night > 0.5 else 1.0))
	panel.slider("day → night", 0.0, 1.0, 0.01, func() -> float: return look.night, func(v: float) -> void: look.night = v)
	panel.heading("Layers (1-7)")
	for i in LAYERS.size():
		var layer: String = LAYERS[i]
		panel.checkbox("%d  %s" % [i + 1, LAYER_NAMES[i]], func() -> bool: return look.get(layer), func(on: bool) -> void: look.set(layer, on))
	panel.heading("Fog")
	_slider("fog hue (magenta / orange / violet)", "fog_hue", 0.0, 1.0, 0.01)
	_slider("fog saturation", "fog_saturation", 0.0, 1.0, 0.01)
	_slider("fog brightness", "fog_brightness", 0.1, 1.0, 0.01)
	_slider("fog begins (m)", "fog_begin", 0.0, 30.0, 0.5)
	_slider("fog solid at (m)", "fog_end", 10.0, 120.0, 1.0)
	_slider("ground fog", "fog_height_density", 0.0, 1.5, 0.05)
	panel.heading("Light")
	_slider("ambient (purple) energy", "ambient_energy", 0.0, 2.0, 0.05)
	_slider("moon energy", "moon_energy", 0.0, 2.0, 0.05)
	_slider("moon height (°)", "moon_pitch", -80.0, -5.0, 1.0)
	_slider("lantern energy", "lantern_energy", 0.0, 6.0, 0.1)
	_slider("lantern range (m)", "lantern_range", 1.0, 10.0, 0.1)
	panel.heading("Post")
	_slider("glow (bloom)", "glow", 0.0, 2.5, 0.05)
	_slider("saturation", "saturation", 0.5, 2.0, 0.01)
	_slider("contrast", "contrast", 0.5, 2.0, 0.01)
	_slider("embers", "ember_amount", 0.0, 1.0, 0.05)
	_slider("vignette", "vignette", 0.0, 1.0, 0.01)
	panel.checkbox("free camera (C)", func() -> bool: return camera.active, _set_free_camera)


func _set_free_camera(on: bool) -> void:
	camera.active = on
	if on:
		camera.take_over(8.0)


func _slider(text: String, property: String, low: float, high: float, step: float) -> void:
	panel.slider(text, low, high, step, func() -> float: return look.get(property), func(v: float) -> void: look.set(property, v))


# --- the world ----------------------------------------------------------------------------------

# Where the path runs at distance `z` along it (it winds gently, heading −Z).
func _path_point(z: float) -> Vector3:
	return Vector3(sin(z * 0.11) * 4.0 + sin(z * 0.037) * 3.0, 0.0, -z)


func _add_ground() -> void:
	var body := StaticBody3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(200, 1, 200)
	var collision := CollisionShape3D.new()
	collision.shape = shape
	collision.position.y = -0.5
	body.add_child(collision)
	MoodProps.box(body, Vector3(200, 1, 200), Vector3(0, -0.5, -40), GROUND)
	add_child(body)


# Planks along the path (lit by the lanterns: the warm line the eye follows), lanterns on
# alternating sides, fences and pumpkins along the edges.
func _add_path() -> void:
	var z := 0.0
	while z < PATH_LENGTH:
		var at := _path_point(z)
		var ahead := _path_point(z + 0.5)
		var yaw := atan2(ahead.x - at.x, ahead.z - at.z)
		if _rng.randf() > 0.08: # a few missing planks
			MoodProps.box(self, Vector3(2.2, 0.08, 0.42), at + Vector3(0, 0.04, 0), MoodProps.PLANK, Vector3(0, yaw + _rng.randf_range(-0.06, 0.06), 0))
		z += 0.55
	var side := 1.0
	for i in int(PATH_LENGTH / 7.0):
		var at := _path_point(4.0 + i * 7.0)
		var across := _across(4.0 + i * 7.0)
		var toward := -across * side # the arm (+X) leans out over the path
		var parts: Array = MoodProps.lantern(self, at + across * 1.6 * side, atan2(-toward.z, toward.x))
		look.lanterns.append(parts[1])
		look.flames.append(parts[2])
		side = -side
	for i in 6: # fence runs along stretches of the path edge
		var start := 8.0 + i * 11.0
		var s := 1.0 if i % 2 == 0 else -1.0
		MoodProps.fence(self, _path_point(start) + _across(start) * 2.6 * s, _path_point(start + 6.0) + _across(start + 6.0) * 2.6 * s)
	for i in 18:
		var along := _rng.randf_range(2.0, PATH_LENGTH)
		var s := 1.0 if _rng.randf() < 0.5 else -1.0
		MoodProps.pumpkin(self, _path_point(along) + _across(along) * _rng.randf_range(1.6, 3.5) * s, _rng.randf_range(0.18, 0.35), _rng.randf() < 0.3)


func _across(z: float) -> Vector3:
	var a := _path_point(z)
	var b := _path_point(z + 0.5)
	return (b - a).normalized().cross(Vector3.UP)


# Pines and dead trees either side of the path, thicker further out: they fade into the fog in
# layers, near ones dark and crisp, far ones flat silhouettes.
func _add_forest() -> void:
	for i in 220:
		var along := _rng.randf_range(-10.0, PATH_LENGTH + 30.0)
		var out := _rng.randf_range(5.0, 45.0) * (1.0 if _rng.randf() < 0.5 else -1.0)
		var at := _path_point(along) + _across(along) * out
		var height := _rng.randf_range(4.0, 9.0) * (1.0 + absf(out) / 60.0)
		if _rng.randf() < 0.7:
			MoodProps.pine(self, at, height)
		else:
			MoodProps.dead_tree(self, at, height * 0.7)


# Tall stone towers in the distance with lanterns on top, and soft light shafts.
func _add_background() -> void:
	for i in 5:
		var along := 20.0 + i * 13.0
		var s := 1.0 if i % 2 == 0 else -1.0
		var at := _path_point(along) + _across(along) * _rng.randf_range(9.0, 16.0) * s
		var tower := MoodProps.tower(self, at, _rng.randf_range(8.0, 14.0))
		var parts: Array = MoodProps.lantern(self, at + Vector3.UP * (tower.get_child(-1) as Node3D).position.y, 0.0)
		look.lanterns.append(parts[1])
		look.flames.append(parts[2])
		_add_shaft(at + Vector3(0, 6, 2.5 * s))


# A light shaft: a tall camera-facing plane with a soft vertical gradient, added on top.
func _add_shaft(at: Vector3) -> void:
	var gradient := Gradient.new()
	gradient.colors = PackedColorArray([Color(1.0, 0.6, 0.5, 0.0), Color(1.0, 0.6, 0.5, 0.25), Color(1.0, 0.6, 0.5, 0.0)])
	gradient.offsets = PackedFloat32Array([0.0, 0.5, 1.0])
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.fill_to = Vector2(1, 0) # across the shaft
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.albedo_texture = texture
	material.billboard_mode = BaseMaterial3D.BILLBOARD_FIXED_Y
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var quad := QuadMesh.new()
	quad.size = Vector2(2.5, 14.0)
	var shaft := MeshInstance3D.new()
	shaft.mesh = quad
	shaft.material_override = material
	shaft.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	shaft.position = at
	add_child(shaft)


# A stand-in hero: a coat, a pumpkin head with glowing eyes — rim-lit so it stands out of the fog.
func _add_character() -> Node3D:
	var root := Node3D.new()
	root.position = _path_point(1.0)
	root.rotation.y = 0.0
	add_child(root)
	var body := Node3D.new()
	root.add_child(body)
	var coat := CapsuleMesh.new()
	coat.radius = 0.32
	coat.height = 1.2
	MoodProps.mesh(body, coat, Vector3(0, 0.75, 0), Color(0.75, 0.62, 0.45)).material_override = MoodProps.material(Color(0.75, 0.62, 0.45), 0.0, 0.8)
	var head := SphereMesh.new()
	head.radius = 0.3
	head.height = 0.48
	MoodProps.mesh(body, head, Vector3(0, 1.55, 0), MoodProps.PUMPKIN).material_override = MoodProps.material(MoodProps.PUMPKIN, 0.0, 0.8)
	for x: float in [-0.1, 0.1]:
		MoodProps.box(body, Vector3(0.08, 0.06, 0.04), Vector3(x, 1.6, -0.28), MoodProps.FLAME).material_override = MoodProps.material(MoodProps.FLAME, 4.0)
	return root


func _add_environment() -> Environment:
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.fog_enabled = true
	environment.fog_mode = Environment.FOG_MODE_DEPTH
	environment.fog_sun_scatter = 0.3
	environment.fog_height = 1.2
	environment.fog_depth_curve = 1.4
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.glow_enabled = true
	environment.glow_hdr_threshold = 1.0
	environment.glow_bloom = 0.05
	environment.adjustment_enabled = true
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)
	return environment


func _add_moon() -> DirectionalLight3D:
	var moon := DirectionalLight3D.new()
	moon.rotation_degrees = Vector3(-22, 150, 0) # low, from behind-left: long shadows toward the camera
	moon.shadow_enabled = true
	moon.directional_shadow_max_distance = 40.0
	add_child(moon)
	return moon


# Embers: small additive orange sparks drifting up and wobbling around the camera.
func _add_embers() -> GPUParticles3D:
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	process.emission_box_extents = Vector3(12, 4, 12)
	process.direction = Vector3.UP
	process.spread = 30.0
	process.initial_velocity_min = 0.2
	process.initial_velocity_max = 0.6
	process.gravity = Vector3(0, 0.15, 0)
	process.turbulence_enabled = true
	process.turbulence_noise_strength = 0.6
	process.scale_min = 0.5
	process.scale_max = 1.3
	var fade := Gradient.new()
	fade.colors = PackedColorArray([Color(1, 0.6, 0.2, 0), Color(1, 0.6, 0.2, 1), Color(1, 0.3, 0.1, 0)])
	fade.offsets = PackedFloat32Array([0.0, 0.2, 1.0])
	var fade_texture := GradientTexture1D.new()
	fade_texture.gradient = fade
	process.color_ramp = fade_texture
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	material.vertex_color_use_as_albedo = true
	material.albedo_color = Color(3.0, 2.0, 1.2) # over-bright: caught by the glow
	var quad := QuadMesh.new()
	quad.size = Vector2(0.05, 0.05)
	quad.material = material
	var embers := GPUParticles3D.new()
	embers.amount = 300
	embers.lifetime = 5.0
	embers.process_material = process
	embers.draw_pass_1 = quad
	embers.top_level = true
	embers.visibility_aabb = AABB(Vector3(-15, -6, -15), Vector3(30, 14, 30))
	add_child(embers)
	return embers


func _add_vignette(layer: CanvasLayer) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = load("res://mood/vignette.gdshader")
	var rect := ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	rect.material = material
	layer.add_child(rect)
	return material
