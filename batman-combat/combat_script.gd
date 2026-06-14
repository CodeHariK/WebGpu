extends Node
class_name CombatScript

signal on_trajectory(target)
signal on_hit(target)
signal on_counter_attack(target)

@export var attack_cooldown: float = 0.8
@export var enemy_manager_path: NodePath
@export var enemy_detection_path: NodePath

var enemy_manager: EnemyManager
var enemy_detection: EnemyDetection
var animation_player: AnimationPlayer
var character: RigidBody3D

var is_attacking_enemy: bool = false
var is_countering: bool = false

var locked_target = null
var animation_count: int = 0
var attacks: Array = ["AnimKicking", "AnimFlyingKick", "AnimFlyingKneePunchCombo", "AnimPunchingBag"]

func _ready() -> void:
	character = get_parent()
	# Search for components
	if enemy_detection_path:
		enemy_detection = get_node(enemy_detection_path)
	else:
		enemy_detection = character.get_node_or_null("EnemyDetection")
		
	animation_player = character.get_node_or_null("CollisionShape3D/Batman/AnimationPlayer")

func get_enemy_manager() -> EnemyManager:
	if not enemy_manager:
		if enemy_manager_path:
			enemy_manager = get_node(enemy_manager_path)
		else:
			enemy_manager = get_tree().get_first_node_in_group("enemy_manager")
	return enemy_manager


func attack_check() -> void:
	print("[Combat] attack_check called. is_attacking_enemy: %s, is_countering: %s" % [is_attacking_enemy, is_countering])
	if is_attacking_enemy or is_countering:
		return

	var manager = get_enemy_manager()
	if not manager:
		print("[Combat] enemy_manager is null, attacking thin air.")
		attack(null, 0.0)
		return

	print("[Combat] Alive enemies count: %d" % manager.get_alive_enemy_count())
	
	if not enemy_detection or enemy_detection.current_target == null:
		if manager.get_alive_enemy_count() == 0:
			print("[Combat] No alive enemies, attacking thin air.")
			attack(null, 0.0)
			return
		else:
			locked_target = manager.random_enemy()
			print("[Combat] Picked random enemy target: %s" % str(locked_target))
	else:
		locked_target = enemy_detection.current_target
		print("[Combat] Picked detected enemy target: %s" % str(locked_target))

	if enemy_detection and enemy_detection.get_input_magnitude() > 0.2:
		locked_target = enemy_detection.current_target
		print("[Combat] Overrode target with input direction detection: %s" % str(locked_target))


	if locked_target == null:
		locked_target = manager.random_enemy()
		print("[Combat] Fallback random target: %s" % str(locked_target))

	if locked_target:
		var dist = target_distance(locked_target)
		print("[Combat] Launching attack towards target: %s (distance: %.2f)" % [locked_target.name, dist])
		attack(locked_target, dist)
	else:
		print("[Combat] No targets found, attacking thin air.")
		attack(null, 0.0)



func attack(target, distance: float) -> void:
	print("[Combat] attack called. Target: %s, Distance: %.2f" % [str(target), distance])
	if target == null:
		print("[Combat] target is null. Playing AnimPunchingBag.")
		attack_type("AnimPunchingBag", 0.4, null, 0.0)
		return

	if distance < 15.0:
		animation_count = (animation_count + 1) % attacks.size()
		var attack_string = attacks[animation_count]
		print("[Combat] Target distance validation passed. Playing combo animation: %s" % attack_string)
		attack_type(attack_string, attack_cooldown, target, 0.45)
	else:
		print("[Combat] Distance %.2f >= 15.0. Target too far, attacking thin air." % distance)
		locked_target = null
		attack_type("AnimPunchingBag", 0.4, null, 0.0)

