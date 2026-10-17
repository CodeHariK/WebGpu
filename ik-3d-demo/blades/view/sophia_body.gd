## The player as Sophia (Godot's demo character, copied from the octo scene): her own clips play
## underneath, picked from how the player moves (Idle, Move, Jump, Fall via SophiaSkin), and two
## custom modifiers on her Skeleton3D layer the combat on top:
##   SpineLeanModifier   adds BodyPose's lean and twist to three spine bones
##   ArmChainModifier ×2 bends each arm toward its blade (LimbChain two-bone solve); its influence
##                       is BodyPose's arm weight, so the arm eases from the clip into the reach
##                       and back (no hard switch)
##   HeadLookModifier    turns neck and head toward BodyPose's look (between the two blades)
##   LegPlantModifier ×2 keeps her feet planted while she stands and turns (FootPlanter decides
##                       where each foot stands and when it steps); its influence is the
##                       planter's weight: 1 standing, 0 running / jumping / rushing (the clip's feet)
## Godot runs the modifiers in child order right after the AnimationTree: spine first, then arms
## (so the arms start from the leaned shoulders), then head, then legs.
class_name SophiaBody
extends PlayerBody

const SKIN := preload("res://assets/character/sophia_skin/sophia_skin.tscn")
const SCALE := 1.6 ## she is a ~1.1 m chibi; this makes her about the capsule body's size
const ARM_BONES := [
	["DEF-upper_arm.L", "DEF-forearm.L", "DEF-hand.L"], # her left = the left blade (side −1)
	["DEF-upper_arm.R", "DEF-forearm.R", "DEF-hand.R"],
]
const LEG_BONES := [
	["DEF-thigh.L", "DEF-shin.L", "DEF-foot.L"],
	["DEF-thigh.R", "DEF-shin.R", "DEF-foot.R"],
]
const KNEE_FORWARD := 0.6 ## m: the knee pole sits this far in front of the leg, along the foot
const RUN_SPEED := 0.5 ## m/s: faster than this plays the run clip

var _skin: SophiaSkin
var _skeleton: Skeleton3D
var _spine: SpineLeanModifier
var _arms: Array[ArmChainModifier] = []
var _head: HeadLookModifier
var _legs: Array[LegPlantModifier] = []
var _feet := FootPlanter.new()
var _last_player := Vector3.ZERO
var _moving := false ## the run clip is playing


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
	_head = HeadLookModifier.new()
	_skeleton.add_child(_head)
	for bones in LEG_BONES:
		var leg := LegPlantModifier.new()
		leg.upper_bone = bones[0]
		leg.forearm_bone = bones[1]
		leg.hand_bone = bones[2]
		leg.grip_offset = 0.0
		leg.influence = 0.0
		_skeleton.add_child(leg)
		_legs.append(leg)


func reset() -> void:
	_last_player = demo.player
	_place()
	var hands: Array[Vector3] = [_bone_position(ARM_BONES[0][2]), _bone_position(ARM_BONES[1][2])]
	pose.reset(hands)
	_feet.reset(_foot_homes(), pose.heading)
	for leg in _legs:
		leg.influence = 0.0


func grip(side: float) -> Vector3:
	var arm := _arms[0 if side < 0.0 else 1]
	if arm.hand_position == Vector3.ZERO: # before her first frame
		return demo.hand(side)
	return arm.hand_position


func head() -> Vector3:
	return _skeleton.global_transform * _skeleton.get_bone_global_pose(_skeleton.find_bone("DEF-spine.006")).origin + Vector3.UP * 0.15


## Her upper spine (where the lean starts showing).
func chest() -> Vector3:
	return _bone_position("DEF-spine.003")


