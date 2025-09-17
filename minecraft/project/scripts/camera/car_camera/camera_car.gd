extends Node3D

@export var target: Node3D = null
@export var y_dis: float = 5.0
@export var follow_speed: float = 5.0
@export var zoom_speed: float = 2
@export var orbit_speed: float = 1
@export var min_pitch_angle := -89.0
@export var max_pitch_angle := 89.0
@export var min_zoom_distance := 5.0
@export var max_zoom_distance := 40.0
@export var target_distance: float = 0.0

@onready var camera: Camera3D = $FlyCamera3D

func _ready():
    Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

    target_distance = min_zoom_distance

func _physics_process(delta: float) -> void:
    if is_instance_valid(target):
        global_transform.origin = target.global_transform.origin
        # Calculate camera position at target_distance along local -Z axis
        # Calculate camera position at target_distance along local -Z axis
        camera.transform.origin = Vector3(0, y_dis, target_distance)
        camera.look_at(target.global_transform.origin, Vector3.UP)

func _input(event):
    if event is InputEventMouseButton:
        if event.button_index == MOUSE_BUTTON_WHEEL_UP:
            target_distance = clamp(target_distance - zoom_speed, min_zoom_distance, max_zoom_distance)
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            target_distance = clamp(target_distance + zoom_speed, min_zoom_distance, max_zoom_distance)
            
    elif event is InputEventMouseMotion:
        # Orbit pivot horizontally around Y
        rotate_y(-event.relative.x * orbit_speed * 0.01)

        # Pitch camera locally around X
        var new_pitch = camera.rotation.x - event.relative.y * orbit_speed * 0.01
        camera.rotation.x = clampf(new_pitch, deg_to_rad(min_pitch_angle), deg_to_rad(max_pitch_angle))

    elif event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
        if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
            Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
        else:
            Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
