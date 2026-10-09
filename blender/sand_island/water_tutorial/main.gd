## Shader tutorials, step by step: the water (steps/) and the sand (sand_steps/). Each file adds one
## idea to the one before it. Tab switches which shader you are stepping through; the other one
## stays finished.
## Keys: 1–9, 0 and − pick step 1–11 · ← → previous / next · left-drag orbit · Shift-drag or right-drag
##       pan · wheel zoom · R reset.
##       G island: loft (rings) → grid (heightmap) → Blender · N new island (next seed)
##       V loft bank style: wall → slope → round → overhang · F wireframe · J grid bank smooth ↔ jagged
##       L layers: none → grass sheet → layered ice cream · [ ] bevel − + · , . overhang − +
## The panel shows the comment at the top of the current step's file (what that step adds).
## Run with `-- --capture` to save one screenshot per step into shots/ and quit.
extends Node3D

const SAND_STEPS := [
	"step01_lit", "step02_height", "step03_zones", "step04_wet", "step05_waves",
	"step06_noise", "step07_grain", "step08_ripples", "step09_ripple_light", "step10_toon",
	"step11_noise_texture",
]
const STEPS := [
	"step01_flat", "step02_depth", "step03_colour", "step04_alpha", "step05_foam",
	"step06_wash", "step07_wobble", "step08_lines", "step09_sparkles", "step10_swell",
	"step11_noise_texture",
]
## Random pixels for the step-11 shaders, loaded as-is (no import compression, which would change them).
var noise_texture: Texture2D = load("res://textures/noise_128.png")  # imported Lossless, no mipmaps

var step := 9
var sand_step := 9
var track := 0                    # 0 stepping the water, 1 stepping the sand
## Which tutorial the scene opens on: main.tscn starts on the water, sand.tscn on the sand.
@export_enum("Water", "Sand") var start_track := 0
var track_buttons: Array[Button] = []
var ui_layer: CanvasLayer
var hud := PerfHud.new()
var blender_terrain: MeshInstance3D
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
			blender_terrain = mesh
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
	add_child(hud)
	_set_track(start_track)
	_place_camera()
	if "--hud-shot" in OS.get_cmdline_user_args():
		_hud_shot()
	elif "--bench" in OS.get_cmdline_user_args():
		_bench(true)
	elif "--capture-step11" in OS.get_cmdline_user_args():
		_capture_step11()
	elif "--test-keys" in OS.get_cmdline_user_args():
		_test_keys()
	elif "--capture" in OS.get_cmdline_user_args():
		_capture_all()
	elif "--capture-sand" in OS.get_cmdline_user_args():
		_capture_sand()
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
	var tabs := HBoxContainer.new()
	for i in 2:
		var b := Button.new()
		b.text = ["Water shader steps", "Sand shader steps"][i]
		b.toggle_mode = true
		b.focus_mode = Control.FOCUS_NONE  # keep keys (Tab, arrows) for the tutorial, not UI focus
		b.pressed.connect(_set_track.bind(i))
		tabs.add_child(b)
		track_buttons.append(b)
	box.add_child(tabs)
	box.add_child(title)
	box.add_child(notes)
	var keys := Label.new()
	keys.text = "Tab / S: water ↔ sand · 1–9, 0, −: step 1–11 · ← →: prev/next · G: loft / grid / Blender · V: bank style · N: new island · F: wireframe · J: grid smooth/jagged · drag: orbit · Shift-drag / right-drag: pan · wheel: zoom · R: reset"
	keys.modulate = Color(1, 1, 1, 0.6)
	box.add_child(keys)
	status.modulate = Color(1, 0.95, 0.7)
	box.add_child(status)
	panel.add_child(box)
	var layer := CanvasLayer.new()
	ui_layer = layer
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


## Switch which shader the step keys control: 0 water, 1 sand.
func _set_track(t: int) -> void:
	track = t
	if track == 1 and layer_preset != 0:  # the sand tutorial needs the grass top visible
		layer_preset = 0
		loft.layers = IslandLayer.preset(0, loft.loft.plateau_height)
		loft.rebuild_layers()
	for i in track_buttons.size():
		track_buttons[i].button_pressed = i == track
	_show_step(sand_step if track == 1 else step)


func _show_step(index: int) -> void:
	var names: Array = SAND_STEPS if track == 1 else STEPS
	index = clampi(index, 0, names.size() - 1)
	if track == 1:
		sand_step = index
	else:
		step = index
	_apply_step(0, step)
	_apply_step(1, sand_step)
	var path := _step_path(track, index)
	var header := _header_comment(FileAccess.get_file_as_string(path))
	title.text = ("WATER · " if track == 0 else "SAND · ") + header[0]
	notes.text = header[1]
	_update_status()


