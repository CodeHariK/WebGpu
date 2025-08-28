extends Camera3D

@export var mouse_sensitivity = 0.5
@export var pan_speed = .5
@export var zoom_speed = 5
@export var orbit_speed = 1
@export var min_pitch_angle := -89.0
@export var max_pitch_angle := 89.0

var camera_distance = 10.0
var camera_offset = Vector3.ZERO
@onready var horizontal_pivot: Node3D = get_parent()

func _ready():
    Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
    position = Vector3(0, 0, camera_distance)
    # Initialize rotation
    rotation.x = 0
    horizontal_pivot.rotation.y = 0

func _input(event):
    if event is InputEventMouseButton:
        if event.button_index == MOUSE_BUTTON_WHEEL_UP:
            # Zoom in
            camera_distance = max(1, camera_distance - zoom_speed)
            position.z = camera_distance
            
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            # Zoom out
            camera_distance = camera_distance + zoom_speed
            position.z = camera_distance
            
    elif event is InputEventMouseMotion:
        if event.button_mask == MOUSE_BUTTON_MASK_MIDDLE:
            if Input.is_key_pressed(KEY_SHIFT):
                # Pan (middle mouse + shift)
                var pan = horizontal_pivot.transform.basis * Vector3(
                    - event.relative.x * pan_speed,
                    event.relative.y * pan_speed,
                    0
                )
                horizontal_pivot.position += pan
            else:
                # Orbit (middle mouse)
                # Horizontal rotation (around Y-axis)
                horizontal_pivot.rotate_y(-event.relative.x * orbit_speed * 0.01)
                
                # Vertical rotation (around X-axis)
                var new_pitch = rotation.x - event.relative.y * orbit_speed * 0.01
                rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))
