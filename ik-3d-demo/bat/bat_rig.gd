## A cartoon bat's body, built in code: a fuzzy egg body, a big head (ears, eyes, pink nose,
## two tiny fangs), two IK wings (BatWing) and two little IK legs, all on one Skeleton3D with a
## single TwoBoneIK3D (settings 0–1 the arms, 2–3 the legs). Faces −Z, sides are left (−X) and
## right (+X). Bat drives it: pose_wings() every frame, and set_feet() to tuck or grip.
class_name BatRig
extends Node3D

const SHOULDER := Vector3(0.07, 0.035, -0.03) ## right shoulder (left is mirrored)
const HIP := Vector3(0.045, -0.045, 0.1) ## right hip
const MEMBRANE_HIP := Vector3(0.055, -0.01, 0.07) ## where the right membrane meets the body
const UPPER_ARM := 0.15
const FOREARM := 0.2
const THIGH := 0.055
const SHIN := 0.06
const TUCKED_FOOT := Vector3(0.04, -0.05, 0.2) ## right foot while flying, trailing behind

var fur_color := Color(0.45, 0.32, 0.34)
var belly_color := Color(0.62, 0.48, 0.45)
var membrane_color := Color(0.38, 0.23, 0.42)
var inner_ear_color := Color(0.95, 0.6, 0.65)

var skeleton: Skeleton3D
var ik: TwoBoneIK3D
var head: Node3D ## turn it to look
var wings: Array[BatWing] = [] ## left, right
var feet: Array[Marker3D] = [] ## leg IK targets, left, right (place in world space)


func _ready() -> void:
	skeleton = Skeleton3D.new()
	skeleton.name = "Skeleton3D"
	add_child(skeleton)
	var body := skeleton.add_bone("body")
	skeleton.set_bone_rest(body, Transform3D.IDENTITY)
	skeleton.set_bone_pose(body, Transform3D.IDENTITY)
	ik = TwoBoneIK3D.new()
	ik.name = "IK"
	skeleton.add_child(ik)
	ik.set_setting_count(4)
	for i in 2:
		_build_side(i, body)
	_build_body()
	skeleton.skeleton_updated.connect(_on_skeleton_updated)


## Pose both wings the same: `wrist` in right-wing coordinates relative to the shoulder
## (BatWing.wrist_offset), `spread` 0 folded … 1 splayed fingers.
func pose_wings(wrist: Vector3, spread: float) -> void:
	for wing in wings:
		wing.stroke(_mirror(SHOULDER, wing.side), wrist, spread)


## Where foot `i` (0 left, 1 right) tucks while flying, in world space.
func tucked_foot(i: int) -> Vector3:
	return to_global(_mirror(TUCKED_FOOT, _side(i)))


# One side: arm bones + wing, leg bones + foot target, and their IK settings.
func _build_side(i: int, body: int) -> void:
	var side := _side(i)
	var prefix := "L_" if side < 0.0 else "R_"
	var upper := _add_bone(prefix + "arm_upper", body, _mirror(SHOULDER, side))
	var lower := _add_bone(prefix + "arm_lower", upper, Vector3(side * UPPER_ARM, 0, 0))
	var wrist := _add_bone(prefix + "wrist", lower, Vector3(side * FOREARM * 0.99, 0, -0.03)) # a slight rest bend
	var thigh := _add_bone(prefix + "leg_upper", body, _mirror(HIP, side))
	var shin := _add_bone(prefix + "leg_lower", thigh, Vector3(0, -THIGH * 0.5, THIGH * 0.85))
	var foot := _add_bone(prefix + "foot", shin, Vector3(0, 0, SHIN))
	var wing := BatWing.new()
	wing.name = prefix + "Wing"
	wing.side = side
	wing.membrane_color = membrane_color
	add_child(wing)
	wing.setup(skeleton, upper, lower, wrist, foot, _mirror(MEMBRANE_HIP, side))
	wings.append(wing)
	var foot_target := Marker3D.new()
	foot_target.name = prefix + "FootTarget"
	add_child(foot_target)
	foot_target.position = _mirror(TUCKED_FOOT, side)
	feet.append(foot_target)
	var knee_pole := Marker3D.new()
	knee_pole.name = prefix + "KneePole"
	add_child(knee_pole)
	knee_pole.position = _mirror(HIP, side) + Vector3(side * 0.15, 0.0, 0.05) # knees bow outward
	_add_ik(i, upper, lower, wrist, wing.target, wing.pole)
	_add_ik(2 + i, thigh, shin, foot, foot_target, knee_pole)
	_limb(upper, Vector3(side * UPPER_ARM, 0, 0), 0.012)
	_limb(lower, Vector3(side * FOREARM, 0, -0.03), 0.01)
	_limb(thigh, Vector3(0, -THIGH * 0.5, THIGH * 0.85), 0.012)
	_limb(shin, Vector3(0, 0, SHIN), 0.009)


