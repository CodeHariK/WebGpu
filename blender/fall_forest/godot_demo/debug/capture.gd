class_name Capture
extends Node
## Times the scene from fixed views with things switched off in turn, prints a table (on a phone:
## adb logcat, tag godot) and quits.
##   capture    -- --capture, or an export with the "capture" feature: the debug panel's switches
##   diagnose   -- --diagnose, or the "diagnose" feature: what the props cost on a phone (each
##              kind alone, their shading, the 3D resolution)
## Before timing, every view and case is drawn a few times (warm-up): the first time a material
## is drawn its shader is compiled, and meanwhile Godot draws with a slower general shader, so
## the first numbers would be too high.

const VIEWS := {
	"grass": [Vector3(-60.0, 1.7, 40.0), Vector3(-20.0, 0.5, 10.0)], # eye height over the ground
	"ground": [Vector3(-40.0, 3.0, 120.0), Vector3(30.0, 2.0, -40.0)],
	"overview": [Vector3(-170.0, 70.0, 210.0), Vector3(0.0, 0.0, 0.0)],
}
const CAPTURE := [
	["all on", {}],
	["no occlusion culling", {"occlusion culling": false}],
	["no near grass", {"near grass": false}],
	["no far grass", {"far grass": false}],
	["no grass", {"near grass": false, "far grass": false}],
	["no props", {"props": false}],
	["no tree billboards", {"tree billboards": false}],
	["no visibility ranges", {"visibility ranges": false}],
	["no mesh LOD", {"mesh LOD": false}],
	["no shadows", {"shadows": false}],
]
## keys: a debug panel switch, "kind:<PropKinds name>" (false hides it), "shading" (PropScatter's
## modes), "scale" (the 3D resolution, x the screen)
const DIAGNOSE := [
	["all on", {}],
	["no props", {"props": false}],
	["no tree billboards", {"tree billboards": false}],
	["no trees", {"kind:trees": false}],
	["no bushes", {"kind:bushes": false}],
	["no small props", {"kind:pumpkins": false, "kind:mushrooms": false, "kind:nuts": false, "kind:leaves": false}],
	["props: no shadows received", {"shading": "no shadows received"}],
	["props: unshaded", {"shading": "unshaded"}],
	["no shadows", {"shadows": false}],
	["3D at 0.75", {"scale": 0.75}],
	["3D at 0.5", {"scale": 0.5}],
	["3D at 0.5, no props", {"scale": 0.5, "props": false}],
]
const FRAMES := 120 ## frames drawn back to back for a timing (half on a phone)
const WARM_FRAMES := 6

var camera: FlyCamera
var panel: DebugPanel
var props: PropScatter


## Run the capture (or with p_diagnose the diagnosis), then quit.
func run(p_diagnose: bool) -> void:
	var cases: Array = DIAGNOSE if p_diagnose else CAPTURE
	var views: Array = ["ground", "overview"] if p_diagnose else VIEWS.keys()
	RenderingServer.viewport_set_measure_render_time(get_viewport().get_viewport_rid(), true)
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(_shots_dir()))
	await _warm_up(views, cases)
	var rows := PackedStringArray()
	for view: String in views:
		place(view)
		for case: Array in cases:
			_apply(case[1])
			var stats := await _measure()
			rows.append("| %s | %s | %.2f | %.2f | %d | %d | %.2f |" % [view, case[0], stats[0], stats[4], stats[1], stats[2], stats[3]])
			print(rows[-1])
			if case[0] == "all on" and not p_diagnose:
				await _shot("fall_forest_%s.png" % view)
		if not p_diagnose:
			_apply({"show occluders": true}) # occluders are only drawn with occlusion culling on
			await _shot("fall_forest_%s_occluders.png" % view)
	_apply({})
	var size := get_viewport().get_visible_rect().size
	# a line at a time: Android's log cuts a message at about 4 KB
	print("%s, %s, %dx%d" % [OS.get_name(), RenderingServer.get_video_adapter_name(), size.x, size.y])
	print("| view | case | ms / frame | GPU ms | draw calls | objects | M triangles |\n|---|---|---|---|---|---|---|")
	for row in rows:
		print(row)
	get_tree().quit()


## Put the camera at a view (its heights are over the ground).
func place(p_view: String) -> void:
	var from: Vector3 = VIEWS[p_view][0]
	var at: Vector3 = VIEWS[p_view][1]
	from.y += TerrainHeight.height(from.x, from.z)
	at.y += TerrainHeight.height(at.x, at.z)
	camera.place(from, at)


## Everything back on, then this case's changes.
func _apply(p_case: Dictionary) -> void:
	for name: String in DebugPanel.SWITCHES:
		panel.apply(name, name != "show occluders")
	for kind: Dictionary in PropKinds.KINDS:
		props.set_kind_visible(kind["name"], true)
	props.set_shading("lit")
	get_viewport().scaling_3d_scale = 1.0
	for key: String in p_case:
		if key.begins_with("kind:"):
			props.set_kind_visible(key.trim_prefix("kind:"), p_case[key])
		elif key == "shading":
			props.set_shading(p_case[key])
		elif key == "scale":
			get_viewport().scaling_3d_scale = p_case[key]
		else:
			panel.apply(key, p_case[key])


func _warm_up(p_views: Array, p_cases: Array) -> void:
	for view: String in p_views:
		place(view)
		for case: Array in p_cases:
			_apply(case[1])
			for f in WARM_FRAMES:
				await get_tree().process_frame
	OS.delay_msec(1000)


## [ms a frame (frames drawn back to back), draw calls, objects, million triangles, GPU ms (the
## GPU timer, of the last normal frame: 0 where the driver has none, as on macOS's Metal)]
func _measure() -> Array:
	for f in 20:
		await get_tree().process_frame
	OS.delay_msec(300)
	for f in 5:
		await get_tree().process_frame
	var calls := Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
	var objects := Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME)
	var triangles := Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME) / 1000000.0
	var gpu := RenderingServer.viewport_get_measured_render_time_gpu(get_viewport().get_viewport_rid())
	var frames := FRAMES / 2 if OS.has_feature("mobile") else FRAMES
	RenderingServer.force_sync()
	var start := Time.get_ticks_usec()
	for f in frames:
		RenderingServer.force_draw(false)
	RenderingServer.force_sync()
	return [(Time.get_ticks_usec() - start) / 1000.0 / frames, calls, objects, triangles, gpu]


func _shot(p_file: String) -> void:
	for f in 10:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path(_shots_dir() + p_file))


## The project folder in the editor; a phone's app folder (res:// is read-only in an export).
func _shots_dir() -> String:
	return "res://shots/" if OS.has_feature("editor") else "user://shots/"
