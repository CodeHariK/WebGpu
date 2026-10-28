## What the enemy wants: wander, notice you, chase, lunge, back off, get hurt, die. Sits on the
## enemy's root node and only gives orders: enemy_body.gd (where to go), limb_gait.gd (lunges,
## staggers) and cultist_rig.gd (where the head looks). Ported from EnemyAIController.
##
##   WANDER   walks to random spots within wander_radius of where it spawned, pausing between
##   CHASE    re-plans to the target whenever it has moved retarget_move metres; once close
##            (attack_start) and the attack timer is ready, lunges
##   ATTACK   mid-lunge: anything within attack_reach is struck (take_hit with attack_force),
##            then it bounces back off you
##   RETREAT  runs back to where the bounce points (retreat metres), then chases again
##   DEAD     limbs drop, the ball rolls to rest, dissolve.gd burns the body away
##
## Noticing you (theirs only reacted to being shot; we add sight and hearing, both cheap):
##   shot     always: whoever shot it becomes the target
##   sight    every sense_interval: within sight_range, inside the cone in front of the torso (or
##            within feel_range, any direction), and one ray says nothing's in the way
##   hearing  hear(at): call it for loud noises (gunshots); within hearing_range
## Once it has a target it never lets go (like theirs).
##
## Health: max_health shots (theirs died to one). Each non-lethal hit staggers it briefly.
##
## Mobile: one ray every sense_interval while it hasn't noticed you, nothing else per frame.
extends Node3D

const EnemyBody := preload("res://src/enemy/enemy_body.gd")
const Gait := preload("res://src/enemy/limb_gait.gd")
const Dissolve := preload("res://src/enemy/dissolve.gd")

enum State { WANDER, CHASE, ATTACK, RETREAT, DEAD }
const STATE_NAMES := ["wander", "chase", "attack", "retreat", "dead"]

signal died

@export var max_health := 3
@export var wander_radius := 20.0
@export var wander_pause := Vector2(1.0, 3.0)  ## seconds, random in this range
@export var sight_range := 18.0
@export var sight_cone := deg_to_rad(60.0)  ## half-angle
@export var feel_range := 5.0  ## noticed this close whichever way it's facing
@export var hearing_range := 30.0
@export var sense_interval := 0.2
@export var retarget_move := 3.0
@export var attack_start := 6.0  ## lunge from this close
@export var attack_reach := 2.0  ## a lunge hits within this
@export var attack_force := 30.0  ## pushed into take_hit (the player scales it)
@export var bounce_back := 12.0  ## impulse on the ball after a hit
@export var retreat := 8.0  ## metres it backs off after a hit
@export var attack_delay := Vector2(1.5, 4.0)  ## seconds between lunges, random in this range
@export var stagger_time := 0.4

var state := State.WANDER
var health := 3
var target: Node3D  ## who it's after (null: hasn't noticed anyone)
var player: Node3D  ## who it can see or hear (set by whoever spawns it)
var home := Vector3.ZERO

var _cooldown := 0.0
var _pause := 0.0
var _sense := 0.0
var _last_target_at := Vector3.INF

@onready var body: EnemyBody = $Body
@onready var gait: Gait = $Gait
@onready var rig: Node3D = get_node_or_null("Rig")


func _ready() -> void:
	health = max_health
	home = body.global_position
	body.hit_taken.connect(_on_hit)
	_sense = randf() * sense_interval


func _physics_process(delta: float) -> void:
	if state == State.DEAD:
		return
	_cooldown = maxf(_cooldown - delta, 0.0)
	_look_around(delta)
	match state:
		State.WANDER: _wander(delta)
		State.CHASE: _chase()
		State.ATTACK: _attack()
		State.RETREAT: _retreat()


## A loud noise at `at` (a gunshot).
func hear(at: Vector3) -> void:
	if state != State.DEAD and target == null and player and at.distance_to(body.global_position) < hearing_range:
		_notice(player)


func state_name() -> String:
	return STATE_NAMES[state]


func _look_around(delta: float) -> void:
	if target != null or player == null:
		return
	_sense -= delta
	if _sense > 0.0:
		return
	_sense = sense_interval
	if _can_see(player):
		_notice(player)


func _can_see(who: Node3D) -> bool:
	var eye := body.global_position
	var at := who.global_position + Vector3.UP
	var to := at - eye
	if to.length() > sight_range:
		return false
	if to.length() > feel_range and (-gait.torso.frame.basis.z).angle_to(to) > sight_cone:
		return false
	var query := PhysicsRayQueryParameters3D.create(eye, at, body.world_mask, [body.get_rid()])
	return body.get_world_3d().direct_space_state.intersect_ray(query).is_empty()


func _notice(who: Node3D) -> void:
	target = who
	_last_target_at = Vector3.INF
	if rig:
		rig.look_target = who.get_node_or_null("Head") if who.has_node("Head") else who
	if state == State.WANDER:
		state = State.CHASE


func _wander(delta: float) -> void:
	if not body.target_reached():
		return
	_pause -= delta
	if _pause > 0.0:
		return
	_pause = randf_range(wander_pause.x, wander_pause.y)
	var angle := randf() * TAU
	body.set_target(home + Vector3(cos(angle), 0.0, sin(angle)) * randf() * wander_radius)


func _chase() -> void:
	var at := target.global_position
	if at.distance_to(_last_target_at) > retarget_move:
		_last_target_at = at
		body.set_target(at)
	var distance := at.distance_to(body.global_position)
	if _cooldown == 0.0 and distance < attack_start and gait.attack(target):
		body.set_target(at)
		state = State.ATTACK
		_cooldown = distance / (body.max_speed * gait.launch_scale) * 2.0  # how long a lunge may take


func _attack() -> void:
	if target.global_position.distance_to(body.global_position) <= attack_reach:
		_strike()
	elif _cooldown == 0.0:
		state = State.CHASE  # missed


## Hit the target, bounce off it and back away.
func _strike() -> void:
	var direction := (target.global_position - body.global_position).normalized()
	if target.has_method("take_hit"):
		target.take_hit(body.global_position, direction * attack_force, body)
	body.apply_impulse(-direction * bounce_back)
	body.set_target(body.global_position - direction * retreat)
	_cooldown = randf_range(attack_delay.x, attack_delay.y)
	state = State.RETREAT


func _retreat() -> void:
	if body.target_reached():
		_last_target_at = Vector3.INF
		state = State.CHASE


func _on_hit(_point: Vector3, _force: Vector3, attacker: Node3D) -> void:
	if state == State.DEAD:
		return
	if target == null and attacker:
		_notice(attacker)
	health -= 1
	if health <= 0:
		_die()
	else:
		gait.stagger(stagger_time)


func _die() -> void:
	state = State.DEAD
	gait.die()
	body.die()
	if rig:
		rig.look_target = null
		rig.set_head_active(false)
		var dissolve := Dissolve.new()
		dissolve.meshes = rig.meshes()
		add_child(dissolve)
	died.emit()
