## Step 7 — limb IK: the cultist crawls after you on its hands and feet. Same ball and gait as
## step 6; now the model hangs on it (src/enemy/cultist_rig.gd), limbs step in Bezier arcs
## (limb.gd), flail in the air, and the head turns to look at you.
##   B show the ball · P route lines · L lamps · F flashlight · LMB fire …
##
## Test run: Godot --path . res://demos/step07_cultist.tscn -- --shot  → shots/step07_*.png
##   the camera follows it across the hall, from the side, into the mine, and shoots it
extends "res://demos/step06_enemy.gd"

const CULTIST := preload("res://scenes/cultist.tscn")

## [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const CULTIST_MOVES := [
	["watch it wake up", Vector3(0, 0.05, 6), 0, 0, [], 1.0, true],
	["watch it crawl at you", null, 0, 0, [], 2.5, true],
	["watch it arrive", null, 0, 0, [], 2.5, true],
	["step aside", Vector3(8, 0.05, 4), 0, 0, [], 0.4, false],
	["watch it turn and come", null, 0, 0, [], 0.8, true],
	["watch it from the side", null, 0, 0, [], 0.6, true],
	["go into the mine", Vector3(-1, 0.05, 30), 0, 0, [], 0.5, false],
	["watch it in the doorway", null, 0, 0, [], 3.5, true],
	["watch it in the mine", null, 0, 0, [], 4.0, true],
	["watch: shoot it", null, 0, 0, ["fire"], 0.03, true],
	["watch it get knocked", null, 0, 0, [], 0.5, true],
]

var rig: Node3D


func _ready() -> void:
	super()
	debug.visible = false
	rig = enemy.get_node("Rig")
	rig.look_target = player.get_node("Head")


func _enemy_scene() -> PackedScene:
	return CULTIST


func _unhandled_input(event: InputEvent) -> void:
	super(event)
	if event is InputEventKey and event.pressed and event.physical_keycode == KEY_B:
		var ball: Node3D = body.get_node("Mesh")
		ball.visible = not ball.visible


func _moves() -> Array:
	return CULTIST_MOVES


func _title() -> String:
	return "Step 7 · cultist   (B ball · P route · L lamps · F flashlight · LMB fire · RMB aim · R reload · WASD)"


func _shot_prefix() -> String:
	return "step07"
