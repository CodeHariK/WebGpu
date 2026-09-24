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

# Camera modes: 0 = folio (FolioView), 1 = free-fly, 2 = car chase.
# Cycle with Tab. Fly: WASD move, Q/E down/up, arrows look, Shift = faster.
# Chase rides behind the car (drive it with WASD in this mode).
var _cam_mode := 0
var _folio_cam: Camera3D = null
var _fly_cam: Camera3D = null
var _chase_cam: Camera3D = null

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

func _process(delta: float) -> void:
	if _cam_mode == 1 and _fly_cam:
		_fly_update(delta)
	elif _cam_mode == 2 and _chase_cam and _car:
		_chase_update(delta)

# Lazily create the alternate cameras and grab the folio camera (bound C++).
func _ensure_cams() -> void:
	if _folio_cam == null and _game and _game.has_method("get_view"):
		var v = _game.get_view()
		if v and v.has_method("get_camera"):
			_folio_cam = v.get_camera()
	if _fly_cam == null:
		_fly_cam = Camera3D.new()
		_fly_cam.fov = 45.0
		_fly_cam.far = 600.0
		add_child(_fly_cam)
	if _chase_cam == null:
		_chase_cam = Camera3D.new()
		_chase_cam.fov = 38.0
		_chase_cam.far = 400.0
		add_child(_chase_cam)

func _cycle_cam() -> void:
	_set_cam_mode((_cam_mode + 1) % 3)

func _set_cam_mode(m: int) -> void:
	_ensure_cams()
	_cam_mode = m
	match _cam_mode:
		0:
			if _folio_cam:
				_folio_cam.make_current()
		1:
			# Seed the fly cam from whatever view is on screen for a smooth start.
			var cur := get_viewport().get_camera_3d()
			if cur:
				_fly_cam.global_transform = cur.global_transform
			_fly_cam.make_current()
		2:
			if _car:
				_chase_update(1.0) # snap into place this frame
			_chase_cam.make_current()

func _fly_update(delta: float) -> void:
	var fast := 3.0 if Input.is_key_pressed(KEY_SHIFT) else 1.0
	var spd := 22.0 * fast
	var look := 1.6 * delta
	if Input.is_key_pressed(KEY_LEFT):
		_fly_cam.rotate_y(look)
	if Input.is_key_pressed(KEY_RIGHT):
		_fly_cam.rotate_y(-look)
	if Input.is_key_pressed(KEY_UP):
		_fly_cam.rotate_object_local(Vector3(1, 0, 0), look)
	if Input.is_key_pressed(KEY_DOWN):
		_fly_cam.rotate_object_local(Vector3(1, 0, 0), -look)
	var b := _fly_cam.global_transform.basis
	var dir := Vector3.ZERO
	if Input.is_key_pressed(KEY_W):
		dir -= b.z
	if Input.is_key_pressed(KEY_S):
		dir += b.z
	if Input.is_key_pressed(KEY_A):
		dir -= b.x
	if Input.is_key_pressed(KEY_D):
		dir += b.x
	if Input.is_key_pressed(KEY_E):
		dir += Vector3.UP
	if Input.is_key_pressed(KEY_Q):
		dir -= Vector3.UP
	_fly_cam.global_position += dir * spd * delta

func _chase_update(delta: float) -> void:
	var ct := _car.global_transform
	# Car forward is -Z, so +Z basis is behind it.
	var behind := ct.basis.z.normalized()
	var target := ct.origin + behind * 8.5 + Vector3.UP * 4.0
	var t := clampf(delta * 6.0, 0.0, 1.0)
	_chase_cam.global_position = _chase_cam.global_position.lerp(target, t)
	_chase_cam.look_at(ct.origin + Vector3.UP * 1.2, Vector3.UP)

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
		KEY_TAB:
			_cycle_cam()

func _seek_season(yc, weather, phase: float) -> void:
	if yc.has_method("seek_season"):
		yc.seek_season(phase)
	else:
		yc.set_progress_override(phase)
	if weather:
		weather.clear_override()
