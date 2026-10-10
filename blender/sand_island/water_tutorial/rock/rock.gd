## Rock.blend's rock in Godot: the Blender look (rock.gdshader) and the 9 stylised steps (rock/steps/)
## on the same mesh and baked texture (rock_rg.png: R edges, G pattern).
## Keys: 0 Blender look · 1–9 step · ← → previous / next · H hide text · left-drag orbit · wheel zoom
##       P measure GPU now · B bench every view → perf.md (same method as the water / sand bench)
## Run with `-- --capture /path/file.png` (current view), `-- --capture-steps /path/dir` (every
## view, 0–9) or `-- --bench` (bench every view into perf.md) and quit.
extends Node3D

const STEPS := [
	"step01_plain", "step02_facet_tint", "step03_sky_colours", "step04_painted_light",
	"step05_washes", "step06_strokes", "step07_edges", "step08_moss", "step09_baked_paint",
]

## The Blender look; its texture and two colours are set in rock.tscn.
@export var material: Material
@export var focus := Vector3(0.0, 1.0, 0.0)

var rock_rg: Texture2D = load("res://rock/rock_rg.png")
var noise_texture: Texture2D = load("res://textures/noise_128.png")
var view := 0            # 0 = Blender look, 1..9 = step
var yaw := deg_to_rad(-25.0)
var pitch := deg_to_rad(-20.0)
var distance := 7.0
var hud := PerfHud.new()
var title := Label.new()
var notes := Label.new()


func _ready() -> void:
	_add_panel()
	add_child(hud)
	_show(0)
	_place_camera()
	var args := OS.get_cmdline_user_args()
	var i := args.find("--capture")
	if i >= 0 and i + 1 < args.size():
		await _save(args[i + 1])
		get_tree().quit()
	if "--bench" in args:
		await _bench()
		get_tree().quit()
	i = args.find("--capture-steps")
	if i >= 0 and i + 1 < args.size():
		for v in STEPS.size() + 1:
			_show(v)
			await _save("%s/rock_view_%d.png" % [args[i + 1], v])
		get_tree().quit()


func _show(v: int) -> void:
	view = clampi(v, 0, STEPS.size())
	var mat := material
	var path := "res://rock/rock.gdshader"
	if view > 0:
		path = "res://rock/steps/%s.gdshader" % STEPS[view - 1]
		var step := ShaderMaterial.new()
		step.shader = load(path)
		step.set_shader_parameter("rock_rg", rock_rg)
		step.set_shader_parameter("noise_tex", noise_texture)
		step.set_shader_parameter("facet_grid", 5.0)  # smooth-shaded mesh: coarse rounding (see step 2)
		mat = step
	for mesh in $Rock.find_children("*", "MeshInstance3D", true, false):
		mesh.material_override = mat
	var header := _header_comment(FileAccess.get_file_as_string(path))
	title.text = ("BLENDER LOOK · " if view == 0 else "") + header[0]
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


func _add_panel() -> void:
	var layer := CanvasLayer.new()
	add_child(layer)
	var panel := PanelContainer.new()
	panel.position = Vector2(14, 12)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.05, 0.06, 0.09, 0.78)
	style.set_corner_radius_all(8)
	style.set_content_margin_all(12)
	panel.add_theme_stylebox_override("panel", style)
	layer.add_child(panel)
	var box := VBoxContainer.new()
	panel.add_child(box)
	title.add_theme_font_size_override("font_size", 20)
	box.add_child(title)
	notes.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	notes.custom_minimum_size = Vector2(600, 0)
	box.add_child(notes)
	var keys := Label.new()
	keys.text = "0 Blender look · 1–9 step · ← → prev/next · H hide · drag orbit · wheel zoom"
	keys.modulate = Color(1, 1, 1, 0.6)
	box.add_child(keys)


func _save(path: String) -> void:
	for f in 4:
		await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(path)


