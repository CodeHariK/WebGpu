## Step 5 — the flashlight, in the dark. Every lamp in the level is off: you see only what the
## flashlight (and the muzzle flash) lights up.
##   F flashlight on/off · L level lamps on/off · plus step 4: LMB fire · RMB aim · R reload · WASD …
##
## Test run: Godot --path . res://demos/step05_flashlight.tscn -- --shot  → shots/step05_*.png
##   looks down the dark mine with the beam, snaps round (the beam lags), switches it off (black),
##   fires in the dark (the flash lights the room), and compares with the lamps on
extends "res://demos/step04_shoot.gd"

## [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const LIGHT_MOVES := [
	["mine, flashlight on", Vector3(-1.2, 0.05, 18), 180, -4, [], 1.2, true],
	["walk in", null, 180, -4, ["move_forward"], 2.0, false],
	["the rail cart", null, 175, -14, [], 1.0, true],
	["snap round (beam lags)", null, 120, -4, [], 0.06, true],
	["settle", null, 120, -4, [], 1.0, false],
	["the hall, at the targets", Vector3(-6, 0.05, 9), 0, -6, [], 1.2, true],
	["switch it off", null, 0, -6, ["flashlight"], 0.6, true],
	["fire in the dark", null, 0, -6, ["fire"], 0.03, true],
	["back on", null, 0, -6, ["flashlight"], 0.8, false],
	["lamps on to compare", null, 0, -6, [], 1.0, true],
]

var _lamps_lit := false


func _ready() -> void:
	super()
	level.set_lamps(false)


func _make_environment() -> Environment:
	var env := super()
	env.ambient_light_energy = 0.04  # near black: the flashlight does the work
	env.fog_density = 0.04
	return env


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.physical_keycode == KEY_L:
		_lamps_lit = not _lamps_lit
		level.set_lamps(_lamps_lit)


func _start_move(move: Array) -> void:
	super(move)
	if move[0] == "lamps on to compare":
		level.set_lamps(true)


func _moves() -> Array:
	return LIGHT_MOVES


func _title() -> String:
	return "Step 5 · flashlight   (F flashlight · L lamps · LMB fire · RMB aim · R reload · WASD · Shift · Space · Ctrl)"


func _shot_prefix() -> String:
	return "step05"
