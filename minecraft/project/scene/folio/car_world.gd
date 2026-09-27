extends Node3D
##
## ArcadeVehicle in the folio world, followed by FolioView, tuned toward the
## folio "toy" feel (light body, low CoM, soft/bouncy suspension, gentle top
## speed, exaggerated lean). Handling numbers live on the VehicleConfig in the
## scene so they can be tweaked without a rebuild.
##
## Wiring notes:
##  - FolioGame builds the ticker, the FolioView camera (auto-current), the sun
##    (FolioLighting) and the WorldEnvironment (FolioRendering); we don't add our own.
##  - GameManager provides the PlayerInput singleton; ArcadeVehicle self-registers
##    to it in its init, so keyboard/gamepad drive works with no extra wiring.
##  - Each physics frame we feed the car's world position to FolioView.
##

@onready var _game: Node = $Game
@onready var _car: Node3D = $Car
var _wire := false
var _colliders_shown := false
var _collider_overlays: Array[MeshInstance3D] = []

# Camera modes cycled with C (TAB is GameManager's target-swap action, so it
# can't drive the camera here — it would fight the target-switch and flip the
# GameCamera's mode back on the same press):
#   0 = folio diorama camera (FolioView)
#   1 = GameCamera Fly  (Blender-style: MMB orbit, Shift+MMB pan, wheel zoom)
#   2 = GameCamera Car  (arcade chase)
# The fly/chase behaviour is the project's own GameCamera (src/camera) driven by
# PlayerInput — we just switch which camera is current; no custom camera here.
var _cam_mode := 0
var _folio_cam: Camera3D = null
@onready var _game_cam: Camera3D = get_node_or_null("GameCamera")

func _ready() -> void:
	RenderingServer.set_debug_generate_wireframes(true)
	# Let the day/night cycle run: 2 minutes per full day. (Was locked to dawn,
	# which kept the whole world tinted warm/red the entire time.)
	if _game and _game.has_method("get_day_cycles"):
		var dc = _game.get_day_cycles()
		dc.set_duration(2.0 * 60.0)      # 2 min per day
		dc.set_progress_override(-1.0)   # unlock: real-time cycle
	# Year (season) cycle: 10 minutes for a full year (~2.5 min per season).
	if _game and _game.has_method("get_year_cycles"):
		_game.get_year_cycles().set_duration(10.0 * 60.0)
	# Hide the whole folio HUD (title + mute/menu) so this driving test is uncluttered.
	# FolioUI is a CanvasLayer, so hiding it removes the "Play" start screen too.
	if _game and _game.has_method("get_ui"):
		var ui = _game.get_ui()
		if ui:
			ui.visible = false
			# Demo minimap points (folio Map is content-agnostic; the scene supplies
			# its own points of interest). Press M in-game to open the map.
			var m = ui.get_map() if ui.has_method("get_map") else null
			if m:
				m.add_point("Plaza", Vector3(39.5, 0.0, 37.8))
				m.add_point("Origin", Vector3(0.0, 0.0, 0.0))
				m.add_point("Ramps", Vector3(27.0, 0.0, 24.0))
				m.add_point("North", Vector3(-10.0, 0.0, -70.0))
				m.add_point("West Shore", Vector3(-72.0, 0.0, 10.0))
	# Start on the folio diorama camera (C cycles to GameCamera Fly / Car).
	_set_cam_mode(0)
	# --shot=NAME: let the sim settle (car drops onto the ground, camera frames it)
	# then save docs/png/NAME.png and quit. For headless verification only.
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--shot="):
			await get_tree().create_timer(3.0).timeout
			for ca in OS.get_cmdline_args() + OS.get_cmdline_user_args():
				if ca.begins_with("--cam="):
					_set_cam_mode(int(ca.substr(6)))
					await get_tree().process_frame
					await get_tree().process_frame
			var img := get_viewport().get_texture().get_image()
			img.save_png("res://../docs/png/" + a.substr(7) + ".png")
			get_tree().quit()

func _physics_process(_delta: float) -> void:
	# --drive: hold throttle so the car drives itself (headless demo / screenshots).
	if OS.get_cmdline_args().has("--drive") or OS.get_cmdline_user_args().has("--drive"):
		Input.action_press("move_forward")
	if _game == null or _car == null:
		return
	if not _game.has_method("get_view"):
		return
	var view = _game.get_view()
	if view:
		view.set_target_position(_car.global_position)

