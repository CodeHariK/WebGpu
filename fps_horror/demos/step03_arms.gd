## Step 3 — first-person arms and revolver in the test level.
##   click to grab the mouse · right mouse (hold) aim · left mouse fire · R reload
##   plus step 2's movement: WASD · Shift sprint · Space jump · Ctrl/C crouch
##
## Test run: Godot --path . res://demos/step03_arms.tscn -- --shot  → shots/step03_*.png
##   the autopilot raises the gun, fires aimed and from the hip, reloads, walks (sway), sprints
##   (gun tips down) and snaps the view round (the gun lags behind it)
extends "res://demos/step02_walk.gd"

## [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const ARMS_MOVES := [
	["gun low (idle)", Vector3(-1.2, 0.05, 20), 180, -2, [], 1.2, true],
	["raise to aim", null, 180, -2, ["aim"], 1.2, true],
	["fire aimed", null, 180, -2, ["aim", "fire"], 0.12, true],
	["settle", null, 180, -2, ["aim"], 0.8, false],
	["lower", null, 180, -2, [], 1.2, false],
	["fire from the hip", null, 180, -2, ["fire"], 0.12, true],
	["settle", null, 180, -2, [], 0.8, false],
	["reload", null, 180, -2, ["reload"], 2.5, true],
	["finish reloading", null, 180, -2, [], 7.0, false],
	["walk (sway)", null, 180, 0, ["move_forward"], 1.3, true],
	["sprint (gun down)", null, 180, 0, ["move_forward", "sprint"], 1.2, true],
	["stop", null, 180, 0, [], 1.0, false],
	["snap the view round", null, 120, 0, [], 0.05, true],
]


func _ready() -> void:
	super()
	player.get_node("Head/Arms").force_input = _auto


func _moves() -> Array:
	return ARMS_MOVES


func _title() -> String:
	return "Step 3 · arms   (click to grab mouse · RMB aim · LMB fire · R reload · WASD · Shift · Space · Ctrl)"


func _status_extra() -> String:
	return "arms: " + String(player.get_node("Head/Arms").animator.current())


func _shot_prefix() -> String:
	return "step03"
