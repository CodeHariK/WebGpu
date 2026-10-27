## The flashlight: a warm spotlight with their cookie texture (the light's pattern), held low and
## to the left like the original's left hand, aimed where you look but lagging a little, so the
## beam drifts after the view when you turn. F switches it on and off (with a click of flicker).
##
## It's the only light in the game that casts shadows, so the darkness reads but stays cheap:
## one spotlight, one shadow map (project setting: positional shadow atlas 2048, one quadrant).
## On the player's Head, next to the Camera3D. top_level: we set its transform each frame.
extends SpotLight3D

const COOKIE := preload("res://assets/player/textures/Flashlight_Cookie.png")

@export var hold := Vector3(-0.18, -0.15, -0.5)  ## where the beam starts, in the camera's space
## (just past the hands: a light this close to the arms would blow them out)
@export var follow := 7.0  ## how fast the beam catches up with the view
@export var max_lag := deg_to_rad(10.0)
@export var energy := 7.0
@export var on := true

var _aim := Basis.IDENTITY
var _flicker := 0.0

@onready var _camera: Camera3D = $"../Camera3D"


func _ready() -> void:
	top_level = true
	process_priority = 10  # after the camera has moved
	light_projector = COOKIE
	light_color = Color(0.93, 0.73, 0.57)
	spot_range = 35.0
	spot_angle = 28.0
	spot_attenuation = 0.8
	spot_angle_attenuation = 0.6
	shadow_enabled = true
	shadow_bias = 0.05
	shadow_normal_bias = 1.0
	_aim = _camera.global_basis
	_apply()


func toggle() -> void:
	on = not on
	_flicker = 0.12  # a short stutter when it switches
	_apply()


func _process(delta: float) -> void:
	if Input.is_action_just_pressed("flashlight"):
		toggle()
	var target := _camera.global_basis.orthonormalized()
	_aim = _aim.slerp(target, 1.0 - exp(-follow * delta)).orthonormalized()
	var behind := _aim.get_rotation_quaternion().angle_to(target.get_rotation_quaternion())
	if behind > max_lag:
		_aim = _aim.slerp(target, 1.0 - max_lag / behind).orthonormalized()
	global_transform = Transform3D(_aim, _camera.global_transform * hold)
	if _flicker > 0.0:
		_flicker -= delta
		light_energy = energy * randf_range(0.2, 1.0) if on else 0.0
		if _flicker <= 0.0:
			_apply()


func _apply() -> void:
	visible = on or _flicker > 0.0
	light_energy = energy if on else 0.0
