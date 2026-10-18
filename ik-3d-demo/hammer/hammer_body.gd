## What every hammer-holding body shares (KingClownBody, ClownBody = Sophia): the giant candy
## hammer, and where the attack puts it and the hands. Each frame the demo calls apply() with a
## SmashAttack.Pose; a body poses itself and calls _hold_hammer() for the hammer and its grips.
##
## The hammer's sizes and grips are the attack's (tuning panel, "Hammer" tab), applied every frame:
## the right hand sits where the attack's hands are (a short arc round the shoulders), the handle
## sticks out handle_end past it and runs on along the hammer's angle to the head; the left hand
## grips grip_gap further up the handle; the head's barrel axis lies along the swing, so it lands
## face-first.
##
## A body can have its own sliders (tunables(), the panel's "Body" tab) under its own key.
class_name HammerBody
extends Node3D

const RINGS := 4 ## red candy stripes round the handle

var key := "body" ## its section in the saved tuning
var hammer := Node3D.new() ## the hammer (its origin at the head's centre, +Z down the handle to the hands)
var head_point := Vector3.ZERO ## world: the hammer head's centre now
var shoulders := Vector3.ZERO ## world: the swing's centre now (the attack's shoulders point)
var hand_targets: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO] ## world: where each hand grips (left, right)

var _handle: MeshInstance3D
var _rings: Array[MeshInstance3D] = []
var _barrel: Node3D


## The shoulders' height above the feet, standing (the attack is worked out from it).
func shoulder_height() -> float:
	return 1.3


## Whether shoulder_height() can be read every frame (a code-built body), or only once, standing
## (a skeleton whose clips move the shoulders).
func live_shoulders() -> bool:
	return false


## The body's own sliders: [var name, min, max, step], or a String: a heading.
func tunables() -> Array:
	return []


## Pose the body and the hammer from `pose`; look at `look_at_point`; `moving`: walking.
func apply(
		_pose: SmashAttack.Pose,
		_attack: SmashAttack,
		_look_at_point: Vector3,
		_moving: bool
) -> void:
	pass


## Arm i (0 left, 1 right) as solved (shoulder → elbow → wrist), for the debug view.
func arm_points(_i: int) -> PackedVector3Array:
	return PackedVector3Array()


# Put the hammer where `pose` says (sized as the attack says) and set the grips; its direction.
func _hold_hammer(
		pose: SmashAttack.Pose,
		attack: SmashAttack
) -> Vector3:
	shoulders = attack.shoulders(pose)
	var along := SmashAttack.direction(pose.forward, pose.angle)
	var swing := SmashAttack.swing_of(pose.forward, pose.angle)
	head_point = attack.hammer_point(pose, attack.reach)
	_place_hammer(head_point, along, swing)
	_size_hammer(attack)
	var grip := attack.hands(pose)
	hand_targets = [grip + along * attack.grip_gap, grip] # the left hand further up the handle
	return along


# The hammer: the head's centre at `head`, the handle back along −`along`, the barrel's axis along
# the swing (its faces hit what it swings into).
func _place_hammer(
		head: Vector3,
		along: Vector3,
		swing: Vector3
) -> void:
	var z := -along # +Z runs from the head down the handle
	var y := swing.normalized()
	var x := y.cross(z).normalized()
	hammer.global_transform = Transform3D(Basis(x, y, z), head)


# The attack's sizes: the handle from the head's centre back past the right hand, its thickness,
# the stripes spread along it beyond the barrel, the barrel's radius and length.
func _size_hammer(attack: SmashAttack) -> void:
	var length := attack.reach - attack.hand_radius + attack.handle_end
	var thick := attack.handle_radius / 0.065
	_handle.scale = Vector3(thick, length, thick)
	_handle.position = Vector3(0, 0, length * 0.5)
	var start := attack.head_radius + 0.1
	for k in RINGS:
		_rings[k].position = Vector3(0, 0, start + (length - start - 0.25) * (k + 0.5) / RINGS)
		_rings[k].scale = Vector3(thick, 1.0, thick)
	_barrel.scale = Vector3(attack.head_radius, attack.head_length, attack.head_radius)


func _build_hammer() -> void:
	add_child(hammer)
	hammer.top_level = true
	_handle = _mesh(_cylinder(0.065, 1.0), Color(0.95, 0.95, 0.9)) # unit sized: _size_hammer scales it
	_handle.rotation = Vector3(PI * 0.5, 0.0, 0.0) # the cylinder's axis (Y) along +Z
	for k in RINGS:
		var ring := _mesh(_cylinder(0.072, 0.1), Color(0.9, 0.15, 0.2))
		ring.rotation = _handle.rotation
		_rings.append(ring)
	_barrel = Node3D.new() # a unit barrel (radius 1, length 1, axis Y = the swing), scaled to size
	hammer.add_child(_barrel)
	_mesh(_cylinder(1.0, 1.0), Color(0.9, 0.15, 0.2), _barrel)
	for end in [-1.0, 1.0]:
		var cap := _mesh(_cylinder(1.06, 0.1), Color(1.0, 0.8, 0.15), _barrel)
		cap.position = Vector3(0, end * 0.5, 0)
	_mesh(_cylinder(1.03, 0.14), Color(1.0, 1.0, 1.0), _barrel)


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
