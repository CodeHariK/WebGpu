## Mouse look, on the player's Head node: moving the mouse sideways turns the whole body (yaw),
## up/down tilts only the head (pitch, clamped). Click to capture the mouse, Esc to let it go.
## Touch look (step 11) will call `look()` with a drag delta.
extends Node3D

@export var sensitivity := 0.0025  ## radians per pixel
@export var max_pitch := deg_to_rad(85.0)

@onready var _body: Node3D = get_parent()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event.is_action_pressed("release_mouse"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		look(event.screen_relative)


## Turn by a screen-space delta in pixels.
func look(delta_pixels: Vector2) -> void:
	_body.rotate_y(-delta_pixels.x * sensitivity)
	rotation.x = clampf(rotation.x - delta_pixels.y * sensitivity, -max_pitch, max_pitch)


## Point the view at a yaw and pitch (radians), for spawning and the test autopilot.
func face(yaw: float, pitch := 0.0) -> void:
	_body.rotation.y = yaw
	rotation.x = pitch
