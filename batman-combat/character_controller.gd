extends RigidBody3D

@onready var animation_player: AnimationPlayer = $CollisionShape3D/Batman/AnimationPlayer
var combat_script: CombatScript = null
var enemy_detection: EnemyDetection = null

func _ready() -> void:
	# Keep the RigidBody3D from sleeping and rotate freely if needed, 
	# but lock rotation to keep player upright:
	lock_rotation = true
	add_to_group("player")
	if animation_player:
		animation_player.play("AnimPlayerIdle")
		
	# Instantiate CombatScript dynamically
	combat_script = CombatScript.new()
	add_child(combat_script)
	
	# Instantiate EnemyDetection dynamically
	enemy_detection = EnemyDetection.new()
	add_child(enemy_detection)
	
	# Connect internal component bindings
	combat_script.enemy_detection = enemy_detection



func _input(event: InputEvent) -> void:
	if event.is_action_pressed("attack"):
		print("[Input] Attack action triggered via J/LMB")
		if combat_script:
			combat_script.attack_check()
			
	if event.is_action_pressed("counter"):
		print("[Input] Counter action triggered via K/RMB")
		if combat_script:
			combat_script.counter_check()


func _physics_process(delta: float) -> void:
	# Avoid applying default movement physics while active in combat sequences
	if combat_script and (combat_script.is_attacking_enemy or combat_script.is_countering):
		linear_velocity = Vector3.ZERO
		return

	var input_dir = Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var camera = get_viewport().get_camera_3d()
	var direction = Vector3.ZERO
	
	if camera:
		var cam_forward = -camera.global_transform.basis.z
		var cam_right = camera.global_transform.basis.x
		cam_forward.y = 0.0
		cam_right.y = 0.0
		direction = (cam_forward * -input_dir.y + cam_right * input_dir.x)
		if direction.length() > 0.01:
			direction = direction.normalized()
	else:
		direction = Vector3(input_dir.x, 0, input_dir.y).normalized()

	
	if direction.length() > 0:
		linear_velocity.x = direction.x * 5.0
		linear_velocity.z = direction.z * 5.0
		
		# Rotate character mesh to face movement direction
		var char_mesh = $CollisionShape3D/Batman
		if char_mesh:
			var look_target = char_mesh.global_position + direction
			look_target.y = char_mesh.global_position.y
			char_mesh.look_at(look_target, Vector3.UP)
			char_mesh.rotate_y(PI) # Align base orientation

		
		var tree = get_node_or_null("CollisionShape3D/Batman/AnimationTree")
		if tree and tree.active:
			var pb = tree.get("parameters/playback")
			if pb and pb.get_current_node() != "AnimRun":
				pb.travel("AnimRun")
		elif animation_player and animation_player.current_animation != "AnimRun":
			animation_player.play("AnimRun")
	else:
		linear_velocity.x = 0
		linear_velocity.z = 0
		var tree = get_node_or_null("CollisionShape3D/Batman/AnimationTree")
		if tree and tree.active:
			var pb = tree.get("parameters/playback")
			if pb and pb.get_current_node() != "AnimPlayerIdle":
				pb.travel("AnimPlayerIdle")
		elif animation_player and animation_player.current_animation != "AnimPlayerIdle":
			animation_player.play("AnimPlayerIdle")
