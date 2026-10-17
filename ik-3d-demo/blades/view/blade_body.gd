## A simple stand-in body: capsule legs, a torso that leans and twists with BodyPose, a head, and
## two IK arms (two-bone LimbChains, shoulder → elbow → hand) that reach for their blades.
class_name BladeBody
extends PlayerBody

const HIP_HEIGHT := 0.95
const CHEST := 0.55 ## chest above the hips, along the torso
const SHOULDER_WIDTH := 0.3
const UPPER_ARM := 0.32
const FOREARM := 0.3

var _legs: MeshInstance3D
var _torso: MeshInstance3D
var _head: MeshInstance3D
var _arms: Array[LimbChain] = [] ## [left, right]
var _arm_renderer: LimbRenderer
var _hips := Vector3.ZERO
var _torso_basis := Basis.IDENTITY


func setup(owner: BladesDemo) -> void:
	super(owner)
	var skin := Color(0.85, 0.85, 0.8)
	_legs = DemoProps.mesh_instance(self, DemoProps.capsule(0.2, 0.95), skin)
	_torso = DemoProps.mesh_instance(self, DemoProps.capsule(0.26, 0.75), skin)
	var head := SphereMesh.new()
	head.radius = 0.17
	head.height = 0.34
	_head = DemoProps.mesh_instance(self, head, skin)
	_arm_renderer = LimbRenderer.new()
	add_child(_arm_renderer)
	for i in 2:
		var arm := LimbChain.new(PackedFloat32Array([UPPER_ARM, FOREARM]), Vector3.ZERO, LimbChain.Solver.TWO_BONE)
		arm.radius = 0.07
		arm.joint_radius = 0.08
		arm.taper = 0.8
		arm.color = skin
		_arms.append(arm)
		_arm_renderer.add(arm)


func reset() -> void:
	_hips = demo.player + Vector3.UP * HIP_HEIGHT
	_torso_basis = _basis()
	var hands: Array[Vector3] = [_rest_hand(-1.0), _rest_hand(1.0)]
	pose.reset(hands)
	for i in 2:
		_arms[i].points[2] = hands[i]


func grip(side: float) -> Vector3:
	return _arms[0 if side < 0.0 else 1].points[2]


func chest() -> Vector3:
	return _hips + _torso_basis.y * CHEST


func _draw_arm_debug(
		debug: BladeDebug,
		i: int
) -> void:
	var arm := _arms[i]
	draw_arm(debug, i, arm.points, arm.target, arm.pole)


func update(delta: float) -> void:
	pose.update(delta)
	_hips = demo.player + Vector3.UP * HIP_HEIGHT
	_torso_basis = _basis()
	for i in 2:
		var side := -1.0 if i == 0 else 1.0
		var shoulder := _shoulder(side)
		var arm := _arms[i]
		arm.root = shoulder
		arm.target = pose.reach(i, shoulder, UPPER_ARM + FOREARM, _rest_hand(side), delta)
		arm.pole = shoulder + Vector3.DOWN * 0.3 + _torso_basis.x * side * 0.45 - pose.chest_forward() * 0.2 # elbow out, down, back (of the chest)
		arm.solve()
	_legs.position = demo.player + Vector3.UP * 0.5
	_legs.basis = Basis(Vector3.UP, pose.heading)
	_torso.global_transform = Transform3D(_torso_basis, _hips + _torso_basis.y * 0.3)
	_head.position = _hips + _torso_basis.y * (CHEST + 0.35)
	_arm_renderer.draw()


# Torso frame: up tipped by the lean, forward = the body's heading turned by the twist.
func _basis() -> Basis:
	var up := (Vector3.UP + pose.lean).normalized()
	var forward := pose.forward().rotated(Vector3.UP, pose.twist)
	forward = (forward - up * forward.dot(up)).normalized()
	var back := -forward
	return Basis(up.cross(back), up, back)


func _shoulder(side: float) -> Vector3:
	return _hips + _torso_basis.y * CHEST + _torso_basis.x * side * SHOULDER_WIDTH


# The arm at rest: hanging a little forward of the shoulder.
func _rest_hand(side: float) -> Vector3:
	return _shoulder(side) + Vector3.DOWN * 0.45 + pose.forward() * 0.2
