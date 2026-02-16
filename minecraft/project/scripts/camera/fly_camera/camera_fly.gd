extends Node3D

enum CameraType {FLY, TARGET}
@export var camera_type: CameraType = CameraType.FLY

@export_group("FLY Camera")
@export var pan_speed = .05
@export var zoom_speed = .4
@export var orbit_speed = .8
@export var min_pitch_angle := -89.0
@export var max_pitch_angle := 89.0

@export_group("Target Camera")
@export var target: Node3D = null
@export var camera_offset: Vector3 = Vector3(0, 4, 0)

@onready var camera_x_pivot: Node3D = $"."
@onready var camera_spring: SpringArm3D = $SpringArm3D
@onready var camera: Camera3D = $SpringArm3D/Camera

# Collision & occlusion tuning
@export_group("Collision & Occlusion")
@export var enable_collision := true
@export var collision_mask := 1
@export var camera_collision_margin := 0.3
@export var collision_smooth_speed := 12.0
@export var min_distance := 0.5
@export var max_distance := 50.0

# Player fade when target obstructs view
@export var fade_on_occlusion := true
@export var fade_alpha := 0.25
@export var fade_smooth_speed := 8.0

# Internal state
var desired_distance: float = 0.0
var current_distance: float = 0.0
var _desired_fade_alpha := 1.0
var _current_fade_alpha := 1.0
var _fade_meshes: Array = []
var _fade_orig_colors: Dictionary = {} # mesh_id -> original albedo Color
var _fade_orig_materials: Dictionary = {} # mesh_id -> original material_override (or null)
var _cached_target: Node = null

func _ready():
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	# Initialize distances and fade state
	current_distance = (camera.global_transform.origin - camera_x_pivot.global_transform.origin).length()
	desired_distance = current_distance
	_current_fade_alpha = 1.0
	_desired_fade_alpha = 1.0
	_collect_fade_meshes()
	#camera.position = Vector3(0, 0, camera_distance)
	# Initialize rotation
	#camera.rotation.x = 0
	#camera_x_pivot.rotation.y = 0

func _input(event):
	if camera_type == CameraType.FLY:
		process_fly_camera(event)
	if camera_type == CameraType.TARGET:
		process_target_camera(event)

func process_fly_camera(event) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			# Zoom in - move pivot forward along its basis Z (negative direction)
			camera_x_pivot.translate(-camera.transform.basis.z * zoom_speed)
			
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			# Zoom out - move pivot backward along its basis Z (positive direction)
			camera_x_pivot.translate(camera.transform.basis.z * zoom_speed)
		
	elif event is InputEventMouseMotion:
		var mm: InputEventMouseMotion = event
		if mm.button_mask == MOUSE_BUTTON_MASK_MIDDLE:
			var dx: float = mm.relative.x if mm.relative.x != null else 0.0
			var dy: float = mm.relative.y if mm.relative.y != null else 0.0
			# safe local orbit speed
			var os: float = float(orbit_speed) if orbit_speed != null else 0.8
			if Input.is_key_pressed(KEY_SHIFT):
				# Pan (middle mouse + shift)
				var pan: Vector3 = camera.global_transform.basis * Vector3(
					- dx * pan_speed,
					dy * pan_speed,
					0
				)
				camera_x_pivot.position += pan
			else:
				# Orbit (middle mouse)
				# Rotate around Y-axis relative to camera's current position
				camera_x_pivot.rotate_y(-dx * os * 0.01)
				
				# Adjust pitch on self
				var new_pitch: float = camera.rotation.x - dy * os * 0.01
				camera.rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))
func _process(delta: float) -> void:
	# Update pivot when following a target and ensure fade meshes are cached
	if camera_type == CameraType.TARGET and is_instance_valid(target) and target is Node3D:
		var t3d: Node3D = target as Node3D
		if t3d != _cached_target:
			_cached_target = t3d
			_collect_fade_meshes()
		var target_pos: Vector3 = t3d.global_position
		var offset_vec: Vector3 = camera_offset if camera_offset is Vector3 else Vector3.ZERO
		camera_x_pivot.global_position = target_pos + offset_vec
		camera.look_at(target_pos)

func process_target_camera(event) -> void:
	if event is InputEventMouseMotion:
		var mm: InputEventMouseMotion = event
		var rel: Vector2 = mm.relative if mm.relative is Vector2 else Vector2.ZERO
		if mm.button_mask == MOUSE_BUTTON_MASK_MIDDLE:
			# Orbit around the target
			# safe numeric deltas
			var dx: float = rel.x if rel.x != null else 0.0
			var dy: float = rel.y if rel.y != null else 0.0
			var os: float = float(orbit_speed) if orbit_speed != null else 0.8
			# Rotate the pivot for yaw (horizontal movement)
			camera_x_pivot.rotate_y(-dx * os * 0.01)
			
			# Rotate the spring arm for pitch (vertical movement)
			if is_instance_valid(camera_spring):
				var new_pitch: float = camera_spring.rotation.x - dy * os * 0.01
				camera_spring.rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))
