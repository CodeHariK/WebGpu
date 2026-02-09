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
@export var camera_offset = Vector3(0, 4, 0)

@onready var camera_x_pivot: Node3D = $"."
@onready var camera_spring: SpringArm3D = $SpringArm3D
@onready var camera: Camera3D = $SpringArm3D/Camera

func _ready():
    Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
    #camera.position = Vector3(0, 0, camera_distance)
    # Initialize rotation
    #camera.rotation.x = 0
    #camera_x_pivot.rotation.y = 0

func _input(event):
    if camera_type == CameraType.FLY:
        process_fly_camera(event)
    if camera_type == CameraType.TARGET:
        process_target_camera(event)

func _process(delta: float) -> void:
    if camera_type == CameraType.TARGET and is_instance_valid(target):
        camera_x_pivot.global_position = target.global_position + camera_offset
        camera.look_at(target.global_position)

func process_fly_camera(event) -> void:
    if event is InputEventMouseButton:
        if event.button_index == MOUSE_BUTTON_WHEEL_UP:
            # Zoom in - move pivot forward along its basis Z (negative direction)
            camera_x_pivot.translate(-camera.transform.basis.z * zoom_speed)
            
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            # Zoom out - move pivot backward along its basis Z (positive direction)
            camera_x_pivot.translate(camera.transform.basis.z * zoom_speed)
            
    elif event is InputEventMouseMotion:
        if event.button_mask == MOUSE_BUTTON_MASK_MIDDLE:
            if Input.is_key_pressed(KEY_SHIFT):
                # Pan (middle mouse + shift)
                var pan = camera.global_transform.basis * Vector3(
                    - event.relative.x * pan_speed,
                    event.relative.y * pan_speed,
                    0
                )
                camera_x_pivot.position += pan
            else:
                # Orbit (middle mouse)
                # Rotate around Y-axis relative to camera's current position
                camera_x_pivot.rotate_y(-event.relative.x * orbit_speed * 0.01)
                
                # Adjust pitch on self
                var new_pitch = camera.rotation.x - event.relative.y * orbit_speed * 0.01
                camera.rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))

func process_target_camera(event) -> void:
    if event is InputEventMouseMotion:
        if event.button_mask == MOUSE_BUTTON_MASK_MIDDLE:
            # Orbit around the target
            # Rotate the pivot for yaw (horizontal movement)
            camera_x_pivot.rotate_y(-event.relative.x * orbit_speed * 0.01)
            
            # Rotate the spring arm for pitch (vertical movement)
            var new_pitch = camera_spring.rotation.x - event.relative.y * orbit_speed * 0.01
            camera_spring.rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))
