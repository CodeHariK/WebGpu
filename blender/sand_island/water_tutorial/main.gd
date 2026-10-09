## Water shader, step by step. Each file in steps/ adds one idea to the one before it.
## Keys: 1–9 and 0 pick step 1–10 · ← → previous / next · left-drag orbit · wheel zoom · R reset.
##       G island: loft (rings) → grid (heightmap) → Blender · N new island (next seed)
##       V loft bank style: wall → slope → round → overhang · F wireframe · J grid bank smooth ↔ jagged
##       L layers: none → grass sheet → layered ice cream · [ ] bevel − + · , . overhang − +
## The panel shows the comment at the top of the current step's file (what that step adds).
## Run with `-- --capture` to save one screenshot per step into shots/ and quit.
extends Node3D

const STEPS := [
	"step01_flat", "step02_depth", "step03_colour", "step04_alpha", "step05_foam",
	"step06_wash", "step07_wobble", "step08_lines", "step09_sparkles", "step10_swell",
]

var step := 9
var water: MeshInstance3D        # the water currently shown (Blender's or the procedural one)
var blender_island: Node3D
var blender_water: MeshInstance3D
var procedural: ProceduralIsland  # heightmap-grid island
var loft: LoftIsland              # ring (loft) island
var island_seed := 1
var bank_style: BankProfile.Style = BankProfile.Style.WALL
var mode := 0                     # 0 loft, 1 grid, 2 Blender
var layer_preset := 1
var status := Label.new()
var yaw := deg_to_rad(-10.0)
var pitch := deg_to_rad(-28.0)
var distance := 13.0
var focus := Vector3(1.0, 0.0, 8.5)
var camera := Camera3D.new()
var title := Label.new()
var notes := Label.new()


func _ready() -> void:
	RenderingServer.set_debug_generate_wireframes(true)  # must be on before meshes are created
	var sand := ShaderMaterial.new()
	sand.shader = load("res://sand.gdshader")
	blender_island = load("res://sand_island.glb").instantiate()
	add_child(blender_island)
	for mesh in blender_island.find_children("*", "MeshInstance3D", true, false):
		if mesh.name.begins_with("Terrain"):
			mesh.material_override = sand
		elif mesh.name.begins_with("Water"):
			blender_water = mesh
	procedural = ProceduralIsland.new(sand, null)
	add_child(procedural)
	procedural.regenerate(island_seed)
	loft = LoftIsland.new(sand, null)
	loft.layer_material = ShaderMaterial.new()
	loft.layer_material.shader = load("res://layer.gdshader")
	loft.layers = IslandLayer.preset(layer_preset, loft.loft.plateau_height if loft.loft else 1.7)
	add_child(loft)
	loft.regenerate(island_seed, bank_style)
	_set_mode(0)
	_add_world()
	camera.fov = 40.0
	add_child(camera)
	_add_panel()
	_show_step(step)
	_place_camera()
	if "--capture" in OS.get_cmdline_user_args():
		_capture_all()
	elif "--capture-layers" in OS.get_cmdline_user_args():
		_capture_layers()
	elif "--capture-loft" in OS.get_cmdline_user_args():
		_capture_loft()
	elif "--capture-bank" in OS.get_cmdline_user_args():
		_capture_bank()
	elif "--capture-procedural" in OS.get_cmdline_user_args():
		_capture_procedural()


func _add_world() -> void:
	var sun := DirectionalLight3D.new()
	sun.shadow_enabled = true
	sun.rotation_degrees = Vector3(-55, 30, 0)
	add_child(sun)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.62, 0.82, 0.98)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.8, 0.85, 1.0)
	env.ambient_light_energy = 0.55
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


func _add_panel() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(14, 12)
	panel.custom_minimum_size = Vector2(620, 0)
	var box := VBoxContainer.new()
	title.add_theme_font_size_override("font_size", 22)
	notes.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	notes.custom_minimum_size = Vector2(600, 0)
	box.add_child(title)
	box.add_child(notes)
	var keys := Label.new()
	keys.text = "1–9, 0: step · ← →: prev/next · G: loft / grid / Blender · V: bank style · N: new island · F: wireframe · J: grid smooth/jagged · drag/wheel: camera · R: reset"
	keys.modulate = Color(1, 1, 1, 0.6)
	box.add_child(keys)
	status.modulate = Color(1, 0.95, 0.7)
	box.add_child(status)
	panel.add_child(box)
	var layer := CanvasLayer.new()
	layer.add_child(panel)
	add_child(layer)


