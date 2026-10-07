## One bat wing. The arm is a two-bone IK chain (upper arm → forearm → wrist) on BatRig's
## skeleton; `stroke()` moves its wrist target round the flap loop and its elbow pole stays behind
## the arm, so the elbow bends back like a real bat's.
##
## The three long fingers are not bones: after the IK has solved, they fan out from the wrist in
## the wing's plane (the plane of the forearm and "backward"), folded back along the forearm when
## `spread` is 0 and splayed wide at 1. The membrane is a fan mesh rebuilt every frame from the
## shoulder, elbow, wrist, finger tips, ankle and hip, sagging in between the finger tips for the
## scalloped trailing edge. BatRig calls rebuild() from Skeleton3D.skeleton_updated, so membrane
## and bones never lag a frame apart.
class_name BatWing
extends Node3D

const FINGER_LENGTHS: Array[float] = [0.19, 0.24, 0.2]
const FINGER_SPREAD: Array[float] = [0.3, 1.05, 1.75] ## radians back from the forearm, spread
const FINGER_FOLDED: Array[float] = [2.6, 2.75, 2.9] ## …and folded (back along the forearm)
const SCALLOP := 0.25 ## how far the trailing edge sags between tips (share toward the middle)
const REACH := Vector2(0.2, 0.335) ## shoulder → wrist, folded / fully out (arm is 0.35 long)

var side := 1.0 ## +1 right wing (+X), −1 left
var membrane_color := Color(0.38, 0.23, 0.42)
var bone_color := Color(0.25, 0.16, 0.27)

var target: Marker3D ## wrist IK target (body space)
var pole: Marker3D ## elbow IK pole (body space)

var _skeleton: Skeleton3D
var _shoulder_bone := -1
var _elbow_bone := -1
var _wrist_bone := -1
var _ankle_bone := -1
var _hip := Vector3.ZERO
var _spread := 1.0
var _fingers: Array[MeshInstance3D] = []
var _membrane := ImmediateMesh.new()


## Where the wrist goes (right-wing coordinates, relative to the shoulder) for an extension
## 0 (folded in, swept back) … 1 (straight out) and an elevation (radians, + up).
static func wrist_offset(extension: float, elevation: float) -> Vector3:
	var reach := lerpf(REACH.x, REACH.y, extension)
	return Vector3(cos(elevation) * reach, sin(elevation) * reach, lerpf(0.08, -0.03, extension))


## Hook the wing to its bones. `hip` is where the membrane meets the body (body space).
func setup(skeleton: Skeleton3D, shoulder_bone: int, elbow_bone: int, wrist_bone: int, ankle_bone: int, hip: Vector3) -> void:
	_skeleton = skeleton
	_shoulder_bone = shoulder_bone
	_elbow_bone = elbow_bone
	_wrist_bone = wrist_bone
	_ankle_bone = ankle_bone
	_hip = hip
	target = Marker3D.new()
	target.name = "WristTarget"
	add_child(target)
	pole = Marker3D.new()
	pole.name = "ElbowPole"
	add_child(pole)
	var finger_mesh := CylinderMesh.new() # unit length, stretched per finger every frame
	finger_mesh.top_radius = 0.004
	finger_mesh.bottom_radius = 0.007
	finger_mesh.height = 1.0
	finger_mesh.radial_segments = 6
	finger_mesh.rings = 1
	for length in FINGER_LENGTHS:
		var finger := BatMeshes.instance(finger_mesh, Transform3D.IDENTITY, bone_color)
		add_child(finger)
		_fingers.append(finger)
	var membrane := MeshInstance3D.new()
	membrane.name = "Membrane"
	membrane.mesh = _membrane
	membrane.material_override = BatMeshes.material(membrane_color, 0.0, true)
	add_child(membrane)


## Aim the wing: `wrist` in right-wing coordinates relative to `shoulder` (see wrist_offset),
## `spread` 0 folded … 1 splayed fingers.
func stroke(shoulder: Vector3, wrist: Vector3, spread: float) -> void:
	var mirrored := Vector3(wrist.x * side, wrist.y, wrist.z)
	target.position = shoulder + mirrored
	pole.position = shoulder + mirrored * 0.5 + Vector3(0.0, -0.02, 0.3)
	_spread = clampf(spread, 0.0, 1.0)


## Place the fingers and rebuild the membrane from the solved bones.
func rebuild() -> void:
	var shoulder := _bone(_shoulder_bone)
	var elbow := _bone(_elbow_bone)
	var wrist := _bone(_wrist_bone)
	var ankle := _bone(_ankle_bone)
	var along := wrist - elbow
	if along.length() < 0.001:
		return
	along = along.normalized()
	var back := Vector3.BACK - along * along.dot(Vector3.BACK) # "backward", square to the forearm
	back = back.normalized() if back.length() > 0.01 else Vector3.DOWN
	var tips: Array[Vector3] = []
	for i in FINGER_LENGTHS.size():
		var angle := lerpf(FINGER_FOLDED[i], FINGER_SPREAD[i], _spread)
		var direction := along * cos(angle) + back * sin(angle)
		var length := FINGER_LENGTHS[i]
		tips.append(wrist + direction * length)
		var basis := BatMeshes.basis_along(direction)
		basis.y *= length
		_fingers[i].transform = Transform3D(basis, wrist + direction * length * 0.5)
	_build_membrane(shoulder, elbow, wrist, tips, ankle, along.cross(back) * -side)


# `up` is the wing's top side (its plane normal); every triangle faces it.
func _build_membrane(shoulder: Vector3, elbow: Vector3, wrist: Vector3, tips: Array[Vector3], ankle: Vector3, up: Vector3) -> void:
	var centre := (shoulder + wrist + tips[2] + ankle) * 0.25
	var edge := PackedVector3Array([shoulder, elbow, wrist, tips[0]])
	var trailing: Array[Vector3] = [tips[0], tips[1], tips[2], ankle]
	for k in trailing.size() - 1: # scallops: a sagging midpoint between each pair of tips
		edge.append(((trailing[k] + trailing[k + 1]) * 0.5).lerp(centre, SCALLOP))
		edge.append(trailing[k + 1])
	edge.append(_hip)
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


func _bone(bone: int) -> Vector3:
	return _skeleton.get_bone_global_pose(bone).origin
