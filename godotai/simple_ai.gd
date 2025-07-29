extends CharacterBody2D

@export var speed = 100
@export var chase_distance = 200
@export var attack_distance = 50
@export var flee_health_threshold = 20

var player
var health = 100
var wander_timer = 0.0
var wander_direction = Vector2.ZERO

func _ready():
	player = get_node("../Player")

func _physics_process(delta):
	var to_player = player.global_position - global_position
	var distance = to_player.length()
	var direction = Vector2.ZERO

	if health <= flee_health_threshold:
		# Flee logic
		direction = -to_player.normalized()
		velocity = direction * speed * 1.5
		move_and_slide()
		return

	if distance < attack_distance:
		# Attack logic
		print("Attacking player")
		velocity = Vector2.ZERO
		move_and_slide()
		return

	if distance < chase_distance:
		# Chase logic
		direction = to_player.normalized()
		velocity = direction * speed
		move_and_slide()
		return

	# Wander logic
	wander_timer -= delta
	if wander_timer <= 0:
		wander_direction = Vector2(randf() * 2 - 1, randf() * 2 - 1).normalized()
		wander_timer = randf_range(1, 3)

	velocity = wander_direction * speed * 0.5
	move_and_slide()