func _toggle_wireframe() -> void:
	var on := get_viewport().debug_draw != Viewport.DEBUG_DRAW_WIREFRAME
	get_viewport().debug_draw = Viewport.DEBUG_DRAW_WIREFRAME if on else Viewport.DEBUG_DRAW_DISABLED


## Which island is shown: 0 loft, 1 heightmap grid, 2 the Blender export (same shaders on all).
func _set_mode(m: int) -> void:
	mode = m % 3
	loft.visible = mode == 0
	procedural.visible = mode == 1
	blender_island.visible = mode == 2
	water = [loft.water, procedural.water, blender_water][mode]


func _show_step(index: int) -> void:
	step = clampi(index, 0, STEPS.size() - 1)
	var path := "res://steps/%s.gdshader" % STEPS[step]
	var material := ShaderMaterial.new()
	material.shader = load(path)
	for w in [loft.water, procedural.water, blender_water]:
		w.material_override = material
	var header := _header_comment(FileAccess.get_file_as_string(path))
	title.text = header[0]
	notes.text = header[1]


## The leading // comment of a shader file: [first line, the rest joined].
func _header_comment(code: String) -> Array:
	var lines: PackedStringArray = []
	for line in code.split("\n"):
		if not line.begins_with("//"):
			break
		lines.append(line.trim_prefix("//").strip_edges())
	if lines.is_empty():
		return ["", ""]
	return [lines[0], " ".join(lines.slice(1))]


func _update_status() -> void:
	var text := "%s · bank: %s" % [IslandLayer.PRESET_NAMES[layer_preset], BankProfile.NAMES[bank_style]]
	if not loft.layers.is_empty():
		text += " · bevel %.2f m · overhang %.2f m" % [loft.layers[0].bevel, loft.layers[0].overhang]
	status.text = text


