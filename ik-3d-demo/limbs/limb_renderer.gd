## Draws any number of LimbChains with two MultiMeshes and no other nodes: one for every segment
## (a unit cylinder stretched along it) and one for every joint (a ball). Each instance carries the
## chain's colour, so all limbs of all creatures are 2 draw calls, whatever the count.
##
## Add chains with add(), then call draw() after solving each frame. Leave this node at the origin,
## unrotated: chain points are world space.
class_name LimbRenderer
extends Node3D

var chains: Array[LimbChain] = []

var _segments: MultiMeshInstance3D
var _joints: MultiMeshInstance3D
var _dirty := true


func _ready() -> void:
	var cylinder := CylinderMesh.new()
	cylinder.top_radius = 1.0
	cylinder.bottom_radius = 1.0
	cylinder.height = 1.0
	cylinder.radial_segments = 8
	cylinder.rings = 1
	var ball := SphereMesh.new()
	ball.radius = 1.0
	ball.height = 2.0
	ball.radial_segments = 10
	ball.rings = 5
	_segments = _layer(cylinder, "Segments")
	_joints = _layer(ball, "Joints")


func add(chain: LimbChain) -> void:
	chains.append(chain)
	_dirty = true


func clear() -> void:
	chains.clear()
	_dirty = true


## Move every segment and joint instance to where its chain is now. Only transforms change per
## frame; colours are written once, when the chain list changes. (Tested: filling one float
## buffer and uploading it with `multimesh.buffer` is no faster from GDScript — it's the
## per-instance script work that costs, which C++ would make negligible.)
func draw() -> void:
	if _dirty:
		_resize()
	var segments := _segments.multimesh
	var joints := _joints.multimesh
	var segment := 0
	var joint := 0
	for chain in chains:
		var r := chain.radius
		var ball := Basis.from_scale(Vector3.ONE * r * 1.35)
		for i in chain.segment_count():
			var basis := chain.segment_basis(i)
			basis.x *= r
			basis.y *= chain.segment_length(i)
			basis.z *= r
			segments.set_instance_transform(segment, Transform3D(basis, (chain.points[i] + chain.points[i + 1]) * 0.5))
			segment += 1
		for point in chain.points:
			joints.set_instance_transform(joint, Transform3D(ball, point))
			joint += 1


func _resize() -> void:
	var segment_total := 0
	var joint_total := 0
	for chain in chains:
		segment_total += chain.segment_count()
		joint_total += chain.points.size()
	_segments.multimesh.instance_count = segment_total
	_joints.multimesh.instance_count = joint_total
	var segment := 0
	var joint := 0
	for chain in chains:
		for i in chain.segment_count():
			_segments.multimesh.set_instance_color(segment, chain.color)
			segment += 1
		for point in chain.points:
			_joints.multimesh.set_instance_color(joint, chain.color.lightened(0.2))
			joint += 1
	_dirty = false


func _layer(mesh: Mesh, layer_name: String) -> MultiMeshInstance3D:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.mesh = mesh
	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.roughness = 0.7
	var layer := MultiMeshInstance3D.new()
	layer.name = layer_name
	layer.multimesh = multimesh
	layer.material_override = material
	add_child(layer)
	return layer
