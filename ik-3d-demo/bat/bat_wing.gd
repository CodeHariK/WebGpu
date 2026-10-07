## One bat wing. The arm is BatRig's two-bone LimbChain (shoulder → elbow → wrist); `stroke()`
## moves its wrist target round the flap loop and its elbow pole stays behind the arm, so the
## elbow bends back like a real bat's.
##
## The three long fingers are one-segment AIM chains (drawn by the rig's LimbRenderer): after the
## arm has solved, they fan out from the wrist in the wing's plane (the plane of the forearm and
## "backward"), folded back along the forearm when `spread` is 0 and splayed wide at 1. The
## membrane is a fan mesh rebuilt every frame from the shoulder, elbow, wrist, finger tips, ankle
## and hip, sagging in between the finger tips for the scalloped trailing edge. Works in world
## space (chain points are world space); BatRig.solve() calls rebuild().
class_name BatWing
extends Node3D

const FINGER_LENGTHS: Array[float] = [0.19, 0.24, 0.2]
const FINGER_SPREAD: Array[float] = [0.3, 1.05, 1.75] ## radians back from the forearm, spread
const FINGER_FOLDED: Array[float] = [2.6, 2.75, 2.9] ## …and folded (back along the forearm)
const FINGER_RADIUS := 0.0055
const SCALLOP := 0.25 ## how far the trailing edge sags between tips (share toward the middle)
const REACH := Vector2(0.2, 0.335) ## shoulder → wrist, folded / fully out (arm is 0.35 long)

var side := 1.0 ## +1 right wing (+X), −1 left
var membrane_color := Color(0.38, 0.23, 0.42)
var bone_color := Color(0.25, 0.16, 0.27)

var target: Marker3D ## wrist target (body space)
var pole: Marker3D ## elbow pole (body space)

var _arm: LimbChain
var _leg: LimbChain
var _hip := Vector3.ZERO ## body space
var _spread := 1.0
var _fingers: Array[LimbChain] = []
var _membrane := ImmediateMesh.new()


## Where the wrist goes (right-wing coordinates, relative to the shoulder) for an extension
## 0 (folded in, swept back) … 1 (straight out) and an elevation (radians, + up).
static func wrist_offset(extension: float, elevation: float) -> Vector3:
	var reach := lerpf(REACH.x, REACH.y, extension)
	return Vector3(cos(elevation) * reach, sin(elevation) * reach, lerpf(0.08, -0.03, extension))


## Hook the wing to its arm and leg chains. `hip` is where the membrane meets the body (body
## space); the fingers are added to `renderer`.
func setup(arm: LimbChain, leg: LimbChain, hip: Vector3, renderer: LimbRenderer) -> void:
	_arm = arm
	_leg = leg
	_hip = hip
	target = Marker3D.new()
	target.name = "WristTarget"
	add_child(target)
	pole = Marker3D.new()
	pole.name = "ElbowPole"
	add_child(pole)
	for length in FINGER_LENGTHS:
		var finger := LimbChain.new(PackedFloat32Array([length]), Vector3.ZERO, LimbChain.Solver.AIM)
		finger.radius = FINGER_RADIUS
		finger.joint_radius = FINGER_RADIUS
		finger.color = bone_color
		renderer.add(finger)
		_fingers.append(finger)
	var membrane := MeshInstance3D.new()
	membrane.name = "Membrane"
	membrane.mesh = _membrane
	membrane.material_override = BatMeshes.material(membrane_color, 0.0, true)
	add_child(membrane)
	membrane.top_level = true # vertices are world space
	membrane.global_transform = Transform3D.IDENTITY


## Aim the wing: `wrist` in right-wing coordinates relative to `shoulder` (see wrist_offset),
## `spread` 0 folded … 1 splayed fingers.
func stroke(shoulder: Vector3, wrist: Vector3, spread: float) -> void:
	var mirrored := Vector3(wrist.x * side, wrist.y, wrist.z)
	target.position = shoulder + mirrored
	pole.position = shoulder + mirrored * 0.5 + Vector3(0.0, -0.02, 0.3)
	_spread = clampf(spread, 0.0, 1.0)


## Place the fingers and rebuild the membrane from the solved arm and leg.
func rebuild() -> void:
	var shoulder := _arm.points[0]
	var elbow := _arm.points[1]
	var wrist := _arm.points[2]
	var ankle := _leg.points[2]
	var along := wrist - elbow
	if along.length() < 0.001:
		return
	along = along.normalized()
	var backward := global_basis.z.normalized() # the body's tail-ward direction
	var back := backward - along * along.dot(backward) # "backward", square to the forearm
	back = back.normalized() if back.length() > 0.01 else -global_basis.y.normalized()
	var tips: Array[Vector3] = []
	for i in _fingers.size():
		var angle := lerpf(FINGER_FOLDED[i], FINGER_SPREAD[i], _spread)
		var finger := _fingers[i]
		finger.root = wrist
		finger.target = wrist + (along * cos(angle) + back * sin(angle)) * FINGER_LENGTHS[i]
		finger.solve()
		tips.append(finger.tip())
	_build_membrane(shoulder, elbow, wrist, tips, ankle, to_global(_hip), along.cross(back) * -side)


# `up` is the wing's top side (its plane normal); every triangle faces it.
func _build_membrane(shoulder: Vector3, elbow: Vector3, wrist: Vector3, tips: Array[Vector3], ankle: Vector3, hip: Vector3, up: Vector3) -> void:
	var centre := (shoulder + wrist + tips[2] + ankle) * 0.25
	var edge := PackedVector3Array([shoulder, elbow, wrist, tips[0]])
	var trailing: Array[Vector3] = [tips[0], tips[1], tips[2], ankle]
	for k in trailing.size() - 1: # scallops: a sagging midpoint between each pair of tips
		edge.append(((trailing[k] + trailing[k + 1]) * 0.5).lerp(centre, SCALLOP))
		edge.append(trailing[k + 1])
	edge.append(hip)
	_membrane.clear_surfaces()
	_membrane.surface_begin(Mesh.PRIMITIVE_TRIANGLES)
	for k in edge.size():
		var a := edge[k]
		var b := edge[(k + 1) % edge.size()]
		var normal := (a - centre).cross(b - centre)
		if normal.length_squared() < 1e-12:
			continue
		if normal.dot(up) < 0.0: # face up, winding to match (Godot fronts are clockwise)
			var swap := a
			a = b
			b = swap
			normal = -normal
		_membrane.surface_set_normal(normal.normalized())
		_membrane.surface_add_vertex(centre)
		_membrane.surface_add_vertex(b)
		_membrane.surface_add_vertex(a)
	_membrane.surface_end()