func _place_camera() -> void:
	camera.position = focus + Vector3(0, 0, distance).rotated(Vector3.RIGHT, pitch).rotated(Vector3.UP, yaw)
	camera.look_at(focus)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed:
		if event.keycode >= KEY_1 and event.keycode <= KEY_9:
			_show_step(event.keycode - KEY_1)
		elif event.keycode == KEY_0:
			_show_step(9)
		elif event.keycode == KEY_RIGHT:
			_show_step(step + 1)
		elif event.keycode == KEY_LEFT:
			_show_step(step - 1)
		elif event.keycode == KEY_G:
			_set_mode(mode + 1)
		elif event.keycode == KEY_L:
			layer_preset = (layer_preset + 1) % IslandLayer.PRESET_NAMES.size()
			loft.layers = IslandLayer.preset(layer_preset, loft.loft.plateau_height)
			loft.rebuild_layers()
		elif event.keycode == KEY_BRACKETLEFT or event.keycode == KEY_BRACKETRIGHT:
			for layer in loft.layers:
				layer.bevel = clampf(layer.bevel + (0.03 if event.keycode == KEY_BRACKETRIGHT else -0.03), 0.0, 0.5)
				layer.bottom_bevel = minf(layer.bevel, layer.thickness * 0.4)
			loft.rebuild_layers()
		elif event.keycode == KEY_COMMA or event.keycode == KEY_PERIOD:
			for layer in loft.layers:
				layer.overhang = clampf(layer.overhang + (0.05 if event.keycode == KEY_PERIOD else -0.05), 0.0, 1.0)
			loft.rebuild_layers()
		elif event.keycode == KEY_V:
			bank_style = (bank_style + 1) % BankProfile.NAMES.size() as BankProfile.Style
			loft.regenerate(island_seed, bank_style)
			_set_mode(0)
		elif event.keycode == KEY_F:
			_toggle_wireframe()
		elif event.keycode == KEY_J:
			IslandMeshBuilder.smooth_edges = not IslandMeshBuilder.smooth_edges
			procedural.regenerate(island_seed)
			_set_mode(1)
		elif event.keycode == KEY_N:
			island_seed += 1
			procedural.regenerate(island_seed)
			loft.regenerate(island_seed, bank_style)
		elif event.keycode == KEY_R:
			yaw = deg_to_rad(-10.0); pitch = deg_to_rad(-28.0); distance = 13.0
		_update_status()
	elif event is InputEventMouseMotion and event.button_mask & MOUSE_BUTTON_MASK_LEFT:
		yaw -= event.relative.x * 0.005
		pitch = clampf(pitch - event.relative.y * 0.005, deg_to_rad(-85), deg_to_rad(-5))
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(distance / 1.12, 3.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(distance * 1.12, 60.0)
	_place_camera()


func _capture_all() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	for i in STEPS.size():
		_show_step(i)
		for f in 12:
			await get_tree().process_frame
		get_viewport().get_texture().get_image().save_png("res://shots/%s.png" % STEPS[i])
	get_tree().quit()


## Screenshots of three generated islands, each as the depth view (step 2) and the finished water.
func _capture_procedural() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	distance = 30.0
	pitch = deg_to_rad(-40.0)
	focus = Vector3.ZERO
	_place_camera()
	for s in [1, 2, 3]:
		island_seed = s
		procedural.regenerate(s)
		for i in [1, 9]:
			_show_step(i)
			for f in 12:
				await get_tree().process_frame
			get_viewport().get_texture().get_image().save_png("res://shots/procedural_seed%d_%s.png" % [s, STEPS[i]])
	get_tree().quit()


## Close-ups of the procedural bank: jagged (first version) vs smooth, shaded and as wireframe.
func _capture_bank() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	focus = Vector3(-2.5, 1.0, 3.0)
	distance = 6.5
	pitch = deg_to_rad(-18.0)
	yaw = deg_to_rad(-35.0)
	_place_camera()
	_set_mode(1)
	for smooth in [false, true]:
		IslandMeshBuilder.smooth_edges = smooth
		procedural.regenerate(island_seed)
		for wire in [false, true]:
			get_viewport().debug_draw = Viewport.DEBUG_DRAW_WIREFRAME if wire else Viewport.DEBUG_DRAW_DISABLED
			procedural.water.visible = not wire  # the water's own grid would hide the terrain's
			for f in 12:
				await get_tree().process_frame
			var name := "bank_%s_%s.png" % ["smooth" if smooth else "jagged", "wire" if wire else "shaded"]
			get_viewport().get_texture().get_image().save_png("res://shots/" + name)
	get_tree().quit()


## The loft island in every bank style, shaded and as wireframe (water hidden in the wireframe).
func _capture_loft() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	_set_mode(0)
	focus = Vector3(-2.5, 1.0, 3.0)
	distance = 9.0
	pitch = deg_to_rad(-20.0)
	yaw = deg_to_rad(-35.0)
	_place_camera()
	for style in BankProfile.NAMES.size():
		loft.regenerate(island_seed, style)
		for wire in [false, true]:
			get_viewport().debug_draw = Viewport.DEBUG_DRAW_WIREFRAME if wire else Viewport.DEBUG_DRAW_DISABLED
			loft.water.visible = not wire
			for f in 12:
				await get_tree().process_frame
			get_viewport().get_texture().get_image().save_png("res://shots/loft_%d_%s.png" % [style, "wire" if wire else "shaded"])
	get_tree().quit()


## Layer variations: grass sheet at bevel 0 / 0.15 / 0.35, ice cream, and a sheet on the round bank.
func _capture_layers() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	_set_mode(0)
	focus = Vector3(-2.5, 1.2, 3.0)
	distance = 8.0
	pitch = deg_to_rad(-16.0)
	yaw = deg_to_rad(-35.0)
	_place_camera()
	var shots := [
		["sheet_sharp", BankProfile.Style.WALL, 1, 0.0],
		["sheet_bevel", BankProfile.Style.WALL, 1, 0.15],
		["sheet_round", BankProfile.Style.WALL, 1, 0.35],
		["icecream", BankProfile.Style.WALL, 2, -1.0],
		["sheet_on_round_bank", BankProfile.Style.ROUND, 1, 0.15],
		["icecream_on_slope", BankProfile.Style.SLOPE, 2, -1.0],
	]
	for shot in shots:
		layer_preset = shot[2]
		loft.layers = IslandLayer.preset(layer_preset, 1.7)
		if shot[3] >= 0.0:
			for layer in loft.layers:
				layer.bevel = shot[3]
				layer.bottom_bevel = minf(layer.bevel, layer.thickness * 0.4)
		bank_style = shot[1]
		loft.regenerate(island_seed, bank_style)
		_update_status()
		for f in 12:
			await get_tree().process_frame
		get_viewport().get_texture().get_image().save_png("res://shots/layers_%s.png" % shot[0])
	get_tree().quit()
