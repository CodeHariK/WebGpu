## The cultist hung on the rolling ball: its chest laid along the torso frame, four limbs reaching
## for the gait's limb points with IK, and the head turning to look at you. Ported from the IK half
## of LimbPlacementController and Limb.SetIKTargets in the original.
##
##   chest  spine.003 is the skeleton's root bone, so the whole model just takes the torso frame
##          (torso.gd) and the chest bone gets a fixed pose: spine along the way it's going,
##          face to the ground, crawling head first
##   limbs  one TwoBoneIK3D with 4 settings (upper arm, forearm, hand / thigh, shin, foot). Each
##          target sits a little above its limb point (so hands and feet rest on the surface, not
##          in it); each pole (where the elbow or knee points) is halfway between the limb and
##          the ball, pushed out to the side: elbows out, up and back, knees out, up and forward
##          (their magnet offsets)
##   head   a LookAtModifier3D on the neck (spine.004), aimed at `look_target` (the player), or
##          ahead when there isn't one; eased and limited to 70° so it doesn't spin round
##
## The original used SkeletonIK3D (FABRIK, deprecated); TwoBoneIK3D solves each limb directly,
## which is cheaper, and the 4 limbs share one modifier. The ball itself is hidden.
extends Node3D

const Look := preload("res://src/look/character_look.gd")
const Gait := preload("res://src/enemy/limb_gait.gd")
const CULTIST := preload("res://assets/enemy/Cultist.gltf")

## [root, middle, end] bones per limb, in Gait.Limb order (left hand, right hand, left foot, right foot)
const CHAINS := [
	["upper_arm.L", "forearm.L", "hand.L"],
	["upper_arm.R", "forearm.R", "hand.R"],
	["thigh.L", "shin.L", "foot.L"],
	["thigh.R", "shin.R", "foot.R"],
]
## In the torso frame (X right, Y up, -Z forward)
const TARGET_LIFT := [Vector3(0, 0.1, 0), Vector3(0, 0.1, 0), Vector3(0, 0.15, 0), Vector3(0, 0.15, 0)]
const POLE_OFFSET := [Vector3(-1, 1, 0.5), Vector3(1, 1, 0.5), Vector3(-1, 0.5, -1), Vector3(1, 0.5, -1)]
## The chest bone in the torso frame: spine (bone Y) forward, face (bone Z) down
const CHEST := Basis(Vector3(-1, 0, 0), Vector3(0, 0, -1), Vector3(0, -1, 0))
const HEAD_BONE := "spine.004"

@export var show_ball := false
@export var head_limit := deg_to_rad(70.0)

var look_target: Node3D  ## what the head turns to; null: it looks where it's going

var gait: Gait
var body: RigidBody3D
var model: Node3D
var skeleton: Skeleton3D
var _chest := -1
var _targets: Array[Node3D] = []
var _poles: Array[Node3D] = []
var _head_target := Node3D.new()
var _look: LookAtModifier3D


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
	_chest = skeleton.find_bone("spine.003")
	_make_limb_ik()
	_make_head_look()
	_follow()


func _physics_process(_delta: float) -> void:
	_follow()


## Put the model on the torso frame and every IK target and pole on its limb.
func _follow() -> void:
	var frame: Transform3D = gait.torso.frame
	model.global_transform = frame
	skeleton.set_bone_pose(_chest, Transform3D(CHEST, Vector3.ZERO))
	for limb in 4:
		var point: Vector3 = gait.limbs[limb].position
		_targets[limb].global_position = point + frame.basis * TARGET_LIFT[limb]
		_poles[limb].global_position = point.lerp(body.global_position, 0.5) + frame.basis * POLE_OFFSET[limb]
	_head_target.global_position = look_target.global_position if look_target else frame.origin - frame.basis.z * 3.0


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


func _make_head_look() -> void:
	_head_target.top_level = true
	add_child(_head_target)
	var look := LookAtModifier3D.new()
	_look = look
	look.name = "HeadLook"
	skeleton.add_child(look)  # after the limb IK: modifiers run in child order
	look.bone_name = HEAD_BONE
	look.forward_axis = SkeletonModifier3D.BONE_AXIS_PLUS_Z
	look.primary_rotation_axis = Vector3.AXIS_Y
	look.use_secondary_rotation = true
	look.duration = 0.3
	look.use_angle_limitation = true
	look.symmetry_limitation = true
	look.primary_limit_angle = head_limit
	look.secondary_limit_angle = head_limit
	look.target_node = look.get_path_to(_head_target)


## Dead: the head stops following anything and hangs in its rest pose.
func set_head_active(on: bool) -> void:
	_look.active = on


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
