## Disco hall demo (disco/): the Boogey-Pod's arena on its own, dancing to a 118 bpm clock (no
## audio yet). Floor patterns and light moves change every few bars by themselves.
##   right-drag orbit · middle-drag pan · wheel zoom · right-drag + WASD fly (fly_camera.gd)
##   Space  pause the beat     N  next floor pattern     T  telegraph: flash a random tile
##   B      beams on/off       1-4  camera views          Tab  panel
extends Node3D

const VIEWS := [
	{"pivot": Vector3(0, 1.0, -1.0), "yaw": 0.55, "pitch": -0.5, "distance": 34.0}, # three-quarter
	{"pivot": Vector3(0, 2.0, -2.0), "yaw": 0.0, "pitch": -0.22, "distance": 30.0}, # front, low
	{"pivot": Vector3(0, 0.0, -1.0), "yaw": 0.0, "pitch": -1.35, "distance": 34.0}, # top down
	{"pivot": Vector3(0, 3.5, 0.0), "yaw": 2.6, "pitch": -0.15, "distance": 14.0}, # from the DJ stage
]

var beat: DiscoBeat
var hall: DiscoHall
var lights: DiscoLights
var environment: Environment
var camera: FlyCamera
var panel: OrbPanel
var hud: Label


func _ready() -> void:
	environment = DiscoEnv.build(self)
	beat = DiscoBeat.new()
	add_child(beat)
	hall = DiscoHall.new()
	hall.beat = beat
	add_child(hall)
	lights = DiscoLights.new()
	lights.beat = beat
	lights.hang_y = DiscoHall.TRUSS_Y - 0.6
	lights.floor_half = hall.main_floor.half_extent().x
	add_child(lights)
	camera = FlyCamera.new()
	camera.fov = 50.0
	add_child(camera)
	camera.current = true
	view(0)
	var layer := CanvasLayer.new()
	add_child(layer)
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
	panel.visible = false
	layer.add_child(panel)
	_fill_panel()


func _process(_delta: float) -> void:
	hud.text = "%d bpm  ·  bar %d beat %d%s\nfloor: %s%s   lights: %s\nSpace pause · N pattern · T telegraph · B beams · 1-4 views · Tab panel" % [
		beat.bpm, beat.bar(), beat.beat_in_bar() + 1, "  (paused)" if not beat.playing else "",
		hall.main_floor.pattern, " (auto)" if hall.main_floor.auto else "", lights.move]


## Jump the camera to one of VIEWS.
func view(index: int) -> void:
	var v: Dictionary = VIEWS[index]
	var basis := Basis.from_euler(Vector3(v.pitch, v.yaw, 0.0))
	camera.global_transform = Transform3D(basis, v.pivot + basis.z * float(v.distance))
	camera.take_over(v.distance)


## Flash a random tile on the main floor (the boss telegraph).
func telegraph() -> void:
	var grid := hall.main_floor
	grid.warn(Vector2i(randi() % grid.columns, randi() % grid.rows), 60.0 / beat.bpm * 2.0)


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_SPACE: beat.playing = not beat.playing
		KEY_N: _next_pattern()
		KEY_T: telegraph()
		KEY_B: lights.beams_visible = not lights.beams_visible
		KEY_TAB: panel.visible = not panel.visible
		KEY_1, KEY_2, KEY_3, KEY_4: view(key.keycode - KEY_1)


func _next_pattern() -> void:
	hall.main_floor.auto = false
	hall.main_floor.next_pattern()


func _fill_panel() -> void:
	panel.heading("Beat")
	panel.slider("bpm", 60.0, 180.0, 1.0, func() -> float: return beat.bpm, func(v: float) -> void: beat.bpm = v, "%.0f")
	panel.checkbox("playing", func() -> bool: return beat.playing, func(on: bool) -> void: beat.playing = on)
	panel.heading("Floor")
	panel.checkbox("auto patterns", func() -> bool: return hall.main_floor.auto, func(on: bool) -> void: hall.main_floor.auto = on)
	panel.button("next pattern (N)", _next_pattern)
	panel.slider("brightness", 0.0, 2.0, 0.05, func() -> float: return hall.main_floor.brightness, func(v: float) -> void:
		for grid in hall.floors:
			grid.brightness = v)
	panel.button("telegraph a tile (T)", telegraph)
	panel.heading("Lights")
	panel.checkbox("beams (B)", func() -> bool: return lights.beams_visible, func(on: bool) -> void: lights.beams_visible = on)
	panel.slider("light energy", 0.0, 2.0, 0.05, func() -> float: return lights.energy, func(v: float) -> void: lights.energy = v)
	panel.heading("Air")
	panel.slider("haze", 0.0, 0.05, 0.001, func() -> float: return environment.fog_density, func(v: float) -> void: environment.fog_density = v, "%.3f")
	panel.slider("glow", 0.0, 2.0, 0.05, func() -> float: return environment.glow_intensity, func(v: float) -> void: environment.glow_intensity = v)
