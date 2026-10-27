## Step 4 — shooting. The revolver fires hitscan rounds: muzzle flash, bullet holes, dust and
## sparks, crates that get knocked about, recoil, six rounds and a reload that drops the brass.
##   click to grab the mouse · left mouse fire · right mouse (hold) aim · R reload · WASD etc.
##
## Test run: Godot --path . res://demos/step04_shoot.tscn -- --shot  → shots/step04_*.png
##   fires from the hip and aimed at the plank wall and the crates, empties the gun, pulls the
##   trigger on empty (auto reload), and looks at the holes and the brass on the floor
extends "res://demos/step03_arms.gd"

const WeaponHud := preload("res://src/ui/weapon_hud.gd")

## [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const SHOOT_MOVES := [
	["face the targets", Vector3(-6, 0.05, 9), 0, -6, [], 1.0, true],
	["hip shot 1", null, 0, -6, ["fire"], 0.03, true],
	["settle", null, 0, -6, [], 0.5, false],
	["hip shot 2", null, 3, -4, ["fire"], 0.5, false],
	["raise", null, 3, -4, ["aim"], 1.2, false],
	["aimed shot at a crate", null, 8, -12, ["aim", "fire"], 0.03, true],
	["settle", null, 8, -12, ["aim"], 0.5, false],
	["aimed shot 2", null, -6, -10, ["aim", "fire"], 0.5, false],
	["aimed shot 3", null, -2, -2, ["aim", "fire"], 0.5, false],
	["aimed shot 4", null, 2, 0, ["aim", "fire"], 0.6, true],
	["lower", null, 0, -6, [], 1.0, false],
	["trigger on empty", null, 0, -6, ["fire"], 2.0, true],
	["reloading", null, 0, -6, [], 8.0, false],
	["look at the brass", null, 0, -85, [], 1.0, true],
	["walk up to the holes", Vector3(-6, 0.05, 5.2), 0, -8, [], 1.0, true],
]

var hud := WeaponHud.new()


func _ready() -> void:
	super()
	add_child(hud)


func _process(delta: float) -> void:
	super(delta)
	var revolver = player.get_node("Head/Arms").revolver
	hud.show_state(revolver.rounds, revolver.aiming(), revolver.reloading)


func _moves() -> Array:
	return SHOOT_MOVES


func _title() -> String:
	return "Step 4 · shoot   (click to grab mouse · LMB fire · RMB aim · R reload · WASD · Shift · Space · Ctrl)"


func _status_extra() -> String:
	var revolver = player.get_node("Head/Arms").revolver
	return "%s · rounds %d/%d%s" % [super(), revolver.rounds, revolver.capacity, " · reloading" if revolver.reloading else ""]


func _shot_prefix() -> String:
	return "step04"
