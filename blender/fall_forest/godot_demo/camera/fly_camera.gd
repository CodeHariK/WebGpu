class_name FlyCamera
extends Camera3D
## Fly round the forest.
##   desktop   hold the right mouse button and move the mouse to look, WASD to move, Q / E down /
##             up, Shift faster
##   touch     drag on the left half of the screen to move (like a stick: how far you drag is
##             how fast, along where the camera looks), drag on the right half to look

const SPEED := 12.0 ## metres a second
const FAST := 5.0 ## x SPEED with Shift
const LOOK := 0.004 ## radians a pixel
const STICK := 120.0 ## pixels of drag for full speed

var _yaw := 0.0
var _pitch := 0.0
var _stick_finger := -1 # the touch moving the camera, and where it started
var _stick_start := Vector2.ZERO
var _stick := Vector2.ZERO # -1 .. 1: x right, y down the screen


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
		_turn(p_event.relative)
	elif p_event is InputEventScreenTouch:
		_touch(p_event)
	elif p_event is InputEventScreenDrag:
		if p_event.index == _stick_finger:
			_stick = ((p_event.position - _stick_start) / STICK).limit_length(1.0)
		else:
			_turn(p_event.relative)


func _process(p_delta: float) -> void:
	var move := Vector3(
			Input.get_axis(&"ui_left", &"ui_right") + _key(KEY_D) - _key(KEY_A) + _stick.x,
			_key(KEY_E) - _key(KEY_Q),
			Input.get_axis(&"ui_up", &"ui_down") + _key(KEY_S) - _key(KEY_W) + _stick.y)
	if move == Vector3.ZERO:
		return
	var speed := SPEED * (FAST if Input.is_key_pressed(KEY_SHIFT) else 1.0)
	position += global_basis * move.limit_length(1.0) * speed * p_delta


## A finger down on the left half starts the stick; lifted, the stick lets go.
func _touch(p_event: InputEventScreenTouch) -> void:
	var left_half := p_event.position.x < get_viewport().get_visible_rect().size.x * 0.5
	if p_event.pressed and left_half and _stick_finger == -1:
		_stick_finger = p_event.index
		_stick_start = p_event.position
	elif not p_event.pressed and p_event.index == _stick_finger:
		_stick_finger = -1
		_stick = Vector2.ZERO


func _turn(p_pixels: Vector2) -> void:
	_yaw -= p_pixels.x * LOOK
	_pitch = clampf(_pitch - p_pixels.y * LOOK, -1.5, 1.5)
	rotation = Vector3(_pitch, _yaw, 0.0)


func _key(p_key: Key) -> float:
	return 1.0 if Input.is_key_pressed(p_key) else 0.0