func attack_type(attack_anim: String, cooldown: float, target, movement_duration: float) -> void:
	print("[Combat] attack_type called. Animation: %s, Cooldown: %.2f, Target: %s" % [attack_anim, cooldown, str(target)])
	var tree = character.get_node_or_null("CollisionShape3D/Batman/AnimationTree")
	if tree and tree.active:
		var pb = tree.get("parameters/playback")
		if pb:
			pb.travel(attack_anim)
	elif animation_player:
		animation_player.play(attack_anim)

	is_attacking_enemy = true
	
	if target:
		target.stop_moving()
		move_towards_target(target, movement_duration)

	await get_tree().create_timer(cooldown).timeout
	is_attacking_enemy = false

func move_towards_target(target, duration: float) -> void:
	on_trajectory.emit(target)
	
	# Rotate to face target
	var look_target = Vector3(target.global_position.x, character.global_position.y, target.global_position.z)
	var char_mesh = character.get_node_or_null("CollisionShape3D/Batman")
	if char_mesh:
		char_mesh.look_at(look_target, Vector3.UP)
		char_mesh.rotate_y(PI) # Flip if mesh base orientation is backwards
	
	# Smoothly tween player position to the target offset
	var offset_position = target_offset(target.global_position)
	var tween = character.create_tween()
	tween.tween_property(character, "global_position", offset_position, duration).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	
	# Execute hit event near end of leap
	await get_tree().create_timer(duration * 0.85).timeout
	hit_event()

func target_offset(target_pos: Vector3) -> Vector3:
	return target_pos + (character.global_position - target_pos).normalized() * 1.5

func target_distance(target) -> float:
	return character.global_position.distance_to(target.global_position)

func counter_check() -> void:
	var manager = get_enemy_manager()
	if not manager:
		return
	if is_countering or is_attacking_enemy or not manager.has_method("an_enemy_is_preparing_attack") or not manager.an_enemy_is_preparing_attack():
		return

	locked_target = closest_counter_enemy()
	if locked_target == null:
		return
		
	on_counter_attack.emit(locked_target)

	if target_distance(locked_target) > 3.0:
		attack(locked_target, target_distance(locked_target))
		return

	var duration = 0.25
	var tree = character.get_node_or_null("CollisionShape3D/Batman/AnimationTree")
	if tree and tree.active:
		var pb = tree.get("parameters/playback")
		if pb:
			pb.travel("AnimDodging")
	elif animation_player:
		animation_player.play("AnimDodging")
	
	# Turn to face attacker
	var char_mesh = character.get_node_or_null("CollisionShape3D/Batman")
	if char_mesh:
		char_mesh.look_at(Vector3(locked_target.global_position.x, character.global_position.y, locked_target.global_position.z), Vector3.UP)
		char_mesh.rotate_y(PI)

	is_countering = true
	var tween = character.create_tween()
	# Dodge slightly past them or to their side
	var dodge_pos = character.global_position + (locked_target.global_position - character.global_position).normalized().rotated(Vector3.UP, PI/4) * 1.2
	tween.tween_property(character, "global_position", dodge_pos, duration)
	
	await get_tree().create_timer(duration).timeout
	attack(locked_target, target_distance(locked_target))
	is_countering = false

func closest_counter_enemy():
	var manager = get_enemy_manager()
	if not manager:
		return null
	var min_distance = 999.0
	var closest = null
	for item in manager.all_enemies:
		var enemy = item["enemy_script"]
		if enemy and enemy.has_method("check_preparing_attack") and enemy.check_preparing_attack():
			var d = target_distance(enemy)
			if d < min_distance:
				min_distance = d
				closest = enemy
	return closest


func hit_event() -> void:
	var manager = get_enemy_manager()
	if not manager or locked_target == null or manager.get_alive_enemy_count() == 0:
		return
	on_hit.emit(locked_target)


func damage_event() -> void:
	var tree = character.get_node_or_null("CollisionShape3D/Batman/AnimationTree")
	if tree and tree.active:
		var pb = tree.get("parameters/playback")
		if pb:
			pb.travel("AnimHeadHit")
	elif animation_player:
		animation_player.play("AnimHeadHit")

	is_attacking_enemy = true
	await get_tree().create_timer(0.5).timeout
	is_attacking_enemy = false
