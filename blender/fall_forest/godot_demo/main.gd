extends Node3D
## The fall forest test: a chunked terrain, grass round the camera (GrassField), the fall asset
## pack's props (PropScatter), and the ways to keep it fast switched from the debug panel
## (DebugPanel).
## `-- --capture` instead times two views with each speed-up off in turn, saves shots/ and quits.

const VIEWS := {
	"grass": [Vector3(-60.0, 1.7, 40.0), Vector3(-20.0, 0.5, 10.0)], # eye height over the ground
	"ground": [Vector3(-40.0, 3.0, 120.0), Vector3(30.0, 2.0, -40.0)],
	"overview": [Vector3(-170.0, 70.0, 210.0), Vector3(0.0, 0.0, 0.0)],
}
const CASES := [
	["all on", {}],
	["no occlusion culling", {"occlusion culling": false}],
	["no near grass", {"near grass": false}],
	["no far grass", {"far grass": false}],
	["no grass", {"near grass": false, "far grass": false}],
	["no props", {"props": false}],
	["no visibility ranges", {"visibility ranges": false}],
	["no mesh LOD", {"mesh LOD": false}],
	["no shadows", {"shadows": false}],
]
const FRAMES := 120

@onready var camera: FlyCamera = $Camera
@onready var panel: DebugPanel = $DebugPanel


func _ready() -> void:
	_view("grass")
	if "--capture" in OS.get_cmdline_user_args():
		_capture()


func _view(p_name: String) -> void:
	var from: Vector3 = VIEWS[p_name][0]
	var at: Vector3 = VIEWS[p_name][1]
	from.y += TerrainHeight.height(from.x, from.z)
	at.y += TerrainHeight.height(at.x, at.z)
	camera.place(from, at)


func _capture() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	var rows := PackedStringArray()
	for view: String in VIEWS:
		_view(view)
		for case: Array in CASES:
			for name: String in DebugPanel.SWITCHES:
				panel.apply(name, name != "show occluders")
			for name: String in case[1]:
				panel.apply(name, case[1][name])
			var stats := await _measure()
			rows.append("| %s | %s | %.2f | %d | %d | %.2f |" % [view, case[0], stats[0], stats[1], stats[2], stats[3]])
			if case[0] == "all on":
				await _shot("fall_forest_%s.png" % view)
		for name: String in DebugPanel.SWITCHES:
			panel.apply(name, true)  # occluders are only drawn with occlusion culling on
		await _shot("fall_forest_%s_occluders.png" % view)
		panel.apply("show occluders", false)
	print("| view | case | ms / frame | draw calls | objects | M triangles |\n|---|---|---|---|---|---|\n" + "\n".join(rows))
	get_tree().quit()


## [ms a frame (FRAMES drawn back to back), draw calls, objects, million triangles]
func _measure() -> Array:
	for f in 20:
		await get_tree().process_frame
	OS.delay_msec(300)
	for f in 5:
		await get_tree().process_frame
	var calls := Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
	var objects := Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME)
	var triangles := Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME) / 1000000.0
	RenderingServer.force_sync()
	var start := Time.get_ticks_usec()
	for f in FRAMES:
		RenderingServer.force_draw(false)
	RenderingServer.force_sync()
	return [(Time.get_ticks_usec() - start) / 1000.0 / FRAMES, calls, objects, triangles]


func _shot(p_file: String) -> void:
	for f in 10:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + p_file))
