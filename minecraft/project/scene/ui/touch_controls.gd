extends CanvasLayer
## Touch controls layer: two VirtualJoysticks (left = move, right = look).
## Only active on touchscreen devices. On desktop the whole layer is hidden so
## the joysticks' full-screen Control rects can never swallow mouse clicks.
## Set `force_show` to test on desktop (pair it with the project setting
## input_devices/pointing/emulate_touch_from_mouse so clicks act as touches).

@export var force_show: bool = false

func _ready() -> void:
	var touch := DisplayServer.is_touchscreen_available()
	visible = touch or force_show