# Resolve the folio diorama camera (bound C++ FolioView camera) once.
func _ensure_folio_cam() -> void:
	if _folio_cam == null and _game and _game.has_method("get_view"):
		var v = _game.get_view()
		if v and v.has_method("get_camera"):
			_folio_cam = v.get_camera()

func _cycle_cam() -> void:
	_set_cam_mode((_cam_mode + 1) % 3)

# Switch which camera is current. Fly/Car behaviour is the project's GameCamera
# (orbit/pan/dolly + chase, driven by PlayerInput) — we don't reimplement it.
func _set_cam_mode(m: int) -> void:
	_ensure_folio_cam()
	_cam_mode = m
	match _cam_mode:
		0:
			if _folio_cam:
				_folio_cam.make_current()
		1:
			if _game_cam:
				_game_cam.set_camera_mode(0) # GameCamera.MODE_FLY
				_game_cam.make_current()
		2:
			if _game_cam:
				_game_cam.set_camera_mode(1) # GameCamera.MODE_CAR
				_game_cam.make_current()
	var cur := get_viewport().get_camera_3d()
	var names := ["folio diorama", "GameCamera Fly", "GameCamera Car"]
	print("[cam] mode ", m, " -> ", names[m] if m < names.size() else "?", " (current: ", cur.name if cur else "none", ")")

func _unhandled_key_input(event: InputEvent) -> void:
	# Season test keys (like the world preview): jump season + let it keep running,
	# so we can eyeball snow / leaves / terrain interplay while driving.
	#   1 winter   2 spring   3 summer   4 fall   0 resume real time
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return
	if _game == null or not _game.has_method("get_year_cycles"):
		return
	var yc = _game.get_year_cycles()
	var weather = _game.get_weather() if _game.has_method("get_weather") else null
	match event.keycode:
		KEY_1: _seek_season(yc, weather, 0.125)
		KEY_2: _seek_season(yc, weather, 0.375)
		KEY_3: _seek_season(yc, weather, 0.625)
		KEY_4: _seek_season(yc, weather, 0.875)
		KEY_0:
			yc.set_progress_override(-1.0)
			if weather:
				weather.clear_override()
		KEY_V:
			_wire = not _wire
			get_viewport().debug_draw = Viewport.DEBUG_DRAW_WIREFRAME if _wire else Viewport.DEBUG_DRAW_DISABLED
		KEY_F3:
			_toggle_colliders()
		KEY_C:
			_cycle_cam()

# F3: overlay every collision shape in the scene as a wireframe (physics X-ray),
# the collider equivalent of V's mesh wireframe. Built from each Shape3D's own
# debug mesh and parented to its CollisionShape3D, so overlays follow moving
# bodies (crates, etc.). Toggling off frees them all.
func _toggle_colliders() -> void:
	_colliders_shown = not _colliders_shown
	if not _colliders_shown:
		for m in _collider_overlays:
			if is_instance_valid(m):
				m.queue_free()
		_collider_overlays.clear()
		return
	var shapes: Array[Node] = []
	_collect_collision_shapes(get_tree().root, shapes)
	for s in shapes:
		var cs := s as CollisionShape3D
		if cs == null or cs.shape == null:
			continue
		var dbg: Mesh = cs.shape.get_debug_mesh()
		if dbg == null:
			continue
		var mi := MeshInstance3D.new()
		mi.mesh = dbg
		mi.material_override = _collider_mat()
		mi.top_level = false
		cs.add_child(mi)
		_collider_overlays.append(mi)

func _collect_collision_shapes(n: Node, out: Array[Node]) -> void:
	if n is CollisionShape3D:
		out.append(n)
	for c in n.get_children():
		_collect_collision_shapes(c, out)

func _collider_mat() -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.albedo_color = Color(0.1, 1.0, 0.4, 0.85)     # bright green X-ray
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.no_depth_test = true                          # see colliders through geometry
	return m

func _seek_season(yc, weather, phase: float) -> void:
	if yc.has_method("seek_season"):
		yc.seek_season(phase)
	else:
		yc.set_progress_override(phase)
	if weather:
		weather.clear_override()
