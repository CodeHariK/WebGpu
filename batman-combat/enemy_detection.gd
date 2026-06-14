extends Node3D
class_name EnemyDetection

@export var layer_mask: int = 1 # Match target collision layers
var current_targetNode = null
var current_target = null

var input_direction: Vector3 = Vector3.ZERO
var character: RigidBody3D = null

func _ready() -> void:
	add_to_group("enemy_detection")
	character = get_parent()

func _physics_process(delta: float) -> void:
	# Calculate input direction relative to the Camera
	var camera = get_viewport().get_camera_3d()
	if not camera:
		return
		
	var cam_forward = -camera.global_transform.basis.z
	var cam_right = camera.global_transform.basis.x
	cam_forward.y = 0.0
	cam_right.y = 0.0
	cam_forward = cam_forward.normalized()
	cam_right = cam_right.normalized()
	
	var input_dir = Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	input_direction = cam_forward * -input_dir.y + cam_right * input_dir.x
	if input_direction.length() > 0.01:
		input_direction = input_direction.normalized()

	
	if input_direction.length() > 0.1:
		# Perform a sphere cast equivalent in Godot using PhysicsDirectSpaceState3D
		var space_state = get_world_3d().direct_space_state
		var query = PhysicsShapeQueryParameters3D.new()
		
		# Create a sphere shape with radius 3.0
		var sphere = SphereShape3D.new()
		sphere.radius = 3.0
		query.shape = sphere
		
		# Cast from the player's position in inputDirection up to 10 meters
		query.transform = global_transform
		query.motion = input_direction * 10.0
		query.collision_mask = layer_mask
		
		var result = space_state.cast_motion(query)
		if result.size() > 0:
			# Find what we hit at the contact fraction
			var target_pos = global_position + input_direction * 10.0 * result[0]
			var check_query = PhysicsShapeQueryParameters3D.new()
			check_query.shape = sphere
			check_query.transform = Transform3D(Basis(), target_pos)
			check_query.collision_mask = layer_mask
			
			var hits = space_state.intersect_shape(check_query)

			for hit in hits:
				var collider = hit.collider
				if collider and collider.has_method("is_attackable") and collider.is_attackable():
					current_target = collider
					return
		
		# Fallback: simple raycast sweep
		var ray_query = PhysicsRayQueryParameters3D.create(global_position, global_position + input_direction * 10.0, layer_mask)
		var ray_hit = space_state.intersect_ray(ray_query)
		if ray_hit.size() > 0:
			var collider = ray_hit.collider
			if collider and collider.has_method("is_attackable") and collider.is_attackable():
				current_target = collider

func get_input_magnitude() -> float:
	return input_direction.length()

func set_current_target(target) -> void:
	current_target = target
