extends Node
class_name EnemyManager

# Array of dictionaries/structs: { "enemy_script": EnemyScript, "enemy_availability": bool }
var all_enemies: Array = []

var AI_Loop_Coroutine = null
var alive_enemy_count: int = 0

func _ready() -> void:
	# Add a slight delay to allow all child nodes to initialize
	await get_tree().process_frame
	_initialize_enemies()

func _initialize_enemies() -> void:
	all_enemies.clear()
	var enemy_nodes = get_tree().get_nodes_in_group("enemies")
	for node in enemy_nodes:
		if node.has_method("is_attackable"):
			all_enemies.append({
				"enemy_script": node,
				"enemy_availability": true
			})
	alive_enemy_count = all_enemies.size()
	start_ai()

func start_ai() -> void:
	_ai_loop(null)

func _ai_loop(exclude_enemy) -> void:
	if alive_enemy_count == 0:
		return

	# Random wait time
	await get_tree().create_timer(randf_range(0.5, 1.5)).timeout

	var attacking_enemy = random_enemy_excluding(exclude_enemy)
	if attacking_enemy == null:
		attacking_enemy = random_enemy()

	if attacking_enemy == null:
		return

	# Wait until enemy is ready
	while attacking_enemy.check_retreating() or attacking_enemy.check_locked_target() or attacking_enemy.check_stunned():
		await get_tree().process_frame

	attacking_enemy.set_attack()

	# Wait until they finish preparing attack
	while attacking_enemy.check_preparing_attack():
		await get_tree().process_frame


	attacking_enemy.set_retreat()

	await get_tree().create_timer(randf_range(0.0, 0.5)).timeout

	if alive_enemy_count > 0:
		_ai_loop(attacking_enemy)

func random_enemy():
	var available = []
	for item in all_enemies:
		if item["enemy_availability"]:
			available.append(item["enemy_script"])
	if available.size() == 0:
		return null
	return available[randi() % available.size()]

func random_enemy_excluding(exclude):
	var available = []
	for item in all_enemies:
		if item["enemy_availability"] and item["enemy_script"] != exclude:
			available.append(item["enemy_script"])
	if available.size() == 0:
		return null
	return available[randi() % available.size()]

func an_enemy_is_preparing_attack() -> bool:
	for item in all_enemies:
		var enemy = item["enemy_script"]
		if enemy and enemy.has_method("check_preparing_attack") and enemy.check_preparing_attack():
			return true
	return false

func get_alive_enemy_count() -> int:

	var count = 0
	for item in all_enemies:
		if is_instance_valid(item["enemy_script"]) and item["enemy_script"].is_inside_tree() and item["enemy_script"].health > 0:
			count += 1
	alive_enemy_count = count
	return count

func set_enemy_availability(enemy, state: bool) -> void:
	for item in all_enemies:
		if item["enemy_script"] == enemy:
			item["enemy_availability"] = state
			break
	
	# Update active detection target if it matches the unavailable enemy
	var detection = get_tree().get_first_node_in_group("enemy_detection")
	if detection and detection.current_target == enemy:
		detection.set_current_target(null)

	get_alive_enemy_count()
