## The follow camera, set by the theme (MoodTheme cam_*) and nudged by camera zones (MoodCameraZone):
##   rig       a pivot that trails the character (lag) and leans ahead of where it's going, a
##             SpringArm3D swung down by the pitch (so walls and the ground push the camera in
##             instead of letting it clip), and the Camera3D on its end
##   feel      fov, distance, height, pitch, lag from the theme, blended with every zone the
##             character is inside (each by its own weight, fading at the zone's edge)
##   handheld  slow noise on the camera's rotation (sway) and a slow drifting Dutch roll
##   lens      depth of field (CameraAttributesPractical: far blur; near + far = tilt-shift), and a
##             screen shader for colour fringing at the edges (aberration)
##   events    shake() adds a decaying jolt on top (hits, stomps)
## Everything blends, so switching theme or walking into a zone glides rather than cuts.
class_name MoodCamera
extends Node3D

const LOOK_AHEAD := 0.45 ## seconds of the character's velocity the pivot leans ahead
const ZONE_EASE := 2.5 ## 1/s: how fast the zone blend follows (on top of the theme's own blend)
const SWAY_RATE := 0.4 ## noise units per second for the handheld drift: slow, about half a wobble a second
const SHAKE_RATE := 15.0 ## noise units per second for shake(): a fast jolt
const SWAY_ANGLE := 0.03 ## radians of drift at sway 1
const SHAKE_ANGLE := 0.06 ## radians of jolt at shake 1

var target: Node3D ## the character
var look: MoodLook
var zones: Array[MoodCameraZone] = []
var camera := Camera3D.new()
var lens_material: ShaderMaterial ## the aberration screen shader (MoodDemo adds it)
var active_zone := "" ## name of the strongest zone the character is in ("" = none)

var _arm := SpringArm3D.new()
var _attributes := CameraAttributesPractical.new()
var _noise := FastNoiseLite.new()
var _pivot := Vector3.ZERO
var _yaw := 0.0
var _last_target := Vector3.ZERO
var _time := 0.0
var _shake := 0.0
var _feel := {} ## the blended camera numbers this frame


func _ready() -> void:
	top_level = true
	_arm.spring_length = 6.0
	_arm.margin = 0.3
	_arm.collision_mask = 1
	add_child(_arm)
	_arm.add_child(camera)
	camera.attributes = _attributes
	_noise.frequency = 1.0
	_pivot = target.global_position
	_last_target = _pivot
	_yaw = target.global_rotation.y


## A jolt (0 … 1) that decays over about a second.
func shake(amount: float) -> void:
	_shake = maxf(_shake, amount)


func _process(delta: float) -> void:
	_time += delta
	var theme := look.shown
	_blend_feel(theme, delta)
	var velocity := (target.global_position - _last_target) / maxf(delta, 1e-4)
	_last_target = target.global_position
	var want: Vector3 = target.global_position + Vector3.UP * _feel.height + velocity * Vector3(1, 0, 1) * LOOK_AHEAD
	var follow: float = 1.0 - exp(-_feel.lag * delta)
	_pivot = _pivot.lerp(want, follow)
	_yaw = lerp_angle(_yaw, target.global_rotation.y, follow)
	global_position = _pivot
	_arm.rotation = Vector3(-deg_to_rad(_feel.pitch), _yaw, 0)
	_arm.spring_length = _feel.distance
	camera.fov = _feel.fov
	# handheld sway (slow drift) + drifting roll + shake (fast jolt), on the camera only (the arm
	# stays put). Sway and shake read the noise at very different speeds, so the drift never jitters.
	var sway: float = _feel.sway * SWAY_ANGLE
	var slow := _time * SWAY_RATE
	var jolt := _shake * _shake * SHAKE_ANGLE
	var fast := _time * SHAKE_RATE
	camera.rotation = Vector3(
		_noise.get_noise_2d(slow, 0.0) * sway + _noise.get_noise_2d(fast, 100.0) * jolt,
		_noise.get_noise_2d(0.0, slow) * sway + _noise.get_noise_2d(100.0, fast) * jolt,
		deg_to_rad(_feel.roll * sin(_time * 0.25)) + _noise.get_noise_2d(slow, 50.0) * sway * 0.5 + _noise.get_noise_2d(fast, 150.0) * jolt * 0.5)
	_shake = maxf(_shake - delta * 1.2, 0.0)
	_apply_lens()


# Theme numbers, then each zone pulls toward its overrides by how deep the character is in it.
func _blend_feel(theme: MoodTheme, delta: float) -> void:
	var feel := {
		"fov": theme.cam_fov, "distance": theme.cam_distance, "height": theme.cam_height, "pitch": theme.cam_pitch,
		"lag": theme.cam_lag, "sway": theme.cam_sway, "roll": theme.cam_roll, "dof_far": theme.cam_dof_far,
		"dof_near": theme.cam_dof_near, "aberration": theme.cam_aberration,
	}
	var strongest := 0.0
	active_zone = ""
	for zone in zones:
		zone.weight = move_toward(zone.weight, zone.influence(target.global_position), ZONE_EASE * delta)
		if zone.weight <= 0.0:
			continue
		for key: String in zone.overrides:
			feel[key] = lerpf(feel[key], zone.overrides[key], zone.weight)
		if zone.weight > strongest:
			strongest = zone.weight
			active_zone = zone.zone_name
	_feel = feel


func _apply_lens() -> void:
	var far: float = _feel.dof_far
	var near: float = _feel.dof_near
	_attributes.dof_blur_far_enabled = far > 0.5
	_attributes.dof_blur_far_distance = far
	_attributes.dof_blur_far_transition = maxf(far * 0.5, 2.0)
	_attributes.dof_blur_near_enabled = near > 0.5
	_attributes.dof_blur_near_distance = near
	_attributes.dof_blur_near_transition = maxf(near * 0.5, 1.0)
	_attributes.dof_blur_amount = 0.12
	if lens_material != null:
		var amount: float = _feel.aberration + _shake * 0.5
		lens_material.set_shader_parameter("strength", amount)
		(lens_material.get_meta("rect") as CanvasItem).visible = amount > 0.01 # a full-screen pass: skip it when off
