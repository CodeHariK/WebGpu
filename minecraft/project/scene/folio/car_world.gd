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

func _ready() -> void:
	RenderingServer.set_debug_generate_wireframes(true)
	# Lock to a bright daytime phase so the world + car read clearly for the test.
	if _game and _game.has_method("get_day_cycles"):
		_game.get_day_cycles().set_progress_override(0.15)
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

func _seek_season(yc, weather, phase: float) -> void:
	if yc.has_method("seek_season"):
		yc.seek_season(phase)
	else:
		yc.set_progress_override(phase)
	if weather:
		weather.clear_override()
