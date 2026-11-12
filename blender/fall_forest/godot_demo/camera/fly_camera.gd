class_name FlyCamera
extends Camera3D
## Fly round the forest: hold the right mouse button and move the mouse to look, WASD to move,
## Q / E down / up, Shift faster.

const SPEED := 12.0 ## metres a second
const FAST := 5.0 ## x SPEED with Shift
const LOOK := 0.004 ## radians a pixel

var _yaw := 0.0
var _pitch := 0.0


func _ready() -> void:
	_yaw = rotation.y
	_pitch = rotation.x


## Put the camera at p_from looking at p_at.
func place(p_from: Vector3, p_at: Vector3) -> void:
	position = p_from
	look_at(p_at)
	_yaw = rotation.y
	_pitch = rotation.x


func _unhandled_input(p_event: InputEvent) -> void:
	if p_event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		_yaw -= p_event.relative.x * LOOK
		_pitch = clampf(_pitch - p_event.relative.y * LOOK, -1.5, 1.5)
		rotation = Vector3(_pitch, _yaw, 0.0)


func _process(p_delta: float) -> void:
	var move := Vector3(
			Input.get_axis(&"ui_left", &"ui_right") + _key(KEY_D) - _key(KEY_A),
			_key(KEY_E) - _key(KEY_Q),
			Input.get_axis(&"ui_up", &"ui_down") + _key(KEY_S) - _key(KEY_W))
	if move == Vector3.ZERO:
		return
	var speed := SPEED * (FAST if Input.is_key_pressed(KEY_SHIFT) else 1.0)
	position += global_basis * move.limit_length(1.0) * speed * p_delta


func _key(p_key: Key) -> float:
	return 1.0 if Input.is_key_pressed(p_key) else 0.0
