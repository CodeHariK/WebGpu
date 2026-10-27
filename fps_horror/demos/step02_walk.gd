## Step 2 — walk around the test level in first person.
##   click to grab the mouse (Esc lets go) · WASD move · Shift sprint · Space jump · Ctrl/C crouch
##
## Test run: Godot --path . res://demos/step02_walk.tscn -- --shot
##   an autopilot climbs the stairs, walks down the ramp, crawls into the tunnel (and tries to
##   stand up in it), jumps onto the crate and sprints down the mine; it prints where the player
##   ended up after each move and saves shots/step02_*.png
##
## Later steps extend this script: they override _moves(), _title() and _status_extra().
extends Node3D

const InputActions := preload("res://src/input/input_actions.gd")
const Level := preload("res://src/level/test_level.gd")
const PLAYER := preload("res://scenes/player.tscn")

## Autopilot moves: [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const MOVES := [
	["look at the stairs", Vector3(3, 0.05, -8), 90, -8, [], 0.6, true],
	["climb the stairs", null, 90, 0, ["move_forward"], 3.2, false],
	["top of the ramp", null, 180, -14, [], 0.6, true],
	["walk down the ramp", null, 180, 0, ["move_forward"], 3.0, false],
	["crouch at the tunnel", Vector3(8, 0.05, 3), 0, -4, ["crouch"], 0.8, true],
	["crawl in", null, 0, 0, ["crouch", "move_forward"], 3.0, false],
	["let go of crouch inside", null, 0, -4, [], 1.0, true],
	["run at the crate", Vector3(0, 0.05, 0.6), 0, -12, ["move_forward"], 0.25, false],
	["jump onto it", null, 0, -12, ["move_forward", "jump"], 0.9, true],
	["mine corridor", Vector3(-1.2, 0.05, 17), 180, -3, [], 0.6, true],
	["sprint down it", null, 180, 0, ["move_forward", "sprint"], 3.0, true],
]

var level: Node3D
var player: CharacterBody3D
var label := Label.new()
var _auto := false
var _move := -1
var _time := 0.0
var _held: Array = []
var _shot := 0
var _skip_frame := false  # saving a screenshot makes the next frame long: don't count it


func _ready() -> void:
	InputActions.ensure()
	level = Level.new()
	add_child(level)
	player = PLAYER.instantiate()
	add_child(player)
	player.global_position = Level.SPAWN
	_environment()
	_hud()
	_auto = "--shot" in OS.get_cmdline_user_args()
	_time = -1.5  # let everything settle first


func _moves() -> Array:
	return MOVES


func _title() -> String:
	return "Step 2 · walk   (click to grab mouse · WASD · Shift sprint · Space jump · Ctrl/C crouch)"


func _status_extra() -> String:
	return ""


func _shot_prefix() -> String:
	return "step02"


func _environment() -> void:
	var env := _make_environment()
	var world := WorldEnvironment.new()
	world.environment = env
	add_child(world)


func _make_environment() -> Environment:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.01, 0.01, 0.012)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.45, 0.42, 0.4)
	env.ambient_light_energy = 0.25
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.fog_enabled = true
	env.fog_light_color = Color(0.05, 0.045, 0.04)
	env.fog_density = 0.03
	env.glow_enabled = true
	return env


func _hud() -> void:
	label.position = Vector2(14, 10)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 5)
	var layer := CanvasLayer.new()
	layer.add_child(label)
	add_child(layer)


func _process(delta: float) -> void:
	if _auto:
		_autopilot(delta)
	label.text = _status()


func _status() -> String:
	var flat := Vector2(player.velocity.x, player.velocity.z).length()
	var state := "crouching" if player.crouching else ("sprinting" if player.sprinting else ("on floor" if player.is_on_floor() else "in the air"))
	var lines: PackedStringArray = [
		_title(),
		"%s · %.1f m/s · height %.2f m · at (%.1f, %.1f, %.1f)" % [state, flat, player.height,
			player.global_position.x, player.global_position.y, player.global_position.z],
		"frame: %d fps · %d draw calls · %d primitives" % [Engine.get_frames_per_second(),
			Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
			Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME)],
	]
	var extra := _status_extra()
	if extra != "":
		lines.append(extra)
	var moves := _moves()
	if _auto and _move >= 0 and _move < moves.size():
		lines.append("autopilot: " + moves[_move][0])
	return "\n".join(lines)


## Runs the moves one after another by pressing the same input actions a player would.
func _autopilot(delta: float) -> void:
	var moves := _moves()
	_release_strays()
	if _skip_frame:
		_skip_frame = false
		delta = 0.0
	_time += delta
	if _move >= 0 and _time < moves[_move][5]:
		return
	if _move >= 0:
		_finish_move(moves[_move])
	_move += 1
	if _move == moves.size():
		_release_all()
		get_tree().quit()
		return
	_start_move(moves[_move])


func _start_move(move: Array) -> void:
	_release_all()
	if move[1] != null:
		player.global_position = move[1]
		player.velocity = Vector3.ZERO
	player.get_node("Head").face(deg_to_rad(move[2]), deg_to_rad(move[3]))
	for action: String in move[4]:
		Input.action_press(action)
	_held = move[4]
	_time = 0.0


func _finish_move(move: Array) -> void:
	var p := player.global_position
	var extra := _status_extra()
	print("%-26s → (%.2f, %.2f, %.2f)  %s  crouching=%s  height=%.2f  %s" % [move[0], p.x, p.y, p.z,
		"floor" if player.is_on_floor() else "air", player.crouching, player.height, extra])
	if move[6]:
		DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
		RenderingServer.force_draw()  # in case the OS paused drawing (window covered)
		get_viewport().get_texture().get_image().save_png("res://shots/%s_%d.png" % [_shot_prefix(), _shot])
		_shot += 1
		_skip_frame = true


## Test runs press only what the move says: anything else held (stray input reaching the window
## while the test runs) is let go, so every run behaves the same.
func _release_strays() -> void:
	for action: String in InputActions.KEYS.keys() + InputActions.MOUSE.keys():
		if not action in _held and Input.is_action_pressed(action):
			print("autopilot: released stray input '%s'" % action)
			Input.action_release(action)


func _release_all() -> void:
	for action: String in _held:
		Input.action_release(action)
	_held = []

