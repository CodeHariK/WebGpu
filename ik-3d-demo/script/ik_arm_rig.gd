## Builds the whole adjusted-IK rig at runtime, so the scene only needs the arm, the gun and the
## animation. Recreates exactly what adjusted_ik_3d_demo.tscn sets up in the editor:
##
##   gun hierarchy (under the animated GunRoot, which is the gun.glb instance):
##     GunRoot ── GunOrigin ── Grip  ← the IK target every modifier points at
##                              └─ Mesh (moved here from GunRoot)
##     GunOrigin turns the gun to face away from the arm; Grip is where the hand holds it.
##
##   modifiers under the Skeleton3D, in run order (they run top to bottom):
##     1 AimModifier3D           point the root bone at the grip first (good starting pose)
##     2 FABRIK3D                bend the chain so the hand reaches the grip
##     3 CopyTransformModifier3D give the hand the grip's rotation (rotation only)
##     4 BoneTwistDisperser3D    spread the hand's roll along the whole arm
##   plus the "grab" fade from constraint_interpolation.gd: the hand only takes the grip's
##   rotation when it is close (CopyTransform amount 1 at the grip, 0 at `grab_distance`).
extends Node

@export var skeleton: Skeleton3D ## empty = first Skeleton3D in the scene
@export var gun_root: Node3D ## the animated gun.glb instance; empty = node named "GunRoot"
@export var root_bone := "arm.001"
@export var end_bone := "hand_p"

@export_group("Modifiers")
## Each toggle applies immediately, also while the game runs (Remote tree in the editor).
@export var use_aim := true:
	set(value):
		use_aim = value
		_set_active(aim, value)
@export var use_fabrik := true:
	set(value):
		use_fabrik = value
		_set_active(fabrik, value)
@export var use_copy_rotation := true:
	set(value):
		use_copy_rotation = value
		_set_active(copy, value)
@export var use_twist := true:
	set(value):
		use_twist = value
		_set_active(twist, value)

@export_group("Grab fade")
@export var use_grab_fade := true
@export_range(0.01, 5.0, 0.01, "or_greater") var grab_distance := 0.5
@export var grab_curve: Curve ## x: distance / grab_distance, y: copy amount. Empty = original curve.

# Transforms copied from adjusted_ik_3d_demo.tscn.
const GUN_ORIGIN := Transform3D(Basis(Vector3(0, 1, 0), Vector3(1, 0, 0), Vector3(0, 0, -1)), Vector3(-0.55, 0, 0))
const GRIP := Transform3D(Basis.IDENTITY, Vector3(0, 0.4, 0))
const MESH := Transform3D(Basis(Vector3(0, 0, 1), Vector3(1, 0, 0), Vector3(0, 1, 0)), Vector3(0, 0.15, 0))

var grip: Node3D
var aim: AimModifier3D
var fabrik: FABRIK3D
var copy: CopyTransformModifier3D
var twist: BoneTwistDisperser3D
var _hand_bone := -1


func _ready() -> void:
	if not _resolve_nodes():
		return
	grip = _build_gun_hierarchy()
	aim = _add_aim()
	fabrik = _add_fabrik()
	copy = _add_copy()
	twist = _add_twist()
	_hand_bone = skeleton.find_bone(end_bone)
	if grab_curve == null:
		grab_curve = _original_grab_curve()
	fabrik.modification_processed.connect(_on_fabrik_processed)


func _resolve_nodes() -> bool:
	var scene := get_tree().current_scene
	if skeleton == null:
		var found := scene.find_children("*", "Skeleton3D", true, false)
		if not found.is_empty():
			skeleton = found[0]
	if gun_root == null:
		gun_root = scene.find_child("GunRoot", true, false) as Node3D
	if skeleton == null or gun_root == null:
		push_error("IKArmRig: set `skeleton` and `gun_root` in the Inspector.")
		return false
	return true