## The body's debug view, plus her feet: orange cross = where a foot is planted, dim cross = its
## home (where the clip has it), white = the leg's IK (hip → knee → ankle), cyan = the knee pole,
## and a short arrow = the foot's heading.
func draw_debug(debug: BladeDebug) -> void:
	super(debug)
	if _head.face != Vector3.ZERO: # where her face really points (white), next to the look (yellow)
		debug.arrow(head(), head() + _head.face * 1.0, Color(1.0, 1.0, 1.0, 0.8))
	var homes := _foot_homes()
	for i in 2:
		var leg := _legs[i]
		var foot := _feet.feet[i]
		var points := leg.solved_points
		if points.size() == 3:
			debug.line(points[0], points[1], Color(1.0, 1.0, 1.0, 0.8))
			debug.line(points[1], points[2], Color(1.0, 1.0, 1.0, 0.8))
			debug.line(points[1], leg.pole, Color(DEBUG_POLE, 0.5))
		debug.cross(leg.pole, DEBUG_POLE, 0.04)
		debug.cross(foot.plant, Color(1.0, 0.55, 0.1), 0.07)
		debug.cross(homes[i], Color(1.0, 1.0, 1.0, 0.35), 0.04)
		var toes := Vector3(sin(foot.target_yaw), 0.0, cos(foot.target_yaw))
		debug.arrow(foot.target, foot.target + toes * 0.25, Color(1.0, 0.55, 0.1))
		var state := "step" if foot.step >= 0.0 else "planted"
		debug.label(foot.plant + Vector3.UP * 0.12, "%s %s" % ["L" if i == 0 else "R", state], Color(1.0, 0.7, 0.4))


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
		var shoulder := _shoulder(arm)
		var length := shoulder.distance_to(_bone_position(arm.forearm_bone)) + _bone_position(arm.forearm_bone).distance_to(_bone_position(arm.hand_bone))
		arm.influence = pose.arm_weight[i]
		arm.target = pose.reach(i, shoulder, length, arm.hand_position, delta)
		arm.pole = shoulder + Vector3.DOWN * 0.3 + pose.chest_right() * side * 0.4 - pose.chest_forward() * 0.15 # elbow out, down, back (of the chest)
	_head.look = pose.look
	_plant_feet(delta)


# The shoulder as it really is: after the spine lean and twist (the arm modifier's start, last
# frame, in skeleton space, placed with this frame's body transform). The bone pose read in
# _process is the clip's, before the modifiers.
func _shoulder(arm: ArmChainModifier) -> Vector3:
	if arm.animated_start == Vector3.ZERO:
		return _bone_position(arm.upper_bone)
	return _skeleton.global_transform * arm.animated_start


# Feet: planted while she stands (on the ground, not running or rushing), the clip's otherwise.
func _plant_feet(delta: float) -> void:
	if delta <= 0.0: # hit-stop: hold
		return
	var standing: bool = demo.player.y <= 0.05 and not _moving and not demo.moves.rush.rushing
	_feet.update(delta, _foot_homes(), pose.heading, standing)
	for i in 2:
		var leg := _legs[i]
		var foot := _feet.feet[i]
		var hip := _bone_position(leg.upper_bone)
		var toes := Vector3(sin(foot.target_yaw), 0.0, cos(foot.target_yaw))
		leg.influence = _feet.weight
		leg.target = foot.target
		leg.pole = hip.lerp(foot.target, 0.5) + toes * KNEE_FORWARD # knee over the toes
		leg.foot_twist = foot.target_yaw - pose.heading


# Where the clip has each ankle right now: last frame's clip ankle (skeleton space), placed with
# this frame's body transform (so it turns with her immediately).
func _foot_homes() -> Array[Vector3]:
	var homes: Array[Vector3] = []
	for i in 2:
		var leg := _legs[i]
		homes.append(_skeleton.global_transform * leg.animated_end if leg.animated_end != Vector3.ZERO else _bone_position(LEG_BONES[i][2]))
	return homes


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
		_moving = true
	else:
		_skin.idle()
		_moving = false


func _bone_position(bone: String) -> Vector3:
	return _skeleton.global_transform * _skeleton.get_bone_global_pose(_skeleton.find_bone(bone)).origin
