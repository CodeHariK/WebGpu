## The player as Sophia (Godot's demo character, copied from the octo scene): her own clips play
## underneath, picked from how the player moves (Idle, Move, Jump, Fall via SophiaSkin), and two
## custom modifiers on her Skeleton3D layer the combat on top:
##   SpineLeanModifier   adds BodyPose's lean and twist to three spine bones
##   ArmChainModifier ×2 bends each arm toward its blade (LimbChain two-bone solve); its influence
##                       is BodyPose's arm weight, so the arm eases from the clip into the reach
##                       and back (no hard switch)
## Godot runs the modifiers in child order right after the AnimationTree: spine first, then arms
## (so the arms start from the leaned shoulders).
class_name SophiaBody
extends PlayerBody

const SKIN := preload("res://assets/character/sophia_skin/sophia_skin.tscn")
const SCALE := 1.6 ## she is a ~1.1 m chibi; this makes her about the capsule body's size
const ARM_BONES := [
	["DEF-upper_arm.L", "DEF-forearm.L", "DEF-hand.L"], # her left = the left blade (side −1)
	["DEF-upper_arm.R", "DEF-forearm.R", "DEF-hand.R"],
]
const RUN_SPEED := 0.5 ## m/s: faster than this plays the run clip

var _skin: SophiaSkin
var _skeleton: Skeleton3D
var _spine: SpineLeanModifier
var _arms: Array[ArmChainModifier] = []
var _last_player := Vector3.ZERO


func setup(owner: BladesDemo) -> void:
	super(owner)
	_skin = SKIN.instantiate()
	_skin.scale = Vector3.ONE * SCALE
	add_child(_skin)
	_skeleton = _skin.get_node("sophia/rig/Skeleton3D")
	_spine = SpineLeanModifier.new()
	_skeleton.add_child(_spine)
	for bones in ARM_BONES:
		var arm := ArmChainModifier.new()
		arm.upper_bone = bones[0]
		arm.forearm_bone = bones[1]
		arm.hand_bone = bones[2]
		arm.influence = 0.0
		_skeleton.add_child(arm)
		_arms.append(arm)


func reset() -> void:
	_last_player = demo.player
	_place()
	var hands: Array[Vector3] = [_bone_position(ARM_BONES[0][2]), _bone_position(ARM_BONES[1][2])]
	pose.reset(hands)


func grip(side: float) -> Vector3:
	var arm := _arms[0 if side < 0.0 else 1]
	if arm.hand_position == Vector3.ZERO: # before her first frame
		return demo.hand(side)
	return arm.hand_position


## Her upper spine (where the lean starts showing).
func chest() -> Vector3:
	return _bone_position("DEF-spine.003")


func _draw_arm_debug(
		debug: BladeDebug,
		i: int
) -> void:
	var arm := _arms[i]
	draw_animated_arm(debug, arm.animated_points)
	draw_arm(debug, i, arm.solved_points, arm.target, arm.pole)


func update(delta: float) -> void:
	pose.update(delta)
	_place()
	_pick_clip(delta)
	_spine.lean = pose.lean
	_spine.twist = pose.twist
	for i in 2:
		var side := -1.0 if i == 0 else 1.0
		var arm := _arms[i]
		var shoulder := _bone_position(arm.upper_bone)
		var length := shoulder.distance_to(_bone_position(arm.forearm_bone)) + _bone_position(arm.forearm_bone).distance_to(_bone_position(arm.hand_bone))
		arm.influence = pose.arm_weight[i]
		arm.target = pose.reach(i, shoulder, length, arm.hand_position, delta)
		arm.pole = shoulder + Vector3.DOWN * 0.3 + pose.right() * side * 0.4 - pose.forward() * 0.15 # elbow out, down, back


# Stand on the player's feet, facing the body's heading (her front is +Z; it turns with a spin).
func _place() -> void:
	global_position = demo.player
	rotation = Vector3(0.0, pose.heading, 0.0)


# Her clips, from how the player moves: in the air jump / fall, else run or idle.
func _pick_clip(delta: float) -> void:
	var moved := demo.player - _last_player
	_last_player = demo.player
	if delta <= 0.0:
		return
	if demo.player.y > 0.05 or demo.moves.rush.rushing:
		if demo.player_velocity.y > 0.0 or demo.moves.rush.rushing:
			_skin.jump()
		else:
			_skin.fall()
	elif Vector2(moved.x, moved.z).length() / delta > RUN_SPEED:
		_skin.move()
	else:
		_skin.idle()


func _bone_position(bone: String) -> Vector3:
	return _skeleton.global_transform * _skeleton.get_bone_global_pose(_skeleton.find_bone(bone)).origin
