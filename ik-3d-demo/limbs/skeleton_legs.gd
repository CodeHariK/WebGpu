## Benchmark baseline: the way the spider rig works today, for one creature's legs. One Skeleton3D,
## one TwoBoneIK3D with a setting per leg, a target and a pole Marker3D per leg, and the visuals
## (2 segment cylinders + 3 joint balls per leg) as MeshInstance3Ds on BoneAttachment3Ds.
## Same look as LimbRenderer so the two are compared like for like.
class_name SkeletonLegs
extends Node3D

var hips: Array[Vector3] = [] ## local hip positions
var lengths := Vector2(0.45, 0.5)
var radius := 0.03
var color := Color.WHITE

var targets: Array[Marker3D] = []
var poles: Array[Marker3D] = []

var _skeleton: Skeleton3D


func _ready() -> void:
	_skeleton = Skeleton3D.new()
	add_child(_skeleton)
	var body := _skeleton.add_bone("body")
	_skeleton.set_bone_rest(body, Transform3D.IDENTITY)
	var ik := TwoBoneIK3D.new()
	_skeleton.add_child(ik)
	ik.set_setting_count(hips.size())
	var cylinder := _cylinder()
	var ball := _ball()
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.7
	var joint_material := StandardMaterial3D.new()
	joint_material.albedo_color = color.lightened(0.2)
	joint_material.roughness = 0.7
	for leg in hips.size():
		var upper := _add_bone("upper_%d" % leg, body, hips[leg])
		var lower := _add_bone("lower_%d" % leg, upper, Vector3(0.05, -lengths.x, 0)) # a slight rest bend
		var foot := _add_bone("foot_%d" % leg, lower, Vector3(0, -lengths.y, 0))
		targets.append(_marker("target_%d" % leg))
		poles.append(_marker("pole_%d" % leg))
		ik.set_root_bone_name(leg, "upper_%d" % leg)
		ik.set_middle_bone_name(leg, "lower_%d" % leg)
		ik.set_end_bone_name(leg, "foot_%d" % leg)
		ik.set_target_node(leg, ik.get_path_to(targets[leg]))
		ik.set_pole_node(leg, ik.get_path_to(poles[leg]))
		_visual(upper, cylinder, material, Vector3(0.05, -lengths.x, 0))
		_visual(lower, cylinder, material, Vector3(0, -lengths.y, 0))
		for bone in [upper, lower, foot]:
			_visual(bone, ball, joint_material, Vector3.ZERO)


func _marker(marker_name: String) -> Marker3D:
	var marker := Marker3D.new()
	marker.name = marker_name
	add_child(marker)
	return marker


func _add_bone(bone_name: String, parent: int, offset: Vector3) -> int:
	var bone := _skeleton.add_bone(bone_name)
	_skeleton.set_bone_parent(bone, parent)
	_skeleton.set_bone_rest(bone, Transform3D(Basis.IDENTITY, offset))
	_skeleton.set_bone_pose(bone, _skeleton.get_bone_rest(bone))
	return bone


# A segment (along `child_offset`) or a joint ball (child_offset ZERO) riding on `bone`.
func _visual(bone: int, mesh: Mesh, material: Material, child_offset: Vector3) -> void:
	var attach := BoneAttachment3D.new()
	_skeleton.add_child(attach)
	attach.bone_idx = bone
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	if child_offset == Vector3.ZERO:
		instance.transform = Transform3D(Basis.from_scale(Vector3.ONE * radius * 1.35), Vector3.ZERO)
	else:
		var y := child_offset.normalized()
		var x := y.cross(Vector3.FORWARD).normalized()
		instance.transform = Transform3D(Basis(x * radius, y * child_offset.length(), x.cross(y) * radius), child_offset * 0.5)
	attach.add_child(instance)


static func _cylinder() -> CylinderMesh:
	var cylinder := CylinderMesh.new()
	cylinder.top_radius = 1.0
	cylinder.bottom_radius = 1.0
	cylinder.height = 1.0
	cylinder.radial_segments = 8
	cylinder.rings = 1
	return cylinder


static func _ball() -> SphereMesh:
	var ball := SphereMesh.new()
	ball.radius = 1.0
	ball.height = 2.0
	ball.radial_segments = 10
	ball.rings = 5
	return ball
