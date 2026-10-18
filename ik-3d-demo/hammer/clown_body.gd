## KingClown's stand-in body: Sophia (her clips underneath) with the blades demo's IK modifiers on
## top, holding a giant clown hammer in both hands.
##
## Each frame the demo hands it a SmashAttack.Pose (or a plain standing / walking pose) and it:
##   stands on the pose's feet, facing its forward
##   squashes / stretches (height × squash, width × 1/√squash — the volume stays about the same)
##   leans the spine (SpineLeanModifier) forward / back
##   turns the chest (twist) — away while the hammer is cocked, through on the smash
##   places the hammer: its right hand where the attack's hands are (a short arc round the
##     shoulders), the handle on from there along the hammer's angle, in the swing plane `side` to
##     the right; its head's face looks along the swing (so it lands face-first)
##   puts both hands on the handle (ArmChainModifier, full influence): the right at the handle's
##     end, the left grip_gap further up it; elbows out and down
##   looks at the robot's head (HeadLookModifier)
##   plays her clip: jump while rising, fall while coming down, run / idle on the ground
class_name ClownBody
extends HammerBody

const SKIN := preload("res://assets/character/sophia_skin/sophia_skin.tscn")
const SCALE := 1.6
const ARM_BONES := [
	["DEF-upper_arm.L", "DEF-forearm.L", "DEF-hand.L"],
	["DEF-upper_arm.R", "DEF-forearm.R", "DEF-hand.R"],
]
var _skin: SophiaSkin
var _skeleton: Skeleton3D
var _spine := SpineLeanModifier.new()
var _arms: Array[ArmChainModifier] = []
var _head := HeadLookModifier.new()


func _init() -> void:
	key = "sophia"


func _ready() -> void:
	_skin = SKIN.instantiate()
	_skin.scale = Vector3.ONE * SCALE
	add_child(_skin)
	_skeleton = _skin.get_node("sophia/rig/Skeleton3D")
	_skeleton.add_child(_spine)
	for bones in ARM_BONES:
		var arm := ArmChainModifier.new()
		arm.upper_bone = bones[0]
		arm.forearm_bone = bones[1]
		arm.hand_bone = bones[2]
		arm.influence = 1.0
		_skeleton.add_child(arm)
		_arms.append(arm)
	_skeleton.add_child(_head)
	_build_hammer()


## Her shoulders' height above her feet in the idle pose (the attack is worked out from it).
func shoulder_height() -> float:
	var left := _bone_position(ARM_BONES[0][0])
	var right := _bone_position(ARM_BONES[1][0])
	return (left + right).y * 0.5 - global_position.y


## Pose the body and the hammer from `pose` (the attack knows the hammer's reach and the swing plane).
func apply(
		pose: SmashAttack.Pose,
		attack: SmashAttack,
		look_at_point: Vector3,
		moving: bool
) -> void:
	global_position = pose.feet
	rotation = Vector3(0.0, atan2(pose.forward.x, pose.forward.z), 0.0)
	var width := 1.0 / sqrt(maxf(pose.squash, 0.2))
	_skin.scale = Vector3(width, pose.squash, width) * SCALE
	_spine.lean = pose.forward * pose.lean # a negative lean tips the other way (back)
	_spine.twist = pose.twist
	var right := SmashAttack.right_of(pose.forward)
	_hold_hammer(pose, attack)
	for i in 2:
		var arm := _arms[i]
		var side := -1.0 if i == 0 else 1.0
		var shoulder := _bone_position(arm.upper_bone)
		arm.target = hand_targets[i]
		arm.pole = shoulder + Vector3.DOWN * 0.35 + right * side * 0.45 - pose.forward * 0.1
	var to_look := look_at_point - (shoulders + Vector3.UP * 0.4)
	_head.look = to_look.normalized() if to_look.length() > 0.1 else pose.forward
	_play_clip(pose, moving)


## The arms as the IK solved them (shoulder → elbow → wrist), for the debug view.
func arm_points(i: int) -> PackedVector3Array:
	return _arms[i].solved_points


func _play_clip(
		pose: SmashAttack.Pose,
		moving: bool
) -> void:
	if pose.airborne:
		if pose.rising:
			_skin.jump()
		else:
			_skin.fall()
	elif moving:
		_skin.move()
	else:
		_skin.idle()


func _bone_position(bone: String) -> Vector3:
	return _skeleton.global_transform * _skeleton.get_bone_global_pose(_skeleton.find_bone(bone)).origin
