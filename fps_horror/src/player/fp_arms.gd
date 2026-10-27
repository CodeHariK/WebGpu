## First-person arms and revolver, on the player's Head (next to the Camera3D).
##
## Placement: like the original, the rig's origin sits 0.1 m above the eye, looking where the
## head looks. It's top_level, so we set its transform ourselves every frame:
##   look lag   it turns towards the head's rotation a little late, so the gun trails your view
##   sway       a small figure-8 while walking (less when aiming)
##   sprint     tipped down while sprinting (and no aiming)
## It follows the camera's position (which already bobs and dips), so the arms never slide
## against the view.
##
## Weapon (step 4): a Revolver node (rules, hitscan), a MuzzleFlash on the barrel bone, pooled
## Impacts and ShellDrop, all made here.
##
## Input: aim (hold right mouse), fire (left mouse), reload (R). Only while the mouse is captured,
## and not on the click that captures it. `force_input` turns that check off for test runs.
##
## Mobile: the arms never cast shadows; one AnimationTree, nothing else per frame but a few lerps.
extends Node3D

const Look := preload("res://src/look/character_look.gd")
const Animator := preload("res://src/player/arms_animator.gd")
const ARMS := preload("res://assets/player/ArmsAnimations.gltf")
const Revolver := preload("res://src/weapon/revolver.gd")
const MuzzleFlash := preload("res://src/weapon/muzzle_flash.gd")
const Impacts := preload("res://src/weapon/impacts.gd")
const ShellDrop := preload("res://src/weapon/shell_drop.gd")

@export var offset := Vector3(0.0, 0.1, 0.0)
@export var look_lag := 14.0  ## higher = follows the view more tightly
@export var max_lag := deg_to_rad(6.0)  ## never trail the view by more than this
@export var sway_side := 0.012
@export var sway_up := 0.008
@export var sway_rate := 0.9  ## figure-8 cycles per metre walked
@export var aim_sway := 0.25  ## sway is scaled by this while aiming
@export var sprint_drop := deg_to_rad(22.0)

var animator: Animator
var revolver: Revolver
var force_input := false

var _rig: Node3D
var _lagged := Basis.IDENTITY
var _phase := 0.0
var _sway_weight := 0.0
var _drop := 0.0
var _was_captured := false

@onready var _body: CharacterBody3D = owner
@onready var _camera: Camera3D = $"../Camera3D"


func _ready() -> void:
	top_level = true
	process_priority = 10  # after the camera has moved this frame
	_rig = ARMS.instantiate()
	add_child(_rig)
	Look.dress_arms(_rig)
	for mesh: MeshInstance3D in _rig.find_children("*", "MeshInstance3D", true, false):
		mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	animator = Animator.new(_rig)
	_lagged = get_parent().global_basis
	_setup_weapon()


func _setup_weapon() -> void:
	var skeleton: Skeleton3D = _rig.find_children("*", "Skeleton3D", true, false)[0]
	revolver = Revolver.new()
	add_child(revolver)
	revolver.setup(animator, _camera, _body)
	var barrel := BoneAttachment3D.new()
	barrel.bone_name = "RevolverFront"
	skeleton.add_child(barrel)
	var flash := MuzzleFlash.new()
	barrel.add_child(flash)
	revolver.fired.connect(func(_o: Vector3, _d: Vector3) -> void: flash.flash())
	var impacts := Impacts.new()
	impacts.top_level = true
	add_child(impacts)
	revolver.hit.connect(func(c: Object, p: Vector3, n: Vector3, _d: Vector3) -> void: impacts.spawn(c, p, n))
	var shells := ShellDrop.new()
	add_child(shells)
	shells.setup(skeleton, _body)
	revolver.reload_started.connect(shells.drop)


func _process(delta: float) -> void:
	_handle_input()
	_follow_view(delta)


func _handle_input() -> void:
	var captured := force_input or Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
	var can_act := captured and (_was_captured or force_input)
	_was_captured = captured
	var want_aim: bool = can_act and Input.is_action_pressed("aim") and not _body.sprinting
	animator.update(want_aim)
	if not can_act:
		return
	if Input.is_action_just_pressed("fire"):
		revolver.try_fire()
	elif Input.is_action_just_pressed("reload"):
		revolver.try_reload()


func _follow_view(delta: float) -> void:
	var head_basis := (get_parent() as Node3D).global_basis.orthonormalized()
	_lagged = _lagged.slerp(head_basis, 1.0 - exp(-look_lag * delta)).orthonormalized()
	var behind := _lagged.get_rotation_quaternion().angle_to(head_basis.get_rotation_quaternion())
	if behind > max_lag:  # caught too far behind on a fast turn: snap the excess
		_lagged = _lagged.slerp(head_basis, 1.0 - max_lag / behind).orthonormalized()
	var aiming := animator.current() in [&"HighIdle", &"HighFire", &"LowToHigh"]
	_drop = lerpf(_drop, sprint_drop if _body.sprinting else 0.0, 1.0 - exp(-8.0 * delta))
	global_transform = Transform3D(
		_lagged * Basis(Vector3.RIGHT, -_drop),
		_camera.global_transform * (offset + _sway(delta, aiming)))


## Figure-8 hand sway while walking on the ground, in the camera's local space.
func _sway(delta: float, aiming: bool) -> Vector3:
	var flat := Vector2(_body.velocity.x, _body.velocity.z).length()
	var walking := _body.is_on_floor() and flat > 0.5
	_sway_weight = move_toward(_sway_weight, 1.0 if walking else 0.0, delta * 4.0)
	_phase += flat * sway_rate * delta * PI
	var amount := _sway_weight * (aim_sway if aiming else 1.0)
	return Vector3(sin(_phase) * sway_side, -absf(cos(_phase)) * sway_up, 0.0) * amount
