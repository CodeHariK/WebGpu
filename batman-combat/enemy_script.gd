extends CharacterBody3D
class_name EnemyScript

@export var health: int = 3
@export var move_speed: float = 1.5

var player_combat: CombatScript
var enemy_manager: EnemyManager
var animation_player: AnimationPlayer

var is_preparing_attack: bool = false
var is_moving: bool = false
var is_retreating: bool = false
var is_locked_target: bool = false
var is_stunned: bool = false
var is_waiting: bool = true

var move_direction: Vector3 = Vector3.ZERO
var movement_timer: Timer

func _ready() -> void:
	add_to_group("enemies")
	
	# Find managers/player
	enemy_manager = get_tree().get_first_node_in_group("enemy_manager")
	player_combat = get_tree().get_first_node_in_group("player_combat")
	animation_player = $Batman/AnimationPlayer # Check node path structure
	
	if not animation_player:
		animation_player = find_child("AnimationPlayer")

	# Register event handlers
	if player_combat:
		player_combat.on_hit.connect(_on_player_hit)
		player_combat.on_counter_attack.connect(_on_player_counter)
		player_combat.on_trajectory.connect(_on_player_trajectory)
	
	# Start movement cycle
	_start_movement_cycle()

func _start_movement_cycle() -> void:
	while true:
		if is_waiting:
			var chance = randi() % 2
			if chance == 1:
				var dir_chance = randi() % 2
				move_direction = Vector3.RIGHT if dir_chance == 1 else Vector3.LEFT
				is_moving = true
			else:
				stop_moving()
		
		await get_tree().create_timer(1.0).timeout

func _physics_process(delta: float) -> void:
	if health <= 0:
		return

	if player_combat:
		# Constantly face the player
		var look_target = Vector3(player_combat.character.global_position.x, global_position.y, player_combat.character.global_position.z)
		look_at(look_target, Vector3.UP)
		rotate_y(PI) # Flip base model rotation if needed
	
	_move_enemy(delta)

func _move_enemy(delta: float) -> void:
	if not is_moving or not player_combat:
		velocity = Vector3.ZERO
		move_and_slide()
		return
		
	var speed = move_speed
	if move_direction == Vector3.FORWARD:
		speed = 5.0
	elif move_direction == Vector3.BACK:
		speed = 2.0
		
	# Update animation speed parameters (blend tree equivalents)
	var tree = get_node_or_null("Batman/AnimationTree")
	if tree and tree.active:
		var pb = tree.get("parameters/playback")
		if pb:
			if is_preparing_attack:
				pb.travel("AnimEnemyIdle")
			else:
				pb.travel("AnimWalk")
	elif animation_player:
		if is_preparing_attack:
			animation_player.play("AnimEnemyIdle")
		else:
			animation_player.play("AnimWalk")
			
	var to_player = (player_combat.character.global_position - global_position).normalized()
	var perp_dir = to_player.rotated(Vector3.UP, PI / 2.0)
	
	var final_dir = Vector3.ZERO
	if move_direction == Vector3.FORWARD:
		final_dir = to_player
	elif move_direction == Vector3.RIGHT:
		final_dir = perp_dir
	elif move_direction == Vector3.LEFT:
		final_dir = -perp_dir
	elif move_direction == Vector3.BACK:
		final_dir = -to_player
		
	velocity = final_dir * speed
	move_and_slide()

	if is_preparing_attack and global_position.distance_to(player_combat.character.global_position) < 2.2:
		stop_moving()
		if not player_combat.is_countering and not player_combat.is_attacking_enemy:
			_execute_attack()
		else:
			_set_prepare_attack(false)

func _execute_attack() -> void:
	var tree = get_node_or_null("Batman/AnimationTree")
	if tree and tree.active:
		var pb = tree.get("parameters/playback")
		if pb:
			pb.travel("AnimPunchingBag")
	elif animation_player:
		animation_player.play("AnimPunchingBag") # Enemy attack animation
	
	# Leash slightly forward
	var look_dir = -global_transform.basis.z
	var tween = create_tween()
	tween.tween_property(self, "global_position", global_position + look_dir * 1.0, 0.4)
	
	await get_tree().create_timer(0.35).timeout
	# Hit frame check
	if player_combat and global_position.distance_to(player_combat.character.global_position) < 2.5:
		if not player_combat.is_countering and not player_combat.is_attacking_enemy:
			player_combat.damage_event()
	_set_prepare_attack(false)

func set_attack() -> void:
	is_waiting = false
	_set_prepare_attack(true)
	await get_tree().create_timer(0.25).timeout
	move_direction = Vector3.FORWARD
	is_moving = true

func set_retreat() -> void:
	_set_prepare_attack(false)
	await get_tree().create_timer(1.2).timeout
	is_retreating = true
	move_direction = Vector3.BACK
	is_moving = true
	
	# Retreat until safe distance
	while player_combat and global_position.distance_to(player_combat.character.global_position) < 5.0:
		await get_tree().process_frame
		
	is_retreating = false
	stop_moving()
	is_waiting = true

func _set_prepare_attack(active: bool) -> void:
	is_preparing_attack = active
	if not active:
		stop_moving()

func stop_moving() -> void:
	is_moving = false
	move_direction = Vector3.ZERO
	velocity = Vector3.ZERO

# Player combat listeners
func _on_player_hit(target) -> void:
	if target == self:
		is_locked_target = false
		health -= 1
		
		if health <= 0:
			_death()
			return
			
		is_stunned = true
		var tree = get_node_or_null("Batman/AnimationTree")
		if tree and tree.active:
			var pb = tree.get("parameters/playback")
			if pb:
				pb.travel("AnimHeadHit")
		elif animation_player:
			animation_player.play("AnimHeadHit")
			
		# Knockback
		var tween = create_tween()
		tween.tween_property(self, "global_position", global_position - global_transform.basis.z * 0.8, 0.25)
		
		stop_moving()
		await get_tree().create_timer(0.5).timeout
		is_stunned = false

func _on_player_counter(target) -> void:
	if target == self:
		_set_prepare_attack(false)

func _on_player_trajectory(target) -> void:
	if target == self:
		is_locked_target = true
		_set_prepare_attack(false)
		stop_moving()

func _death() -> void:
	# Hide/disable physics & trigger animation
	stop_moving()
	var tree = get_node_or_null("Batman/AnimationTree")
	if tree and tree.active:
		var pb = tree.get("parameters/playback")
		if pb:
			pb.travel("AnimFlyingBackDeath")
	elif animation_player:
		animation_player.play("AnimFlyingBackDeath")
	set_physics_process(false)
	$CollisionShape3D.disabled = true
	if enemy_manager:
		enemy_manager.set_enemy_availability(self, false)



func is_attackable() -> bool:
	return health > 0

func check_preparing_attack() -> bool:
	return is_preparing_attack

func check_retreating() -> bool:
	return is_retreating

func check_locked_target() -> bool:
	return is_locked_target

func check_stunned() -> bool:
	return is_stunned