func _add_ik(setting: int, root: int, middle: int, end: int, target: Node3D, pole: Node3D) -> void:
	ik.set_root_bone_name(setting, skeleton.get_bone_name(root))
	ik.set_middle_bone_name(setting, skeleton.get_bone_name(middle))
	ik.set_end_bone_name(setting, skeleton.get_bone_name(end))
	ik.set_target_node(setting, ik.get_path_to(target))
	ik.set_pole_node(setting, ik.get_path_to(pole))


func _build_body() -> void:
	add_child(BatMeshes.blob(Vector3(0.1, 0.095, 0.135), Vector3(0, 0, 0.01), fur_color))
	add_child(BatMeshes.blob(Vector3(0.08, 0.075, 0.11), Vector3(0, -0.025, 0.0), belly_color))
	head = Node3D.new()
	head.name = "Head"
	head.position = Vector3(0, 0.04, -0.13)
	add_child(head)
	head.add_child(BatMeshes.blob(Vector3(0.085, 0.08, 0.08), Vector3.ZERO, fur_color))
	head.add_child(BatMeshes.blob(Vector3(0.03, 0.025, 0.02), Vector3(0, 0.07, 0.0), fur_color)) # tuft
	head.add_child(BatMeshes.blob(Vector3(0.018, 0.013, 0.012), Vector3(0, -0.005, -0.082), inner_ear_color)) # nose
	for side: float in [-1.0, 1.0]:
		var ear := Basis(Vector3.BACK, -side * 0.4) * Basis(Vector3.RIGHT, -0.15)
		head.add_child(BatMeshes.instance(BatMeshes.cone(0.042, 0.13), Transform3D(ear, Vector3(side * 0.045, 0.1, 0.0)), fur_color))
		head.add_child(BatMeshes.instance(BatMeshes.cone(0.028, 0.1), Transform3D(ear, Vector3(side * 0.045, 0.095, -0.012)), inner_ear_color))
		head.add_child(BatMeshes.blob(Vector3.ONE * 0.03, Vector3(side * 0.036, 0.015, -0.062), Color(0.98, 0.98, 0.95)))
		head.add_child(BatMeshes.blob(Vector3.ONE * 0.017, Vector3(side * 0.038, 0.017, -0.088), Color(0.05, 0.04, 0.06)))
		var fang := Transform3D(Basis(Vector3.RIGHT, PI), Vector3(side * 0.016, -0.045, -0.068))
		head.add_child(BatMeshes.instance(BatMeshes.cone(0.007, 0.022), fang, Color(1, 1, 0.95)))


func _limb(bone: int, along: Vector3, radius: float) -> void:
	var attach := BoneAttachment3D.new()
	skeleton.add_child(attach)
	attach.bone_idx = bone
	attach.add_child(BatMeshes.limb(along, radius, fur_color.darkened(0.25)))


func _add_bone(bone_name: String, parent: int, offset: Vector3) -> int:
	var bone := skeleton.add_bone(bone_name)
	skeleton.set_bone_parent(bone, parent)
	skeleton.set_bone_rest(bone, Transform3D(Basis.IDENTITY, offset))
	skeleton.set_bone_pose(bone, skeleton.get_bone_rest(bone))
	return bone


func _on_skeleton_updated() -> void:
	for wing in wings:
		wing.rebuild()


static func _side(i: int) -> float:
	return -1.0 if i == 0 else 1.0


static func _mirror(v: Vector3, side: float) -> Vector3:
	return Vector3(v.x * side, v.y, v.z)
