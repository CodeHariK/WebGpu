## Choco Island: assembles a whole island from a seed.
##
##   shape    → the outline; the cake layers are built from it (choco_mesh.gd)
##   mountain → stacked kit cubes (choco_mountain.gd)
##   stairs   → kit step blocks spiralling up the mountain (choco_stairs.gd)
##   syrup    → pond, falls, pool, stream, edge fall (choco_syrup.gd)
##   scatter  → props, patches, drips, floating platforms, sea swirls (choco_scatter.gd)
##
## Repeated kit pieces are drawn with one MultiMeshInstance3D per piece. Collision: a box per
## block and step, a cylinder per platform, the cake's own mesh for the island.
extends Node3D

const Kit := preload("res://choco_kit.gd")
const Mesh_ := preload("res://choco_mesh.gd")
const Shape := preload("res://choco_shape.gd")
const Mountain := preload("res://choco_mountain.gd")
const Stairs := preload("res://choco_stairs.gd")
const Syrup := preload("res://choco_syrup.gd")
const Scatter := preload("res://choco_scatter.gd")

const MOUNTAIN_CENTRE := Vector2(0, -7)
const SEA_HALF_SIZE := 140.0

var kit := Kit.new()
var mountain: Mountain
var stairs: Stairs

var _placed := {}  # piece name -> Array[Transform3D]
var _body: StaticBody3D


func generate(seed_value: int) -> void:
	for child in get_children():
		child.free()
	_placed.clear()
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_value
	_body = StaticBody3D.new()
	_body.name = "Collision"
	add_child(_body)

	var shape := Shape.new(rng)
	var cake := Mesh_.Builder.new()
	Mesh_.island_cake(cake, shape.outline(), -6.0)
	var cake_mesh := _add_mesh("IslandCake", cake)
	_add_shape(cake_mesh.create_trimesh_shape(), Transform3D.IDENTITY)

	mountain = Mountain.new(rng, kit.family("Cube_"), MOUNTAIN_CENTRE)
	for block in mountain.blocks:
		_place(block.piece, block.xform())
		_add_box(block.xform(), Vector3(1, 1, 1))
	_place("Cherry", Transform3D(Basis.IDENTITY, Vector3(mountain.peak.pos.x, mountain.top(), mountain.peak.pos.z)))

	stairs = Stairs.new(rng, mountain, kit.family("Step_"))
	var stair_tops: Array[Vector2] = []
	for step in stairs.steps:
		_place(step.piece, Stairs.xform(step))
		_add_box(Stairs.xform(step), Vector3(1, 0.6, 1))
		stair_tops.append(Vector2(step.top.x, step.top.z))

	var wet := Mesh_.Builder.new()
	var syrup := Syrup.new(wet, rng, mountain, shape)
	Mesh_.sea(wet, rng, SEA_HALF_SIZE, 70, Syrup.SEA_Y)
	_add_mesh("Syrup", wet)
	_place_all(syrup.pieces)

	var ground := Mesh_.Builder.new()
	var scatter := Scatter.new(ground, rng, mountain, shape, syrup.stream, stair_tops, syrup.end_angle)
	_add_mesh("Patches", ground)
	_place_all(scatter.pieces)
	for platform in scatter.platforms:
		var cylinder := CylinderShape3D.new()
		cylinder.radius = platform.radius
		cylinder.height = platform.radius * 0.63
		_add_shape(cylinder, Transform3D(Basis.IDENTITY, platform.centre - Vector3(0, cylinder.height * 0.5, 0)))

	_build_multimeshes()


func _place(piece: String, xform: Transform3D) -> void:
	if not _placed.has(piece):
		_placed[piece] = []
	_placed[piece].append(xform)


func _place_all(list: Array[Dictionary]) -> void:
	for item in list:
		_place(item.piece, item.xform)


func _add_mesh(node_name: String, builder: Mesh_.Builder) -> ArrayMesh:
	var mesh := builder.commit()
	var node := MeshInstance3D.new()
	node.name = node_name
	node.mesh = mesh
	add_child(node)
	return mesh


## One MultiMeshInstance3D per kit piece, holding every copy of it.
func _build_multimeshes() -> void:
	for piece: String in _placed:
		var list: Array = _placed[piece]
		var multi := MultiMesh.new()
		multi.transform_format = MultiMesh.TRANSFORM_3D
		multi.mesh = kit.mesh(piece)
		multi.instance_count = list.size()
		for i in list.size():
			multi.set_instance_transform(i, list[i])
		var node := MultiMeshInstance3D.new()
		node.name = piece
		node.multimesh = multi
		add_child(node)


## A box collider for a kit piece whose unit shape is `unit` (base at y = 0) under `xform`.
## Shapes can't be scaled, so the scale goes into the box size.
func _add_box(xform: Transform3D, unit: Vector3) -> void:
	var stretch := xform.basis.get_scale()
	var box := BoxShape3D.new()
	box.size = unit * stretch
	_add_shape(box, Transform3D(xform.basis.orthonormalized(), xform * Vector3(0, unit.y * 0.5, 0)))


func _add_shape(shape: Shape3D, xform: Transform3D) -> void:
	var node := CollisionShape3D.new()
	node.shape = shape
	node.transform = xform
	_body.add_child(node)
