## Step 8 — enemy AI: the cultist wanders until it sees or hears you, then hunts you down and
## lunges. Three hits from it and you're dead (you respawn); three shots and it burns away.
##   LMB fire (it hears gunshots) · RMB aim · R reload · F flashlight · L lamps · P route · B ball
##
## The enemy's brain is src/enemy/enemy_ai.gd; this demo only hands it the player, passes it
## gunshots (hear) and handles the player's death (a "you died" card, then a respawn).
##
## Test run: Godot --path . res://demos/step08_ai.tscn -- --shot  → shots/step08_*.png
##   watches it wander, fires to get its attention, lets it attack, then shoots it until it dies
extends "res://demos/step07_cultist.gd"

const DamageOverlay := preload("res://src/ui/damage_overlay.gd")
const RESPAWN_DELAY := 3.0

## [name, teleport (or null), yaw°, pitch°, held actions, seconds, screenshot at the end]
const AI_MOVES := [
	["watch it wander", Vector3(-8, 0.05, 8), 0, 0, [], 2.0, true],
	["watch: fire to get its attention", null, 0, 0, ["fire"], 0.03, false],
	["watch it come", null, 0, 0, [], 3.5, true],
	["watch it attack", null, 0, 0, [], 3.0, true],
	["watch: shoot it", null, 0, 0, ["fire"], 0.03, true],
	["watch", null, 0, 0, [], 0.5, false],
	["watch: shoot it again", null, 0, 0, ["fire"], 0.03, false],
	["watch", null, 0, 0, [], 0.5, false],
	["watch: and again", null, 0, 0, ["fire"], 0.03, true],
	["watch", null, 0, 0, [], 0.5, false],
	["watch: once more if it lives", null, 0, 0, ["fire"], 0.03, false],
	["watch it burn", null, 0, 0, [], 1.5, true],
	["watch it burn away", null, 0, 0, [], 1.5, true],
]

var ai: Node3D
var health: Node
var overlay := DamageOverlay.new()
var _respawn := 0.0


func _ready() -> void:
	super()
	ai = enemy
	ai.set_physics_process(true)
	ai.player = player
	rig.look_target = null  # it looks at you once it notices you
	health = player.get_node("Health")
	health.died.connect(_on_player_died)
	overlay.health = health
	add_child(overlay)
	player.get_node("Head/Arms").revolver.fired.connect(_on_gunshot)


## The AI drives the enemy now (step 6's demo chase is off).
func _physics_process(_delta: float) -> void:
	pass


func _process(delta: float) -> void:
	super(delta)
	if _respawn > 0.0:
		_respawn -= delta
		if _respawn <= 0.0:
			_revive()


func _look_at_enemy() -> void:
	if is_instance_valid(enemy):
		super()


func _on_gunshot(origin: Vector3, _direction: Vector3) -> void:
	if is_instance_valid(ai):
		ai.hear(origin)


func _on_player_died() -> void:
	player.controls_enabled = false
	_respawn = RESPAWN_DELAY


func _revive() -> void:
	health.revive()
	player.global_position = Level.SPAWN
	player.velocity = Vector3.ZERO
	player.controls_enabled = true


func _moves() -> Array:
	return AI_MOVES


func _title() -> String:
	return "Step 8 · enemy AI   (LMB fire · RMB aim · R reload · F flashlight · L lamps · P route · B ball)"


func _status_extra() -> String:
	var you := "YOU DIED" if health and health.dead else "health %.1f" % (health.health if health else 0.0)
	if not is_instance_valid(ai):
		return "%s · enemy gone" % you
	return "%s · enemy: %s, health %d, %s · %.1f m away" % [you, ai.state_name(), ai.health,
		GROUNDED[body.grounded], body.global_position.distance_to(player.global_position)]


func _shot_prefix() -> String:
	return "step08"
