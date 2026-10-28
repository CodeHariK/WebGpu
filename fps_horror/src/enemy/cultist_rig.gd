## The cultist hung on the rolling ball: its chest laid along the torso frame, four limbs reaching
## for the gait's limb points with IK, and the head turning to look at you. Ported from the IK half
## of LimbPlacementController (UpdateBodyPositions, UpdateHeadPosition) and Limb.SetIKTargets.
##
##   chest  spine.003 is the skeleton's root bone, so the whole model just takes the torso frame
##          (torso.gd) and the chest bone gets a fixed pose: spine along the way it's going,
##          face to the ground, crawling head first
##   limbs  one TwoBoneIK3D with 4 settings (upper arm, forearm, hand / thigh, shin, foot). Each
##          target is the limb's IK point (limb.gd: lifted a little off the surface); each pole
##          (where the elbow or knee points) is halfway between the limb and the ball, pushed
##          out: elbows out, up and back, knees out, up and forward (their magnet offsets)
##   hands  end_bone_aim.gd turns each hand and foot like theirs: on a surface it faces away
##   feet   from the body with its up along the surface normal (palm and sole flat on it);
##          in the air it points along the bend direction. The fixed turn from that frame to
##          the bone is their Hand_*_Target / Foot_*_Target node rotation (END_TURN).
##   head   their 2-bone head IK (neck spine.004 → head spine.005): a point 1.66 m in front of
##          the chest, swung toward `look_target` (the player) at head_turn per second, or
##          ahead when there isn't one. The neck aims at it (LookAtModifier3D) and the head takes
##          its rotation (end_bone_aim.gd); both fade out as the point goes behind the chest
##          (their IK interpolation)
##   tips   after the modifiers run, where each hand and foot really ended up is written back
##          to the gait (limb.tip), which re-steps limbs that couldn't reach
##
## The original used SkeletonIK3D (FABRIK, deprecated); TwoBoneIK3D solves each limb directly,
## which is cheaper, and the 4 limbs share one modifier. The ball itself is hidden.
extends Node3D

const Look := preload("res://src/look/character_look.gd")
const Gait := preload("res://src/enemy/limb_gait.gd")
const EndBoneAim := preload("res://src/enemy/end_bone_aim.gd")
const CULTIST := preload("res://assets/enemy/Cultist.gltf")

## [root, middle, end] bones per limb, in Gait.Limb order (left hand, right hand, left foot, right foot)
const CHAINS := [
	["upper_arm.L", "forearm.L", "hand.L"],
	["upper_arm.R", "forearm.R", "hand.R"],
	["thigh.L", "shin.L", "foot.L"],
	["thigh.R", "shin.R", "foot.R"],
]
## In the torso frame (X right, Y up, -Z forward): their LimbIKMagnetOffset
const POLE_OFFSET := [Vector3(-1, 1, 0.5), Vector3(1, 1, 0.5), Vector3(-1, 0.5, -1), Vector3(1, 0.5, -1)]
## Their Hand_L/R_Target, Foot_L/R_Target rotations inside the limb's aim frame (from CultistPrefab.tscn)
const END_TURN := [
	Basis(Vector3(0, -1, 0), Vector3(0, 0, -1), Vector3(1, 0, 0)),
	Basis(Vector3(0, 1, 0), Vector3(0, 0, -1), Vector3(-1, 0, 0)),
	Basis(Vector3(0, 0, -1), Vector3(0.766044, -0.642788, 0), Vector3(-0.642788, -0.766044, 0)),
	Basis(Vector3(0, 0, 1), Vector3(-0.766044, -0.642788, 0), Vector3(0.642788, -0.766044, 0)),
]
## The chest bone in the torso frame: spine (bone Y) forward, face (bone Z) down
const CHEST := Basis(Vector3(-1, 0, 0), Vector3(0, 0, -1), Vector3(0, -1, 0))
const NECK_BONE := "spine.004"
const HEAD_BONE := "spine.005"
const HEAD_REACH := 1.664  ## their HeadTarget: this far in front of the chest
const HEAD_TURN := Basis(Vector3.UP, PI)  ## their HeadTarget faces back toward the chest

@export var show_ball := false
@export var head_turn := 2.0  ## how fast the head swings toward what it looks at (per second)

var look_target: Node3D  ## what the head turns to; null: it looks where it's going

var gait: Gait
var body: RigidBody3D
var model: Node3D
var skeleton: Skeleton3D
var _chest := -1
var _ends: PackedInt32Array = []
var _targets: Array[Node3D] = []
var _poles: Array[Node3D] = []
var _limb_aim := EndBoneAim.new()
var _limb_frames: Array[Basis] = [Basis.IDENTITY, Basis.IDENTITY, Basis.IDENTITY, Basis.IDENTITY]
var _head_basis := Basis.IDENTITY
var _head_target := Node3D.new()
var _neck_look := LookAtModifier3D.new()
var _head_aim := EndBoneAim.new()


func _ready() -> void:
	gait = get_node("../Gait")
	body = get_node("../Body")
	body.get_node("Mesh").visible = show_ball
	model = CULTIST.instantiate()
	model.top_level = true
	Look.dress_cultist(model)
	add_child(model)
	skeleton = model.find_children("*", "Skeleton3D", true, false)[0]
	skeleton.modifier_callback_mode_process = Skeleton3D.MODIFIER_CALLBACK_MODE_PROCESS_PHYSICS
	skeleton.skeleton_updated.connect(_read_tips)
	_chest = skeleton.find_bone("spine.003")
	_head_basis = gait.torso.frame.basis
	_make_limb_ik()
	_make_head_look()
	_follow(0.0)


