## How long each simple-waterfall shader takes to fill the screen, live (its noises worked out per
## pixel) and baked (read from noise/), next to the island's water and snow.
## Timed the way the island's bench does it (water_tutorial/perf.md, PerfHud.measure_gpu), so the
## numbers can sit side by side:
##   - one shader on a flat plane seen straight down fills the whole screen
##   - 3D drawn at 3 x the window (1600 x 900 -> 4800 x 2700, 13 million pixels): the GPU is the
##     slow part, not the CPU
##   - FRAMES frames drawn back to back, the wall time / FRAMES (Godot's GPU timer reads 0 on Metal)
##   - each shader timed RUNS times, the fastest kept (a slower run caught something else)
##   - "cost" = minus a flat unshaded colour on the same plane: the shader's own time
## Run in the liquid project (the simple waterfall's shaders, the mobile water, and the one-sheet
## scene from its camera) and in the island project (its water and snow), with the mobile
## renderer in both (`make waterfall-bench`):
##   godot --rendering-method mobile --path liquid -s tools/bench_waterfall.gd
##   godot --rendering-method mobile --path water_tutorial -s <this file's full path>
## On the plane the mobile water takes its flat path. Bubbles and puffs are cut out (alpha scissor) over most of the screen in a real scene.
extends SceneTree

const FRAMES := 240
const RUNS := 3
const WINDOW := Vector2i(1600, 900)
const BASELINE := "shader_type spatial;\nrender_mode unshaded;\nvoid fragment() { ALBEDO = vec3(0.2, 0.5, 0.7); }\n"

# [label, shader, parameters]; a parameter given as "res://..." is loaded
const LIQUID_SHADERS := [
	["sheet: live (step 9)", "res://waterfall_simple/steps/step09_variety.gdshader", {}],
	["sheet: baked noise (step 10)", "res://waterfall_simple/waterfall_simple.gdshader", {}],
	["pool: live (step 4)", "res://waterfall_simple/pool_steps/step04_tiers.gdshader", {}],
	["pool: baked noise (step 5)", "res://waterfall_simple/waterfall_pool.gdshader", {}],
	["froth: live (step 5)", "res://waterfall_simple/froth_steps/step05_light_rim.gdshader", {}],
	["froth: baked noise (step 6)", "res://waterfall_simple/waterfall_froth.gdshader", {}],
	["stones: live (step 3)", "res://waterfall_simple/stone_steps/step03_cracks.gdshader", {}],
	["stones: baked noise (step 4)", "res://waterfall_simple/waterfall_stone.gdshader", {}],
	["bubbles: live (step 2)", "res://waterfall_simple/bubble_steps/step02_wobble.gdshader", {}],
	["bubbles: baked noise (step 3)", "res://waterfall_simple/waterfall_bubble.gdshader", {}],
	["water_mobile (flat)", "res://water_mobile/water_mobile.gdshader", {}],
]
const LIQUID_SCENES := [
	["scene: one sheet (waterfall_simple.tscn)", "res://waterfall_simple/waterfall_simple.tscn"],
	["scene: full (waterfall_full.tscn)", "res://waterfall_simple/waterfall_full.tscn"],
]
const ISLAND_SHADERS := [
	["island water (step 13, opaque)", "res://water_steps/step13_opaque.gdshader",
		{"noise_tex": "res://textures/noise_128.png", "water_macro": "res://water_texture/water_macro.png"}],
	["snow (snow.gdshader)", "res://snow/snow.gdshader",
		{"noise_tex": "res://textures/noise_128.png", "snow_everywhere": 1.0}],
	["snow (step 9, baked)", "res://snow_steps/step09_baked.gdshader",
		{"noise_tex": "res://textures/noise_128.png", "snow_everywhere": 1.0,
		"snow_drifts": "res://snow_texture/snow_drifts.png", "snow_macro": "res://snow_texture/snow_macro.png",
		"snow_sparkle": "res://snow_texture/snow_sparkle.png"}],
]

var stage := Node3D.new()
var plane := MeshInstance3D.new()
var camera := Camera3D.new()


func _initialize() -> void:
	_run()


func _run() -> void:
	print("bench: started")
	root.size = WINDOW
	root.scaling_3d_scale = 3.0
	_build_stage()
	var island := FileAccess.file_exists("res://water_steps/step13_opaque.gdshader")
	var rows := PackedStringArray()
	var baseline := await _time_shader(_shader_from_code(BASELINE), {})
	rows.append(_row("flat colour (baseline)", baseline, baseline))
	print(rows[-1])
	for entry in (ISLAND_SHADERS if island else LIQUID_SHADERS):
		var ms := await _time_shader(load(entry[1]), entry[2])
		rows.append(_row(entry[0], ms, baseline))
		print(rows[-1])
	if not island:
		stage.visible = false
		for entry in LIQUID_SCENES:
			var ms := await _time_scene(entry[1])
			rows.append("| %s | %.2f | |" % [entry[0], ms])
			print(rows[-1])
	print("Measured %s · %s · %s renderer · 3D at %dx%d · %d frames, fastest of %d\n" % [
		Time.get_datetime_string_from_system(false, true), RenderingServer.get_video_adapter_name(),
		RenderingServer.get_current_rendering_method(), WINDOW.x * 3, WINDOW.y * 3, FRAMES, RUNS])
	print("| shader | ms / frame | cost (− flat colour) |\n|---|---|---|\n" + "\n".join(rows))
	quit()


# A plane 60 m square under a camera looking straight down that sees 36 x 20 m of it, and a sun.
func _build_stage() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	mesh.subdivide_width = 32
	mesh.subdivide_depth = 32
	plane.mesh = mesh
	stage.add_child(plane)
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 20.0
	camera.position = Vector3(0, 10, 0)
	camera.rotation_degrees = Vector3(-90, 0, 0)
	stage.add_child(camera)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-60, 150, 0)
	stage.add_child(sun)
	root.add_child(stage)
	camera.make_current()


func _shader_from_code(p_code: String) -> Shader:
	var shader := Shader.new()
	shader.code = p_code
	return shader


func _time_shader(p_shader: Shader, p_parameters: Dictionary) -> float:
	var material := ShaderMaterial.new()
	material.shader = p_shader
	for name in p_parameters:
		var value = p_parameters[name]
		material.set_shader_parameter(name, load(value) if value is String else value)
	plane.material_override = material
	return await _time_view()


# The scene from its own camera (the one marked current in it).
func _time_scene(p_path: String) -> float:
	var scene: Node = load(p_path).instantiate()
	root.add_child(scene)
	var ms := await _time_view()
	scene.queue_free()
	await process_frame
	camera.make_current()
	return ms


# Settle (the shader compiles in the background: the first frames draw a slower general version),
# then the fastest of RUNS timings of FRAMES frames drawn back to back.
func _time_view() -> float:
	for f in 30:
		await process_frame
	OS.delay_msec(500)
	for f in 10:
		await process_frame
	var best := INF
	for run in RUNS:
		RenderingServer.force_sync()
		var start := Time.get_ticks_usec()
		for f in FRAMES:
			RenderingServer.force_draw(false)
		RenderingServer.force_sync()
		best = minf(best, (Time.get_ticks_usec() - start) / 1000.0 / FRAMES)
	return best


func _row(p_label: String, p_ms: float, p_baseline: float) -> String:
	return "| %s | %.2f | %.2f |" % [p_label, p_ms, p_ms - p_baseline]