# --- Collision & occlusion (physics) ---
func _physics_process(delta: float) -> void:
	if not enable_collision:
		return

	var pivot_pos := camera_x_pivot.global_position
	var desired_cam_pos := camera.global_transform.origin
	var dir := desired_cam_pos - pivot_pos
	var desired_len := dir.length()
	if desired_len <= 0.001:
		return
	dir = dir / desired_len

	var space := get_world_3d().direct_space_state
	# First hit along the ray (use PhysicsRayQueryParameters3D for Godot 4.x)
	var params: PhysicsRayQueryParameters3D = PhysicsRayQueryParameters3D.create(pivot_pos, desired_cam_pos)
	params.collision_mask = collision_mask
	var first_hit: Dictionary = space.intersect_ray(params)
	var hit_target: bool = false
	var env_hit: Dictionary = {}
	if first_hit.size() > 0:
		var collider: Object = first_hit.get("collider", null)
		if is_instance_valid(target) and _is_descendant_of(collider, target):
			hit_target = true
		else:
			env_hit = first_hit

	# If environment hit exists, shorten camera; otherwise use full desired length
	if env_hit.size() > 0:
		var hit_pos: Vector3 = env_hit.get("position", pivot_pos)
		desired_distance = max(min_distance, (hit_pos - pivot_pos).length() - camera_collision_margin)
	else:
		desired_distance = clamp(desired_len, min_distance, max_distance)

	# Smooth the distance
	var t := 1.0 - exp(-collision_smooth_speed * delta)
	current_distance = lerp(current_distance, desired_distance, t)

	# Apply camera global position along the same direction from pivot
	var new_cam_pos := pivot_pos + dir * current_distance
	camera.global_transform = Transform3D(camera.global_transform.basis, new_cam_pos)

	# Handle fading of the target when it's between pivot and camera
	if fade_on_occlusion and is_instance_valid(target):
		_desired_fade_alpha = fade_alpha if hit_target else 1.0
		var ft := 1.0 - exp(-fade_smooth_speed * delta)
		_current_fade_alpha = lerp(_current_fade_alpha, _desired_fade_alpha, ft)
		_apply_fade_to_target(_current_fade_alpha)

func _apply_fade_to_target(alpha: float) -> void:
	for mesh in _fade_meshes:
		if not is_instance_valid(mesh):
			continue
		var mesh_id: int = mesh.get_instance_id()
		var orig_color: Color = _fade_orig_colors.get(mesh_id, Color(1, 1, 1, 1))

		# If fully opaque again, restore original material_override
		if alpha >= 0.999:
			var orig_mat: Material = _fade_orig_materials.get(mesh_id, null)
			mesh.material_override = orig_mat
			continue

		# Ensure we have a StandardMaterial3D as material_override to tweak alpha
		var mat: Material = mesh.material_override
		if not (mat is StandardMaterial3D):
			# try to duplicate an existing active material, otherwise create a new StandardMaterial3D
			var base_mat: Material = mesh.get_active_material(0)
			if base_mat is StandardMaterial3D:
				mat = base_mat.duplicate()
			else:
				mat = StandardMaterial3D.new()
				mat.albedo_color = orig_color
			# enable alpha blending on the override
			mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			mesh.material_override = mat

		# Apply faded alpha to the override material
		if mat is StandardMaterial3D:
			var new_color: Color = orig_color
			new_color.a = orig_color.a * alpha
			(mat as StandardMaterial3D).albedo_color = new_color
		else:
			# fallback: do nothing if material type unexpected
			pass

func _collect_fade_meshes() -> void:
	_fade_meshes.clear()
	_fade_orig_colors.clear()
	_fade_orig_materials.clear()
	if not is_instance_valid(target):
		return
	var list: Array = []
	_collect_meshes_recursive(target, list)
	for m in list:
		if m is MeshInstance3D:
			_fade_meshes.append(m)

			var mesh_id: int = m.get_instance_id()
			# store original material_override so we can restore later
			_fade_orig_materials[mesh_id] = m.material_override

			# try to read an albedo_color from the active material or override; fallback to white
			var base_mat: Material = m.material_override if m.material_override else m.get_active_material(0)
			var orig_color: Color = Color(1, 1, 1, 1)
			if base_mat is StandardMaterial3D:
				orig_color = (base_mat as StandardMaterial3D).albedo_color
			_fade_orig_colors[mesh_id] = orig_color
			# ensure we have a material_override we can tint (create if missing)
			if not (m.material_override is StandardMaterial3D):
				var override_mat: StandardMaterial3D = StandardMaterial3D.new()
				override_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
				override_mat.albedo_color = orig_color
				m.material_override = override_mat

func _collect_meshes_recursive(node: Node, out_list: Array) -> void:
	if node is MeshInstance3D:
		out_list.append(node)
	for child in node.get_children():
		if child is Node:
			_collect_meshes_recursive(child, out_list)

func _is_descendant_of(node: Object, parent_node: Node) -> bool:
	if not is_instance_valid(node) or not is_instance_valid(parent_node):
		return false
	var n := node
	while n:
		if n == parent_node:
			return true
		n = n.get_parent()
	return false