func _step_path(which: int, index: int) -> String:
	return "res://sand_steps/%s.gdshader" % SAND_STEPS[index] if which == 1 else "res://steps/%s.gdshader" % STEPS[index]


## Put step `index` of shader `which` (0 water, 1 sand) on every island's water or terrain.
func _apply_step(which: int, index: int) -> void:
	var material := ShaderMaterial.new()
	material.shader = load(_step_path(which, index))
	material.set_shader_parameter("noise_tex", noise_texture)  # ignored by shaders that don't use it
	var targets := [loft.water, procedural.water, blender_water] if which == 0 else [loft.terrain, procedural.terrain, blender_terrain]
	for target in targets:
		target.material_override = material


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


## Slide the point the camera looks at, in the camera's own left/right and up/down, so the scene
## follows the mouse. Scaled by distance: the same drag moves further when zoomed out.
func _pan(mouse_delta: Vector2) -> void:
	var basis := camera.global_transform.basis
	focus += (-basis.x * mouse_delta.x + basis.y * mouse_delta.y) * distance * 0.0015


func _place_camera() -> void:
	camera.position = focus + Vector3(0, 0, distance).rotated(Vector3.RIGHT, pitch).rotated(Vector3.UP, yaw)
	camera.look_at(focus)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed:
		if event.keycode == KEY_TAB or event.keycode == KEY_S:
			_set_track(1 - track)
		elif event.keycode >= KEY_1 and event.keycode <= KEY_9:
			_show_step(event.keycode - KEY_1)
		elif event.keycode == KEY_P:
			hud.measure_now()
		elif event.keycode == KEY_B:
			_bench(false)
		elif event.keycode == KEY_H:
			hud.visible = not hud.visible
		elif event.keycode == KEY_MINUS:
			_show_step(10)
		elif event.keycode == KEY_0:
			_show_step(9)
		elif event.keycode == KEY_RIGHT:
			_show_step((sand_step if track == 1 else step) + 1)
		elif event.keycode == KEY_LEFT:
			_show_step((sand_step if track == 1 else step) - 1)
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
			yaw = deg_to_rad(-10.0); pitch = deg_to_rad(-28.0); distance = 13.0; focus = Vector3(1.0, 0.0, 8.5)
		_update_status()
	elif event is InputEventMouseMotion and event.button_mask & MOUSE_BUTTON_MASK_LEFT and event.shift_pressed:
		_pan(event.relative)
	elif event is InputEventMouseMotion and event.button_mask & (MOUSE_BUTTON_MASK_RIGHT | MOUSE_BUTTON_MASK_MIDDLE):
		_pan(event.relative)
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


## One screenshot per sand step (loft island, no layers, close on the beach and the bank).
func _capture_sand() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	_set_mode(0)
	layer_preset = 0
	loft.layers = IslandLayer.preset(0, 1.7)
	bank_style = BankProfile.Style.SLOPE
	loft.regenerate(island_seed, bank_style)
	track = 1
	focus = Vector3(-1.5, 0.6, 5.0)
	distance = 9.0
	pitch = deg_to_rad(-24.0)
	yaw = deg_to_rad(-30.0)
	_place_camera()
	for i in SAND_STEPS.size():
		_show_step(i)
		for f in 12:
			await get_tree().process_frame
		get_viewport().get_texture().get_image().save_png("res://shots/sand_%s.png" % SAND_STEPS[i])
	get_tree().quit()


## Self-test: press Tab, S and a number key through the input system and check the track/step.
func _test_keys() -> void:
	for f in 5:
		await get_tree().process_frame
	var results := []
	for key in [KEY_TAB, KEY_3, KEY_S, KEY_TAB, KEY_5]:
		var e := InputEventKey.new()
		e.keycode = key
		e.physical_keycode = key
		e.pressed = true
		Input.parse_input_event(e)
		await get_tree().process_frame
		await get_tree().process_frame
		results.append("%s → track %s, water step %d, sand step %d, title: %s"
			% [OS.get_keycode_string(key), ["water", "sand"][track], step + 1, sand_step + 1, title.text.left(40)])
	print("\n".join(results))
	get_tree().quit()


