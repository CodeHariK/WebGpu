## A free editor-style camera: orbit, pan, zoom and fly around a pivot point.
##   right-drag             orbit around the pivot
##   middle-drag / Shift + right-drag   pan (the pivot slides with the view)
##   wheel                  zoom toward / away from the pivot
##   trackpad two-finger    orbit (Shift: pan)      pinch  zoom
##   hold right mouse + W A S D / Q E   fly (the pivot moves with you), Shift = 3× faster
## The camera never rolls; pitch stops just short of straight up / down. Pan and fly speeds grow
## with the distance to the pivot, so far views move fast and close views stay precise.
## Set `active` false to leave the camera alone (another script drives it).
class_name FlyCamera
extends Camera3D

const ORBIT_SPEED := 0.006 ## radians per pixel dragged
const PAN_SPEED := 0.0015 ## × distance, metres per pixel
const ZOOM_STEP := 0.88 ## distance × this per wheel notch in
const DISTANCE := Vector2(1.0, 150.0) ## nearest / furthest from the pivot
const PITCH_LIMIT := 1.5 ## radians (~86°)
const FLY_SPEED := 0.8 ## × distance per second (at least MIN_FLY)
const MIN_FLY := 3.0 ## m/s
const FAST := 3.0 ## Shift multiplier
const EASE := 14.0 ## 1/s: how quickly the view catches up with the input (smooths the mouse)

var active := true
var pivot := Vector3.ZERO
var yaw := 0.0 ## radians about +Y (0 = looking down -Z)
var pitch := -0.4 ## radians, negative = looking down
var distance := 12.0

var _shown_pivot := Vector3.ZERO ## the eased pivot actually shown
var _shown_angles := Vector3.ZERO ## (yaw, pitch, distance)
var _orbiting := false
var _panning := false


## Start from wherever the camera is now, orbiting the point it looks at `look_distance` ahead.
func take_over(look_distance: float) -> void:
	var forward := -global_basis.z
	distance = clampf(look_distance, DISTANCE.x, DISTANCE.y)
	pivot = global_position + forward * distance
	yaw = atan2(-forward.x, -forward.z)
	pitch = clampf(asin(clampf(forward.y, -1.0, 1.0)), -PITCH_LIMIT, PITCH_LIMIT)
	_snap()


## Move the pivot to `point` (smoothly), keeping the angle and distance.
func focus(point: Vector3) -> void:
	pivot = point


## True while the right mouse button is held: WASD flies the camera, not anything else.
func flying() -> bool:
	return active and _orbiting


func _process(delta: float) -> void:
	if not active:
		return
	_fly(delta)
	var t := 1.0 - exp(-EASE * delta)
	_shown_pivot = _shown_pivot.lerp(pivot, t)
	_shown_angles = _shown_angles.lerp(Vector3(yaw, pitch, distance), t)
	_place(_shown_pivot, _shown_angles)


func _unhandled_input(event: InputEvent) -> void:
	if not active:
		return
	var button := event as InputEventMouseButton
	if button != null:
		match button.button_index:
			MOUSE_BUTTON_RIGHT:
				_orbiting = button.pressed
			MOUSE_BUTTON_MIDDLE:
				_panning = button.pressed
			MOUSE_BUTTON_WHEEL_UP:
				if button.pressed:
					_zoom(pow(ZOOM_STEP, button.factor if button.factor > 0.0 else 1.0))
			MOUSE_BUTTON_WHEEL_DOWN:
				if button.pressed:
					_zoom(pow(1.0 / ZOOM_STEP, button.factor if button.factor > 0.0 else 1.0))
		return
	var motion := event as InputEventMouseMotion
	if motion != null:
		if _panning or (_orbiting and Input.is_key_pressed(KEY_SHIFT)):
			_pan(motion.relative)
		elif _orbiting:
			_orbit(motion.relative)
		return
	var pan_gesture := event as InputEventPanGesture # trackpad two-finger scroll
	if pan_gesture != null:
		if Input.is_key_pressed(KEY_SHIFT):
			_pan(-pan_gesture.delta * 12.0)
		else:
			_orbit(pan_gesture.delta * 12.0)
		return
	var magnify := event as InputEventMagnifyGesture # trackpad pinch
	if magnify != null and magnify.factor > 0.0:
		_zoom(1.0 / magnify.factor)


func _orbit(pixels: Vector2) -> void:
	yaw -= pixels.x * ORBIT_SPEED
	pitch = clampf(pitch - pixels.y * ORBIT_SPEED, -PITCH_LIMIT, PITCH_LIMIT)


# Drag the world: the pivot moves opposite to the mouse in the view plane.
func _pan(pixels: Vector2) -> void:
	var view := _basis(yaw, pitch)
	pivot += (-view.x * pixels.x + view.y * pixels.y) * PAN_SPEED * distance


func _zoom(factor: float) -> void:
	distance = clampf(distance * factor, DISTANCE.x, DISTANCE.y)


# Right mouse held: WASD along the view (flat-ish: forward follows the look direction), Q/E down/up.
func _fly(delta: float) -> void:
	if not _orbiting:
		return
	var input := Vector3(
		float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)),
		float(Input.is_key_pressed(KEY_E)) - float(Input.is_key_pressed(KEY_Q)),
		float(Input.is_key_pressed(KEY_S)) - float(Input.is_key_pressed(KEY_W)))
	if input == Vector3.ZERO:
		return
	var view := _basis(yaw, pitch)
	var speed := maxf(FLY_SPEED * distance, MIN_FLY) * (FAST if Input.is_key_pressed(KEY_SHIFT) else 1.0)
	var move := view.x * input.x + Vector3.UP * input.y + view.z * input.z
	pivot += move.normalized() * speed * delta


func _snap() -> void:
	_shown_pivot = pivot
	_shown_angles = Vector3(yaw, pitch, distance)
	_place(_shown_pivot, _shown_angles)


func _place(at: Vector3, angles: Vector3) -> void:
	var view := _basis(angles.x, angles.y)
	global_transform = Transform3D(view, at + view.z * angles.z)


static func _basis(yaw_angle: float, pitch_angle: float) -> Basis:
	return Basis(Vector3.UP, yaw_angle) * Basis(Vector3.RIGHT, pitch_angle)
