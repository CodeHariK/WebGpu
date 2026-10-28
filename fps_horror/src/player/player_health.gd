## The player's health: 3 hits and you're dead; heals back slowly after a quiet spell. Ported from
## PlayerHealthController in the original. Sits under the Player; listens to its hit_taken.
##
##   hit       -1 health, a shove (the attacker's force × knock_scale, plus a little up), a
##             camera shake, a red flash, and the heal timer restarts
##   heal      after heal_delay seconds without a hit, heal_rate health per second
##   overlay   a number for damage_overlay.gd: 0 at full health, up to 3 near death, plus up to 1
##             of flash that fades after each hit (their EffectStrength). It jumps up at once and
##             fades down slowly.
##   death     `died` once health hits 0; revive() puts everything back
extends Node

signal died

@export var max_health := 3.0
@export var heal_delay := 10.0
@export var heal_rate := 0.5  ## health per second
@export var flash_fade := 2.0  ## per second
@export var knock_scale := 0.3  ## their 30 N lunge becomes a 9 m/s shove

var health := 3.0
var overlay := 0.0
var dead := false

var _heal_wait := 0.0
var _flash := 0.0

@onready var _body: CharacterBody3D = get_parent()
@onready var _camera: Node = get_parent().get_node("Head/Camera3D")


func _ready() -> void:
	health = max_health
	_body.hit_taken.connect(_on_hit)


func revive() -> void:
	health = max_health
	dead = false
	overlay = 0.0
	_flash = 0.0


func _on_hit(_point: Vector3, force: Vector3, _attacker: Node3D) -> void:
	if dead:
		return
	health = clampf(health - 1.0, 0.0, max_health)
	_flash = 1.0
	_heal_wait = heal_delay
	_body.velocity += force * knock_scale + Vector3.UP * 2.0
	if _camera.has_method("shake"):
		_camera.shake(2.0)
	if health <= 0.0:
		dead = true
		died.emit()


func _process(delta: float) -> void:
	if not dead and health < max_health:
		_heal_wait -= delta
		if _heal_wait <= 0.0:
			health = minf(health + heal_rate * delta, max_health)
	_flash = maxf(_flash - flash_fade * delta, 0.0)
	var goal := remap(health / max_health, 0.0, 1.0, 3.0, 0.0) + _flash
	overlay = goal if goal > overlay else maxf(overlay - flash_fade * delta, goal)
