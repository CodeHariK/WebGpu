## Draws any number of LimbChains with three MultiMeshes and no other nodes: ROUND chains as a
## unit cylinder stretched along each segment plus a ball at each joint, BLOCK chains as a unit box
## per segment (shortened by the chain's gap). Each instance carries the chain's colour, so all
## limbs of all creatures are 3 draw calls, whatever the count.
##
## Add chains with add(), then call draw() after solving each frame. Chain points are world space,
## so this node is top_level at the origin: parent it anywhere (it still hides with its parent).
class_name LimbRenderer
extends Node3D

var chains: Array[LimbChain] = []

@export var roughness := 0.7
@export var material: Material ## use this instead of the default lit vertex-colour material (set before adding to the tree)

var _segments: MultiMeshInstance3D
var _joints: MultiMeshInstance3D
var _blocks: MultiMeshInstance3D
var _dirty := true


func _ready() -> void:
	top_level = true
	global_transform = Transform3D.IDENTITY
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
	_blocks = _layer(BoxMesh.new(), "Blocks") # unit cube


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
	var blocks := _blocks.multimesh
	var segment := 0
	var joint := 0
	var block := 0
	for chain in chains:
		if chain.style == LimbChain.Style.BLOCK:
			for i in chain.segment_count():
				var r := chain.segment_radius(i)
				var basis := chain.segment_basis(i)
				basis.x *= r * 2.0
				basis.y *= maxf(chain.segment_length(i) - chain.gap * 2.0, 0.02)
				basis.z *= r * 2.0
				blocks.set_instance_transform(block, Transform3D(basis, (chain.points[i] + chain.points[i + 1]) * 0.5))
				block += 1
			continue
		for i in chain.segment_count():
			var r := chain.segment_radius(i)
			var basis := chain.segment_basis(i)
			basis.x *= r
			basis.y *= chain.segment_length(i)
			basis.z *= r
			segments.set_instance_transform(segment, Transform3D(basis, (chain.points[i] + chain.points[i + 1]) * 0.5))
			segment += 1
		for k in chain.points.size():
			joints.set_instance_transform(joint, Transform3D(Basis.from_scale(Vector3.ONE * chain.joint_ball_radius(k)), chain.points[k]))
			joint += 1


func _resize() -> void:
	var counts := Vector3i.ZERO # segments, joints, blocks
	for chain in chains:
		if chain.style == LimbChain.Style.BLOCK:
			counts.z += chain.segment_count()
		else:
			counts.x += chain.segment_count()
			counts.y += chain.points.size()
	_segments.multimesh.instance_count = counts.x
	_joints.multimesh.instance_count = counts.y
	_blocks.multimesh.instance_count = counts.z
	var segment := 0
	var joint := 0
	var block := 0
	for chain in chains: # colours never change per frame: write them once here
		if chain.style == LimbChain.Style.BLOCK:
			for i in chain.segment_count():
				_blocks.multimesh.set_instance_color(block, chain.color)
				block += 1
			continue
		var joint_color := chain.color if chain.joint_radius >= 0.0 else chain.color.lightened(0.2)
		for i in chain.segment_count():
			_segments.multimesh.set_instance_color(segment, chain.color)
			segment += 1
		for point in chain.points:
			_joints.multimesh.set_instance_color(joint, joint_color)
			joint += 1
	_dirty = false


func _layer(mesh: Mesh, layer_name: String) -> MultiMeshInstance3D:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.mesh = mesh
	var lit := StandardMaterial3D.new()
	lit.vertex_color_use_as_albedo = true
	lit.vertex_color_is_srgb = true # instance colours are sRGB, like albedo_color
	lit.roughness = roughness
	var layer := MultiMeshInstance3D.new()
	layer.name = layer_name
	layer.multimesh = multimesh
	layer.material_override = material if material != null else lit
	add_child(layer)
	return layer