## Cost of EVERY step of both shaders, written into perf.md (between the bench markers) and printed.
## The camera looks straight down so ONE material fills the whole screen, 3D is rendered at 3×
## resolution (9× the pixels) so the GPU is the bottleneck, and each step is timed with
## PerfHud.measure_gpu (frames drawn back to back — Godot's GPU timer reads 0 on Metal).
## Water is measured over open shallow sea with the ground under it on the cheapest sand step;
## sand is measured on dry beach with the water hidden. "cost" = minus the step-1 baseline.
func _bench(quit_after: bool) -> void:
	ui_layer.visible = false
	hud.visible = false
	var vp := get_viewport()
	var old_scale := vp.scaling_3d_scale
	vp.scaling_3d_scale = 3.0
	var old_layers := loft.layers
	_set_mode(0)
	loft.layers = []
	loft.regenerate(island_seed, BankProfile.Style.WALL)
	var old_view := [focus, distance, pitch, yaw]
	var size := vp.get_visible_rect().size
	var rows := []
	for which in [0, 1]:
		var names: Array = SAND_STEPS if which == 1 else STEPS
		loft.water.visible = which == 0
		_apply_step(1, 0 if which == 0 else sand_step)
		focus = Vector3(0.5, 0.4, 7.0) if which == 1 else Vector3(0.0, 0.0, 12.5)
		distance = 3.2
		pitch = deg_to_rad(-89.0)
		yaw = 0.0
		_place_camera()
		var baseline := 0.0
		var previous := 0.0
		for index in names.size():
			_apply_step(which, index)
			for f in 30:  # let the shader compile and the picture settle
				await get_tree().process_frame
			var ms := hud.measure_gpu(240)
			if index == 0:
				baseline = ms
				previous = ms
			rows.append("| %s | %d | %s | %.2f | %+.2f | %.2f |" % [
				["water", "sand"][which], index + 1, names[index], ms, ms - previous, ms - baseline])
			previous = ms
	var header := "Measured %s · %s · %s renderer · 3D at %dx%d · 240 frames per step\n\n" % [
		Time.get_datetime_string_from_system(false, true), RenderingServer.get_video_adapter_name(),
		ProjectSettings.get_setting("rendering/renderer/rendering_method"), size.x * 3, size.y * 3]
	var table := header + "| shader | step | file | ms / frame | vs previous step | shader cost (− step 1) |\n|---|---|---|---|---|---|\n" + "\n".join(rows)
	_write_bench_table(table)
	print(table)
	loft.water.visible = true
	loft.layers = old_layers
	loft.regenerate(island_seed, bank_style)
	vp.scaling_3d_scale = old_scale
	focus = old_view[0]; distance = old_view[1]; pitch = old_view[2]; yaw = old_view[3]
	_place_camera()
	_show_step(sand_step if track == 1 else step)
	ui_layer.visible = true
	hud.visible = true
	hud.last_measure = "bench done → perf.md"
	if quit_after:
		get_tree().quit()


## Replace the text between the bench markers in perf.md (the explanations around it stay).
func _write_bench_table(table: String) -> void:
	var path := ProjectSettings.globalize_path("res://perf.md")
	var doc := FileAccess.get_file_as_string(path)
	var start_tag := "<!-- bench:start -->"
	var end_tag := "<!-- bench:end -->"
	var a := doc.find(start_tag)
	var b := doc.find(end_tag)
	if a == -1 or b == -1:
		doc += "\n%s\n%s\n" % [start_tag, end_tag]
		a = doc.find(start_tag)
		b = doc.find(end_tag)
	doc = doc.substr(0, a + start_tag.length()) + "\n" + table + "\n" + doc.substr(b)
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(doc)


## Step 10 vs step 11 (hash noise vs texture noise) for both shaders, same view: should look alike.
func _capture_step11() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	_set_mode(0)
	loft.layers = []
	loft.regenerate(island_seed, BankProfile.Style.SLOPE)
	ui_layer.visible = false
	focus = Vector3(1.0, 0.0, 9.0)
	distance = 7.0
	pitch = deg_to_rad(-38.0)
	yaw = deg_to_rad(-10.0)
	_place_camera()
	for index in [9, 10]:
		_apply_step(0, index)
		_apply_step(1, index)
		for f in 12:
			await get_tree().process_frame
		get_viewport().get_texture().get_image().save_png("res://shots/compare_step%d.png" % (index + 1))
	get_tree().quit()


## Screenshot of the live HUD after an on-demand GPU measurement (for the docs / a quick check).
func _hud_shot() -> void:
	for f in 60:
		await get_tree().process_frame
	hud.measure_now()
	for f in 30:
		await get_tree().process_frame
	get_viewport().get_texture().get_image().save_png("res://shots/hud.png")
	get_tree().quit()
