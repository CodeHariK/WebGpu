## Custom two-bone arm IK as a SkeletonModifier3D: your LimbChain maths, Godot's ordering.
##
## Godot runs modifiers right after the AnimationPlayer / AnimationTree has posed the skeleton, so
## this starts from the animated arm every frame and bends it toward `target`. Its `influence`
## (0..1, built into SkeletonModifier3D) blends animated ↔ IK: 0 = the clip's arm untouched,
## 1 = fully reaching. Fade it, never snap it.
##
## Each frame:
##   1. read the animated shoulder (upper arm), elbow (forearm) and wrist (hand) positions
##   2. LimbChain two-bone solve: root = shoulder, toward `target`, elbow toward `pole`; the bone
##      lengths come from the animated pose (twist bones like Rigify's ".001" are inside them)
##   3. turn the upper arm by the shortest rotation that puts the elbow where the solve says, then
##      the forearm by the one that puts the wrist there — the clip's twist along each bone stays
## The hand keeps its animated rotation relative to the forearm.
class_name ArmChainModifier
extends SkeletonModifier3D

@export var upper_bone := "DEF-upper_arm.L"
@export var forearm_bone := "DEF-forearm.L"
@export var hand_bone := "DEF-hand.L"
@export var grip_offset := 0.05 ## the grip (fist) sits this far past the wrist along the forearm (skeleton units)

var target := Vector3.ZERO ## world position the wrist reaches for
var pole := Vector3.ZERO ## world position the elbow bends toward
var hand_position := Vector3.ZERO ## world: the grip — just past where the wrist ended up (after the influence blend)
var animated_points := PackedVector3Array() ## world, for the debug view: the clip's shoulder, elbow, wrist
var solved_points := PackedVector3Array() ## world, for the debug view: the IK's shoulder, elbow, wrist (full influence)

var _chain := LimbChain.new(PackedFloat32Array([0.1, 0.1]), Vector3.ZERO, LimbChain.Solver.TWO_BONE)
var _bones := Vector3i(-1, -1, -1) ## upper, forearm, hand


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or not _find_bones(skeleton):
		return
	var to_skeleton := skeleton.global_transform.affine_inverse()
	var upper := skeleton.get_bone_global_pose(_bones.x)
	var forearm := skeleton.get_bone_global_pose(_bones.y)
	var animated_wrist := skeleton.get_bone_global_pose(_bones.z).origin
	var shoulder := upper.origin
	var elbow := forearm.origin
	_chain.lengths = PackedFloat32Array([shoulder.distance_to(elbow), elbow.distance_to(animated_wrist)])
	_chain.root = shoulder
	_chain.target = to_skeleton * target
	_chain.pole = to_skeleton * pole
	_chain.solve()
	var new_elbow := _chain.points[1]
	var new_wrist := _chain.points[2]
	# 3a. upper arm: elbow → new elbow
	upper.basis = Basis(_arc(elbow - shoulder, new_elbow - shoulder)) * upper.basis
	skeleton.set_bone_global_pose(_bones.x, upper)
	# 3b. forearm (moved with the upper arm): wrist → new wrist
	forearm = skeleton.get_bone_global_pose(_bones.y)
	var wrist := skeleton.get_bone_global_pose(_bones.z).origin
	forearm.basis = Basis(_arc(wrist - forearm.origin, new_wrist - forearm.origin)) * forearm.basis
	skeleton.set_bone_global_pose(_bones.y, forearm)
	var solved := skeleton.get_bone_global_pose(_bones.z).origin
	var animated_grip := animated_wrist + (animated_wrist - elbow).normalized() * grip_offset
	var solved_grip := solved + (solved - forearm.origin).normalized() * grip_offset
	hand_position = skeleton.global_transform * animated_grip.lerp(solved_grip, influence)
	var to_world := skeleton.global_transform
	animated_points = PackedVector3Array([to_world * shoulder, to_world * elbow, to_world * animated_wrist])
	solved_points = PackedVector3Array([to_world * shoulder, to_world * new_elbow, to_world * new_wrist])


# The shortest rotation turning direction `from` onto `to`.
static func _arc(
		from: Vector3,
		to: Vector3
) -> Quaternion:
	if from.length_squared() < 1e-10 or to.length_squared() < 1e-10:
		return Quaternion.IDENTITY
	var a := from.normalized()
	var b := to.normalized()
	if a.dot(b) > 0.99999:
		return Quaternion.IDENTITY
	return Quaternion(a, b)


func _find_bones(skeleton: Skeleton3D) -> bool:
	if _bones.x < 0:
		_bones = Vector3i(skeleton.find_bone(upper_bone), skeleton.find_bone(forearm_bone), skeleton.find_bone(hand_bone))
	return _bones.x >= 0 and _bones.y >= 0 and _bones.z >= 0
