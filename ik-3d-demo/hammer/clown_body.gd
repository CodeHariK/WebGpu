## KingClown's stand-in body: Sophia (her clips underneath) with the blades demo's IK modifiers on
## top, holding a giant clown hammer in both hands.
##
## Each frame the demo hands it a SmashAttack.Pose (or a plain standing / walking pose) and it:
##   stands on the pose's feet, facing its forward
##   squashes / stretches (height × squash, width × 1/√squash — the volume stays about the same)
##   leans the spine (SpineLeanModifier) forward / back
##   places the hammer: along the pose's angle from the shoulders, in the swing plane `side` to the
##     right; its head's face looks along the swing (so it lands face-first)
##   puts both hands on the handle (ArmChainModifier, full influence): the right at the handle's
##     end, the left further up it; elbows out and down
##   looks at the robot's head (HeadLookModifier)
##   plays her clip: jump while rising, fall while coming down, run / idle on the ground
class_name ClownBody
extends Node3D

const SKIN := preload("res://assets/character/sophia_skin/sophia_skin.tscn")
const SCALE := 1.6
const ARM_BONES := [
	["DEF-upper_arm.L", "DEF-forearm.L", "DEF-hand.L"],
	["DEF-upper_arm.R", "DEF-forearm.R", "DEF-hand.R"],
]
const GRIP_NEAR := 0.42 ## m from the shoulders along the hammer: the right hand (the handle's end)
const GRIP_FAR := 0.72 ## … the left hand
const HANDLE_END := 0.3 ## the handle starts here (a little past the right hand)
const HAMMER_HEAD := Vector2(0.36, 0.7) ## the head's radius and length (a big barrel)

var hammer := Node3D.new() ## the hammer (its origin at the head's centre, −Z along the handle to the hands)
var head_point := Vector3.ZERO ## world: the hammer head's centre now
var shoulders := Vector3.ZERO ## world: between the shoulders now (the swing's centre)
var hand_targets: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO]

var _skin: SophiaSkin
var _skeleton: Skeleton3D
var _spine := SpineLeanModifier.new()
var _arms: Array[ArmChainModifier] = []
var _head := HeadLookModifier.new()
var _handle: MeshInstance3D
var _barrel: Node3D


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
	var right := SmashAttack.right_of(pose.forward)
	shoulders = attack.shoulders(pose)
	var along := SmashAttack.direction(pose.forward, pose.angle)
	var swing := SmashAttack.swing_of(pose.forward, pose.angle)
	head_point = attack.hammer_point(pose, attack.reach)
	_place_hammer(head_point, along, swing)
	_size_handle(attack.reach - HANDLE_END)
	hand_targets = [attack.hammer_point(pose, GRIP_FAR), attack.hammer_point(pose, GRIP_NEAR)] # left higher up the handle
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


# The hammer: the head's centre at `head`, the handle back along −`along`, the barrel's axis along
# the swing (its faces hit what it swings into).
func _place_hammer(
		head: Vector3,
		along: Vector3,
		swing: Vector3
) -> void:
	var z := -along # −Z (Godot's forward) … here +Z runs from the head down the handle
	var y := swing.normalized()
	var x := y.cross(z).normalized()
	hammer.global_transform = Transform3D(Basis(x, y, z), head)


# The handle runs from the head's centre `length` back toward the hands (the reach is a slider).
func _size_handle(length: float) -> void:
	_handle.scale = Vector3(1.0, length, 1.0)
	_handle.position = Vector3(0, 0, length * 0.5)


func _build_hammer() -> void:
	add_child(hammer)
	hammer.top_level = true
	_handle = _mesh(_cylinder(0.045, 1.0), Color(0.95, 0.95, 0.9)) # 1 m long, sized each frame
	_handle.rotation = Vector3(PI * 0.5, 0.0, 0.0) # the cylinder's axis (Y) along +Z
	for k in 4: # red rings round the handle: candy stripes
		var ring := _mesh(_cylinder(0.05, 0.08), Color(0.9, 0.15, 0.2))
		ring.rotation = _handle.rotation
		ring.position = Vector3(0, 0, 0.35 + k * 0.22)
	_barrel = Node3D.new()
	hammer.add_child(_barrel)
	var barrel := _mesh(_cylinder(HAMMER_HEAD.x, HAMMER_HEAD.y), Color(0.9, 0.15, 0.2), _barrel) # axis Y = the swing
	for end in [-1.0, 1.0]:
		var cap := _mesh(_cylinder(HAMMER_HEAD.x * 1.06, 0.1), Color(1.0, 0.8, 0.15), _barrel)
		cap.position = Vector3(0, end * HAMMER_HEAD.y * 0.5, 0)
	var band := _mesh(_cylinder(HAMMER_HEAD.x * 1.03, 0.12), Color(1.0, 1.0, 1.0), _barrel)
	band.position = Vector3.ZERO
	barrel.name = "Barrel"


func _mesh(
		mesh: Mesh,
		color: Color,
		parent: Node3D = null
) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	instance.material_override = material
	(parent if parent != null else hammer).add_child(instance)
	return instance


static func _cylinder(
		radius: float,
		height: float
) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	return mesh


func _bone_position(bone: String) -> Vector3:
	return _skeleton.global_transform * _skeleton.get_bone_global_pose(_skeleton.find_bone(bone)).origin
