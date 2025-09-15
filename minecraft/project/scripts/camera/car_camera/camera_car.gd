extends Camera3D

@export var target: Node3D = null
@export var follow_speed: float = 5.0
@export var zoom_speed = 2
@export var orbit_speed = 1
@export var min_pitch_angle := -89.0
@export var max_pitch_angle := 89.0
@export var min_zoom_distance := 2.0
@export var max_zoom_distance := 20.0

@onready var horizontal_pivot: Node3D = get_parent()

func _ready():
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	position.z = clamp(position.z, min_zoom_distance, max_zoom_distance)
	# Initialize rotation
	rotation.x = 0
	horizontal_pivot.rotation.y = 0
	
	# Immediately snap to the target's position on start to avoid "flying in"
	if is_instance_valid(target):
		horizontal_pivot.global_position = target.global_position

func _physics_process(delta: float) -> void:
	if is_instance_valid(target):
		var target_pos = target.global_transform.origin
		horizontal_pivot.global_transform.origin = horizontal_pivot.global_transform.origin.lerp(target_pos, delta * follow_speed)

func _input(event):
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			# Zoom in
			position.z = clamp(position.z - zoom_speed, min_zoom_distance, max_zoom_distance)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			# Zoom out
			position.z = clamp(position.z + zoom_speed, min_zoom_distance, max_zoom_distance)
			
	elif event is InputEventMouseMotion:
		# Always orbit on mouse motion
		# Rotate around Y-axis relative to camera's current position
		horizontal_pivot.rotate_y(-event.relative.x * orbit_speed * 0.01)
		
		# Adjust pitch on self
		var new_pitch = rotation.x - event.relative.y * orbit_speed * 0.01
		rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))
