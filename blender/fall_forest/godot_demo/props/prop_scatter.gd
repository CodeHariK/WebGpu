class_name PropScatter
extends Node3D
## Places the PropKinds on the terrain and draws them in batches: for every prop chunk
## (CHUNK_SIZE metres square) and every mesh, one MultiMeshInstance3D with all its copies there.
##   batching   a MultiMesh draws every copy of one mesh in one draw call, instead of a call a
##              prop: 20 000 props become a few hundred calls
##   chunks     each batch has its own bounding box, so whole batches are skipped when outside
##              the view (frustum culling), past their range, or behind a hill (occlusion
##              culling); and a batch's level of detail (mesh LOD: Godot made simpler versions
##              of every mesh when it imported the glb) is picked by how far the batch is
##   ranges     each kind stops being drawn past its range (PropKinds): leaves at 35 m, trees
##              at 400 m
##   shadows    only the big kinds cast shadows

const CHUNK_SIZE := 64.0
const SEED := 21
const LEAF_LIFT := 0.03 ## metres: leaves float this much over the ground (no flicker)

@export var enabled := true ## off: nothing is placed (the grass test runs without the props)
@export var meshes_scene: PackedScene ## assets/fall_assets.glb
@export var material: Material
@export var leaf_material: Material

var batches: Array[MultiMeshInstance3D] = []
var placed := 0


func _ready() -> void:
	if not enabled:
		return
	var meshes := _meshes_by_name()
	var rng := RandomNumberGenerator.new()
	rng.seed = SEED
	for kind in PropKinds.KINDS:
		var by_batch := {} # Vector3i(chunk x, chunk z, mesh) -> Array of Transform3D, chunk-local
		_place(kind, rng, by_batch)
		_make_batches(kind, meshes, by_batch)


## Every kind's range on, or none (everything drawn at any distance).
func set_ranges(p_on: bool) -> void:
	for batch in batches:
		batch.visibility_range_end = batch.get_meta("range") if p_on else 0.0


## Random spots, kept as often as the kind likes the place (PropKinds.chance).
func _place(p_kind: Dictionary, p_rng: RandomNumberGenerator, p_by_batch: Dictionary) -> void:
	var half := TerrainHeight.SIZE * 0.5
	var wanted: int = p_kind["count"]
	var sizes: Array = p_kind["scale"]
	var mesh_count: int = (p_kind["meshes"] as Array).size()
	var made := 0
	var tries := 0
	while made < wanted and tries < wanted * 40:
		tries += 1
		var x := p_rng.randf_range(-half, half)
		var z := p_rng.randf_range(-half, half)
		if p_rng.randf() > PropKinds.chance(p_kind["where"], x, z):
			continue
		var up := TerrainHeight.normal(x, z) if p_kind["lie"] else Vector3.UP
		var basis := _standing_on(up, p_rng.randf() * TAU, p_rng.randf_range(sizes[0], sizes[1]))
		var y := TerrainHeight.height(x, z) + (LEAF_LIFT if p_kind.get("leaf", false) else 0.0)
		var cx := clampi(int(floor((x + half) / CHUNK_SIZE)), 0, int(TerrainHeight.SIZE / CHUNK_SIZE) - 1)
		var cz := clampi(int(floor((z + half) / CHUNK_SIZE)), 0, int(TerrainHeight.SIZE / CHUNK_SIZE) - 1)
		var key := Vector3i(cx, cz, p_rng.randi() % mesh_count)
		if not p_by_batch.has(key):
			p_by_batch[key] = []
		p_by_batch[key].append(Transform3D(basis, Vector3(x, y, z) - _chunk_corner(cx, cz)))
		made += 1
	placed += made


## Turned p_yaw about its own up, that up tilted onto p_up, sized p_scale.
func _standing_on(p_up: Vector3, p_yaw: float, p_scale: float) -> Basis:
	var tilt := Basis(Quaternion(Vector3.UP, p_up))
	return (tilt * Basis(Vector3.UP, p_yaw)).scaled(Vector3.ONE * p_scale)


func _make_batches(p_kind: Dictionary, p_meshes: Dictionary, p_by_batch: Dictionary) -> void:
	var names: Array = p_kind["meshes"]
	var range_end: float = p_kind["range"]
	for key: Vector3i in p_by_batch:
		var transforms: Array = p_by_batch[key]
		var multimesh := MultiMesh.new()
		multimesh.transform_format = MultiMesh.TRANSFORM_3D
		multimesh.mesh = p_meshes[names[key.z]]
		multimesh.instance_count = transforms.size()
		for i in transforms.size():
			multimesh.set_instance_transform(i, transforms[i])
		var batch := MultiMeshInstance3D.new()
		batch.name = "%s_%d_%d_%s" % [p_kind["name"], key.x, key.y, names[key.z]]
		batch.multimesh = multimesh
		batch.position = _chunk_corner(key.x, key.y)
		batch.material_override = leaf_material if p_kind.get("leaf", false) else material
		batch.visibility_range_end = range_end
		batch.visibility_range_end_margin = range_end * 0.05
		batch.set_meta("range", range_end)
		batch.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if p_kind["shadow"] else GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(batch)
		batches.append(batch)


func _chunk_corner(p_cx: int, p_cz: int) -> Vector3:
	var half := TerrainHeight.SIZE * 0.5
	return Vector3(-half + p_cx * CHUNK_SIZE, 0.0, -half + p_cz * CHUNK_SIZE)


## The glb's meshes by node name (Tree1, Acorn2, ...), with the LODs Godot made on import.
func _meshes_by_name() -> Dictionary:
	var meshes := {}
	var root := meshes_scene.instantiate()
	for node in root.find_children("*", "MeshInstance3D", true, false):
		meshes[str(node.name)] = (node as MeshInstance3D).mesh
	root.free()
	return meshes
