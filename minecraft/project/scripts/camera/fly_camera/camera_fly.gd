extends Camera3D

@export var pan_speed = .2
@export var zoom_speed = 2
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
            # Zoom in - move pivot forward along its basis Z (negative direction)
            horizontal_pivot.translate(-transform.basis.z * zoom_speed)
            
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            # Zoom out - move pivot backward along its basis Z (positive direction)
            horizontal_pivot.translate(transform.basis.z * zoom_speed)
            
    elif event is InputEventMouseMotion:
        if event.button_mask == MOUSE_BUTTON_MASK_MIDDLE:
            if Input.is_key_pressed(KEY_SHIFT):
                # Pan (middle mouse + shift)
                var pan = global_transform.basis * Vector3(
                    - event.relative.x * pan_speed,
                     event.relative.y * pan_speed,
                    0
                )
                horizontal_pivot.position += pan
            else:
                # Orbit (middle mouse)
                # Rotate around Y-axis relative to camera's current position
                horizontal_pivot.rotate_y(-event.relative.x * orbit_speed * 0.01)
                
                # Adjust pitch on self
                var new_pitch = rotation.x - event.relative.y * orbit_speed * 0.01
                rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))