func _place_camera() -> void:
	$Camera3D.position = focus + Vector3(0, 0, distance).rotated(Vector3.RIGHT, pitch).rotated(Vector3.UP, yaw)
	$Camera3D.look_at(focus)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed:
		if event.keycode >= KEY_0 and event.keycode <= KEY_9:
			_show(event.keycode - KEY_0)
		elif event.keycode == KEY_RIGHT:
			_show(view + 1)
		elif event.keycode == KEY_LEFT:
			_show(view - 1)
		elif event.keycode == KEY_P:
			hud.measure_now()
		elif event.keycode == KEY_B:
			_bench()
		elif event.keycode == KEY_H:
			title.get_parent().get_parent().visible = not title.get_parent().get_parent().visible
	elif event is InputEventMouseMotion and event.button_mask & MOUSE_BUTTON_MASK_LEFT:
		yaw -= event.relative.x * 0.005
		pitch = clampf(pitch - event.relative.y * 0.005, deg_to_rad(-85), deg_to_rad(10))
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(distance / 1.1, 2.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(distance * 1.1, 40.0)
	_place_camera()


## Cost of every view (Blender look + steps 1–9), written into perf.md between the rock-bench markers.
## Same method as main.gd's bench: the rock fills the screen (camera close, straight on), 3D is rendered
## at 3× resolution (9× the pixels) so the GPU is the bottleneck, and each view is timed with
## PerfHud.measure_gpu (frames drawn back to back; Godot's GPU timer reads 0 on Metal).
## "shader cost" = minus step 1 (plain), the cheapest shader on the same pixels.
func _bench() -> void:
	var panel := title.get_parent().get_parent() as Control
	panel.visible = false
	hud.visible = false
	var vp := get_viewport()
	var old_scale := vp.scaling_3d_scale
	vp.scaling_3d_scale = 3.0
	var old_view := [yaw, pitch, distance, view]
	yaw = 0.0
	pitch = deg_to_rad(-8.0)
	distance = 2.2   # close enough that the rock covers the whole screen
	_place_camera()
	var times := []
	for v in STEPS.size() + 1:
		_show(v)
		for f in 30:  # let the shader compile and the picture settle
			await get_tree().process_frame
		times.append(hud.measure_gpu(240))
	var size := vp.get_visible_rect().size
	var rows := []
	for v in times.size():
		var name: String = "rock.gdshader (Blender look)" if v == 0 else STEPS[v - 1]
		var previous: float = times[v - 1] if v > 1 else times[1]
		rows.append("| %s | %s | %.2f | %s | %+.2f |" % [
			"0" if v == 0 else str(v), name, times[v],
			"—" if v <= 1 else "%+.2f" % (times[v] - previous), times[v] - times[1]])
	var table := "Measured %s · %s · %s renderer · 3D at %dx%d · 240 frames per view\n\n" % [
		Time.get_datetime_string_from_system(false, true), RenderingServer.get_video_adapter_name(),
		ProjectSettings.get_setting("rendering/renderer/rendering_method"), size.x * 3, size.y * 3]
	table += "| view | shader | ms / frame | vs previous step | shader cost (− step 1) |\n|---|---|---|---|---|\n"
	table += "\n".join(rows)
	_write_bench_table(table)
	print(table)
	vp.scaling_3d_scale = old_scale
	yaw = old_view[0]; pitch = old_view[1]; distance = old_view[2]
	_place_camera()
	_show(old_view[3])
	panel.visible = true
	hud.visible = true
	hud.last_measure = "rock bench done → perf.md"


## Replace the text between the rock-bench markers in perf.md (added under a Rock heading if missing).
func _write_bench_table(table: String) -> void:
	var path := ProjectSettings.globalize_path("res://perf.md")
	var doc := FileAccess.get_file_as_string(path)
	var start_tag := "<!-- rock-bench:start -->"
	var end_tag := "<!-- rock-bench:end -->"
	if doc.find(start_tag) == -1 or doc.find(end_tag) == -1:
		doc += "\n## Rock (rock/rock.tscn: Rock.blend mesh + rock_rg.png, the Blender look and steps 1–9)\n\n"
		doc += "Run: B in rock/rock.tscn, or `godot --path . res://rock/rock.tscn -- --bench`.\n\n"
		doc += "%s\n%s\n" % [start_tag, end_tag]
	var a := doc.find(start_tag)
	var b := doc.find(end_tag)
	doc = doc.substr(0, a + start_tag.length()) + "\n" + table + "\n" + doc.substr(b)
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(doc)
