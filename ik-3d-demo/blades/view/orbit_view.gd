## The editor camera (while the tuning panel is open): orbits round the player, zooms and pans.
##
##   right drag / two-finger swipe          orbit (round the player, up and down)
##   middle drag / shift + right drag
##   / shift + two-finger swipe             pan
##   wheel / pinch                          zoom
##   the panel's "⟲ View" button            back to the game's view
##
## It only says where the camera should be (eye / focus); BladesDemo eases the camera there. With the
## panel closed the game's view is used (DEFAULT), so play is unchanged.
class_name OrbitView
extends RefCounted

const DEFAULT := Vector3(0.0, 7.0, 8.5) ## the game's camera: this far from the focus (behind, above)
const FOCUS_HEIGHT := 0.5 ## m above the player's feet
const ORBIT_SPEED := 0.006 ## radians per pixel dragged
const SWIPE_SPEED := 0.05 ## radians per unit of trackpad swipe
const PITCH_MIN := -0.1 ## radians: just under the horizon …
const PITCH_MAX := 1.5 ## … to nearly straight down
const DISTANCE_MIN := 2.0
const DISTANCE_MAX := 60.0

var yaw := 0.0 ## round the player (0: behind, along +Z)
var pitch := 0.0 ## up from the horizon
var distance := 0.0
var pan := Vector3.ZERO ## the focus, moved off the player

var _drag := "" ## "orbit", "pan" or ""


func _init() -> void:
	reset()


## Back to the game's view.
func reset() -> void:
	yaw = atan2(DEFAULT.x, DEFAULT.z)
	pitch = atan2(DEFAULT.y, Vector2(DEFAULT.x, DEFAULT.z).length())
	distance = DEFAULT.length()
	pan = Vector3.ZERO
	_drag = ""


## What the camera looks at.
func focus(player: Vector3) -> Vector3:
	return player + Vector3.UP * FOCUS_HEIGHT + pan


## Where the camera sits.
func eye(player: Vector3) -> Vector3:
	var direction := Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch))
	return focus(player) + direction * distance


## Mouse / trackpad input; true when it was a camera control (so it isn't also a move).
func handle(
		event: InputEvent,
		camera: Camera3D
) -> bool:
	if event is InputEventMouseButton:
		match event.button_index:
			MOUSE_BUTTON_WHEEL_UP:
				_zoom(0.9)
				return true
			MOUSE_BUTTON_WHEEL_DOWN:
				_zoom(1.0 / 0.9)
				return true
			MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE:
				_drag = "" if not event.pressed else ("pan" if event.button_index == MOUSE_BUTTON_MIDDLE or event.shift_pressed else "orbit")
				return true
	elif event is InputEventMouseMotion and _drag != "":
		if _drag == "orbit":
			_orbit(-event.relative.x * ORBIT_SPEED, event.relative.y * ORBIT_SPEED)
		else:
			_pan(event.relative, camera)
		return true
	elif event is InputEventPanGesture: # two-finger swipe
		if event.shift_pressed:
			_pan(-event.delta * 20.0, camera)
		else:
			_orbit(-event.delta.x * SWIPE_SPEED, -event.delta.y * SWIPE_SPEED)
		return true
	elif event is InputEventMagnifyGesture: # pinch
		_zoom(1.0 / event.factor)
		return true
	return false


func _orbit(
		turn: float,
		tilt: float
) -> void:
	yaw += turn
	pitch = clampf(pitch + tilt, PITCH_MIN, PITCH_MAX)


func _zoom(factor: float) -> void:
	distance = clampf(distance * factor, DISTANCE_MIN, DISTANCE_MAX)


# Drag the focus with the mouse: across the screen's right / up, scaled by how far away it is.
func _pan(
		pixels: Vector2,
		camera: Camera3D
) -> void:
	var per_pixel := distance * 0.0015
	pan += (-camera.global_basis.x * pixels.x + camera.global_basis.y * pixels.y) * per_pixel