# GunRoot (animated) → GunOrigin → Grip, with the gun's Mesh moved under Grip.
func _build_gun_hierarchy() -> Node3D:
	var origin := Node3D.new()
	origin.name = "GunOrigin"
	origin.transform = GUN_ORIGIN
	gun_root.add_child(origin)
	var target := Node3D.new()
	target.name = "Grip"
	target.transform = GRIP
	origin.add_child(target)
	var mesh := gun_root.get_node_or_null("Mesh") as Node3D
	if mesh != null:
		mesh.reparent(target, false)
		mesh.transform = MESH
	return target


# --- the four modifiers, added in run order --------------------------------------------------

func _add_aim() -> AimModifier3D:
	var m := AimModifier3D.new()
	_attach(m, "AimModifier3D", use_aim)
	m.set_setting_count(1)
	m.set_apply_bone_name(0, root_bone)
	m.set_reference_type(0, BoneConstraint3D.REFERENCE_TYPE_NODE)
	m.set_reference_node(0, m.get_path_to(grip))
	m.set_forward_axis(0, SkeletonModifier3D.BONE_AXIS_PLUS_X)
	m.set_use_euler(0, false)
	m.set_relative(0, true)
	m.set_amount(0, 1.0)
	return m


func _add_fabrik() -> FABRIK3D:
	var m := FABRIK3D.new()
	_attach(m, "FABRIK3D", use_fabrik)
	m.angular_delta_limit = PI
	m.deterministic = true
	m.set_setting_count(1)
	m.set_root_bone_name(0, root_bone)
	m.set_end_bone_name(0, end_bone)
	m.set_extend_end_bone(0, false)
	m.set_target_node(0, m.get_path_to(grip))
	return m


func _add_copy() -> CopyTransformModifier3D:
	var m := CopyTransformModifier3D.new()
	_attach(m, "CopyTransformModifier3D", use_copy_rotation)
	m.set_setting_count(1)
	m.set_apply_bone_name(0, end_bone)
	m.set_reference_type(0, BoneConstraint3D.REFERENCE_TYPE_NODE)
	m.set_reference_node(0, m.get_path_to(grip))
	m.set_copy_flags(0, CopyTransformModifier3D.TRANSFORM_FLAG_ROTATION)
	m.set_axis_flags(0, CopyTransformModifier3D.AXIS_FLAG_ALL)
	m.set_additive(0, false)
	m.set_amount(0, 1.0)
	return m


func _add_twist() -> BoneTwistDisperser3D:
	var m := BoneTwistDisperser3D.new()
	_attach(m, "BoneTwistDisperser3D", use_twist)
	m.set_setting_count(1)
	m.set_root_bone_name(0, root_bone)
	m.set_end_bone_name(0, end_bone)
	m.set_extend_end_bone(0, true)
	m.set_end_bone_direction(0, SkeletonModifier3D.BONE_DIRECTION_FROM_PARENT)
	m.set_twist_from_rest(0, true)
	m.set_disperse_mode(0, BoneTwistDisperser3D.DISPERSE_MODE_EVEN)
	return m


# Null until _ready has built the modifiers (setters also run when the scene loads).
func _set_active(modifier: SkeletonModifier3D, enabled: bool) -> void:
	if modifier != null:
		modifier.active = enabled


# Added as the last child, so modifiers run in the order they are attached.
func _attach(modifier: SkeletonModifier3D, node_name: String, enabled: bool) -> void:
	modifier.name = node_name
	modifier.active = enabled
	skeleton.add_child(modifier)


# --- grab fade (constraint_interpolation.gd, fixed) ------------------------------------------

# Runs right after FABRIK and before CopyTransform, every frame.
func _on_fabrik_processed() -> void:
	if not use_grab_fade:
		copy.set_amount(0, 1.0)
		return
	var hand := skeleton.global_transform * skeleton.get_bone_global_pose(_hand_bone).origin
	var t := clampf(hand.distance_to(grip.global_position) / grab_distance, 0.0, 1.0)
	copy.set_amount(0, clampf(grab_curve.sample(t), 0.0, 1.0))


# Same as the original: 1 at the grip, falling fast at first, 0 at grab_distance.
func _original_grab_curve() -> Curve:
	var curve := Curve.new()
	curve.add_point(Vector2(0, 1), 0.0, -2.0)
	curve.add_point(Vector2(1, 0))
	return curve
