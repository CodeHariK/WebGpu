@tool
extends Node3D
## Stones scattered round the waterfall by a seed: a stack up each side of the sheet (the cliffs
## the water falls between) and a few lying along the pool. `variants` stone meshes are made
## (stone_mesh.gd), each placed stone picks one, with its own size, stretch and turn. The stones
## are made when the scene starts (and in the editor), not saved with it. `material` goes on all.
## `multimesh`: all the stones sharing a mesh drawn at once (a MultiMeshInstance3D per variant:
## 6 draws instead of one per stone, for mobile); off: a MeshInstance3D per stone.

const StoneMesh := preload("res://waterfall_simple/stone_mesh.gd")

@export var seed := 7:
	set(value):
		seed = value
		_scatter()
@export var variants := 6:
	set(value):
		variants = value
		_scatter()
@export var per_side := 7: # stones stacked up each side of the sheet
	set(value):
		per_side = value
		_scatter()
@export var along_pool := 8: # stones lying along the pool's sides
	set(value):
		along_pool = value
		_scatter()
@export var half_width := 4.0: # half the falls' width: the stacks stand just outside it
	set(value):
		half_width = value
		_scatter()
@export var flat_shading := false:
	set(value):
		flat_shading = value
		_scatter()
@export var multimesh := true:
	set(value):
		multimesh = value
		_scatter()
@export var material: Material:
	set(value):
		material = value
		for child in get_children():
			(child as GeometryInstance3D).material_override = material

var _placed := {} # mesh -> the transforms of the stones using it (for the multimeshes)


func _ready() -> void:
	_scatter()


func _scatter() -> void:
	if not is_inside_tree():
		return
	for child in get_children():
		child.free()
	_placed.clear()
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var meshes: Array[ArrayMesh] = []
	for i in variants:
		meshes.append(StoneMesh.build(seed * 101 + i, 3, 0.73, 0.5, -0.55, flat_shading))
	# up each side of the falls (half_width out from the middle, its foot at z 0 .. 1.5), a stone
	# every 2 m or so
	for side in [-1.0, 1.0]:
		var height := -0.5
		for i in per_side:
			var size := rng.randf_range(1.6, 2.6)
			var at := Vector3(side * (half_width + rng.randf_range(1.0, 2.2)), height + size * 0.6, rng.randf_range(-1.5, 2.0))
			_add(meshes[rng.randi() % meshes.size()], at, size, rng)
			height += size * rng.randf_range(1.0, 1.4)
	# along the pool's sides, in front of the falls
	for i in along_pool:
		var side := -1.0 if i % 2 == 0 else 1.0
		var size := rng.randf_range(0.9, 1.8)
		var at := Vector3(side * (half_width + rng.randf_range(2.0, 5.5)), 0.4, rng.randf_range(3.0, 14.0))
		_add(meshes[rng.randi() % meshes.size()], at, size, rng)
	if multimesh:
		for mesh in _placed:
			var transforms: Array = _placed[mesh]
			var many := MultiMesh.new()
			many.transform_format = MultiMesh.TRANSFORM_3D
			many.mesh = mesh
			many.instance_count = transforms.size()
			for i in transforms.size():
				many.set_instance_transform(i, transforms[i])
			var node := MultiMeshInstance3D.new()
			node.multimesh = many
			node.material_override = material
			add_child(node)


func _add(p_mesh: ArrayMesh, p_at: Vector3, p_size: float, p_rng: RandomNumberGenerator) -> void:
	var stretch := Vector3(p_rng.randf_range(0.8, 1.3), p_rng.randf_range(0.7, 1.0), p_rng.randf_range(0.8, 1.3))
	var where := Transform3D(Basis(Vector3.UP, p_rng.randf_range(0.0, TAU)).scaled(stretch * p_size), p_at)
	if multimesh:
		if not _placed.has(p_mesh):
			_placed[p_mesh] = []
		_placed[p_mesh].append(where)
		return
	var stone := MeshInstance3D.new()
	stone.mesh = p_mesh
	stone.material_override = material
	stone.transform = where
	add_child(stone)