func _physics_process(delta: float) -> void:
	_follow(delta)


## Put the model on the torso frame, and every IK target, pole and aim on its limb.
func _follow(delta: float) -> void:
	var frame: Transform3D = gait.torso.frame
	model.global_transform = frame
	skeleton.set_bone_pose(_chest, Transform3D(CHEST, Vector3.ZERO))
	for limb in 4:
		var point = gait.limbs[limb]
		var target: Vector3 = point.ik_target(frame)
		_targets[limb].global_position = target
		_poles[limb].global_position = target.lerp(body.global_position, 0.5) + frame.basis * POLE_OFFSET[limb]
		_limb_aim.aims[limb] = _limb_frame(limb, point, target, frame) * END_TURN[limb]
	_follow_head(delta, frame)


## Their LimbIKContainer.look_at: on a surface, -Z away from the body (level) with Y along the
## surface normal; in the air, -Z along the bend direction with Y the torso's up.
func _limb_frame(limb: int, point, target: Vector3, frame: Transform3D) -> Basis:
	var facing: Vector3
	var up: Vector3
	if point.airborne:
		facing = (target - body.global_position) * 0.5 + frame.basis * POLE_OFFSET[limb]
		up = frame.basis.y
	else:
		facing = (target - body.global_position).normalized()
		facing.y = 0.0
		var normal: Vector3 = point.surface_normal()
		up = frame.basis.z if absf(facing.dot(normal)) > 0.99 or normal.is_zero_approx() else normal
	if facing.length() > 0.001 and absf(facing.normalized().dot(up.normalized())) < 0.999:
		_limb_frames[limb] = Basis.looking_at(facing, up)
	return _limb_frames[limb]


## Their UpdateHeadPosition: a frame at the chest swings toward the target; the head's aim point
## sits HEAD_REACH in front of it; the IK weight is how much the target is in front of the chest.
func _follow_head(delta: float, frame: Transform3D) -> void:
	var aim_at: Vector3 = look_target.global_position if look_target else frame.origin + gait.heading
	var to := aim_at - frame.origin
	var up := Vector3.UP if absf(frame.basis.y.dot(to)) > 0.99 else frame.basis.y  # unnormalised, like theirs
	if to.length() > 0.001 and absf(to.normalized().dot(up)) < 0.999:
		var facing := Basis.looking_at(to, up)
		var turned := _head_basis.get_rotation_quaternion().slerp(facing.get_rotation_quaternion(), minf(delta * head_turn, 1.0))
		_head_basis = Basis(turned)
	_head_target.global_position = frame.origin + _head_basis * Vector3(0.0, 0.0, -HEAD_REACH)
	_head_aim.aims[0] = _head_basis * HEAD_TURN
	var weight := clampf(((-frame.basis.z).dot(to) + 1.0) * 0.5, 0.0, 1.0)  # unnormalised, like theirs
	_neck_look.influence = weight
	_head_aim.influence = weight


## After the modifiers: where each hand and foot really is.
func _read_tips() -> void:
	for limb in 4:
		var point = gait.limbs[limb]
		point.tip = skeleton.global_transform * skeleton.get_bone_global_pose(_ends[limb]).origin
		point.tip_known = true


func _make_limb_ik() -> void:
	var ik := TwoBoneIK3D.new()
	ik.name = "LimbIK"
	skeleton.add_child(ik)
	ik.setting_count = 4
	for limb in 4:
		var target := _marker("Target%d" % limb)
		var pole := _marker("Pole%d" % limb)
		_targets.append(target)
		_poles.append(pole)
		ik.set_root_bone_name(limb, CHAINS[limb][0])
		ik.set_middle_bone_name(limb, CHAINS[limb][1])
		ik.set_end_bone_name(limb, CHAINS[limb][2])
		ik.set_target_node(limb, ik.get_path_to(target))
		ik.set_pole_node(limb, ik.get_path_to(pole))
		_ends.append(skeleton.find_bone(CHAINS[limb][2]))
	_limb_aim.name = "LimbAim"
	_limb_aim.bones = _ends
	_limb_aim.aims.assign(_limb_frames)
	skeleton.add_child(_limb_aim)  # modifiers run in child order: after the IK


func _make_head_look() -> void:
	_head_target.top_level = true
	add_child(_head_target)
	_neck_look.name = "NeckLook"
	skeleton.add_child(_neck_look)
	_neck_look.bone_name = NECK_BONE
	_neck_look.forward_axis = SkeletonModifier3D.BONE_AXIS_PLUS_Y  # the neck points along its bone
	_neck_look.primary_rotation_axis = Vector3.AXIS_Z
	_neck_look.use_secondary_rotation = true
	_neck_look.target_node = _neck_look.get_path_to(_head_target)
	_head_aim.name = "HeadAim"
	_head_aim.bones = PackedInt32Array([skeleton.find_bone(HEAD_BONE)])
	_head_aim.aims.assign([_head_basis * HEAD_TURN])
	skeleton.add_child(_head_aim)


## Dead: the head stops following anything and hangs in its rest pose.
func set_head_active(on: bool) -> void:
	_neck_look.active = on
	_head_aim.active = on


## The meshes of the model (the dissolve swaps their materials).
func meshes() -> Array[MeshInstance3D]:
	var out: Array[MeshInstance3D] = []
	out.assign(model.find_children("*", "MeshInstance3D", true, false))
	return out


func _marker(marker_name: String) -> Node3D:
	var node := Node3D.new()
	node.name = marker_name
	node.top_level = true
	add_child(node)
	return node
