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
##   billboards a kind with billboard_from (the trees) gets a second batch next to each of its
##              batches: the same copies drawn as flat stand-ins (TreeBillboards), from that
##              distance on. The 3D batch has the stand-in batch as its visibility parent
##              (Godot's HLOD), so a chunk draws one or the other, never both

const CHUNK_SIZE := 64.0
const SEED := 21
const LEAF_LIFT := 0.03 ## metres: leaves float this much over the ground (no flicker)

@export var enabled := true ## off: nothing is placed (the grass test runs without the props)
@export var meshes_scene: PackedScene ## assets/fall_assets.glb
@export var material: Material
@export var leaf_material: Material

var batches: Array[MultiMeshInstance3D] = []
var far_batches: Array[MultiMeshInstance3D] = [] # the stand-ins (billboards)
var placed := 0
var _ranges_on := true
var _billboards_on := true
var _kind_shown := {} # kind name -> false when hidden
var _billboard_meshes := {} # tree mesh name -> its stand-in
var _billboard_material := ShaderMaterial.new()
var _variants := {} # the materials set_shading made, by base material and mode


func _ready() -> void:
	if not enabled:
		return
	_billboard_material.shader = load("res://props/tree_billboard.gdshader")
	var meshes := _meshes_by_name()
	var rng := RandomNumberGenerator.new()
	rng.seed = SEED
	for kind in PropKinds.KINDS:
		var by_batch := {} # Vector3i(chunk x, chunk z, mesh) -> Array of Transform3D, chunk-local
		_place(kind, rng, by_batch)
		_make_batches(kind, meshes, by_batch)


## Every kind's range on, or none (everything drawn at any distance, in 3D).
func set_ranges(p_on: bool) -> void:
	_ranges_on = p_on
	_refresh()


## The far trees as stand-ins (on), or as 3D trees all the way to their range (off).
func set_billboards(p_on: bool) -> void:
	_billboards_on = p_on
	_refresh()


## One kind (PropKinds' name: trees, bushes, ...) shown or hidden.
func set_kind_visible(p_kind: String, p_on: bool) -> void:
	_kind_shown[p_kind] = p_on
	_refresh()


## Every batch's visibility and ranges from the switches above.
func _refresh() -> void:
	var standing_in := _ranges_on and _billboards_on
	for batch in batches:
		var shown: bool = _kind_shown.get(batch.get_meta("kind"), true)
		batch.visible = shown
		var far: MultiMeshInstance3D = batch.get_meta("far", null)
		if far:
			far.visible = shown and standing_in
		if far and standing_in:
			batch.visibility_parent = batch.get_path_to(far)
			batch.visibility_range_end = 0.0
		else:
			batch.visibility_parent = NodePath()
			batch.visibility_range_end = batch.get_meta("range") if _ranges_on else 0.0


## How the props are shaded, to find what they cost: "lit" (as made), "no shadows received"
## (lit, the shadow map not read), "unshaded" (the texture's colour only, no lighting at all).
func set_shading(p_mode: String) -> void:
	for batch in batches:
		var base: Material = leaf_material if batch.get_meta("leaf") else material
		batch.material_override = base if p_mode == "lit" else _variant(base, p_mode)



func _variant(p_base: StandardMaterial3D, p_mode: String) -> StandardMaterial3D:
	var key := "%d %s" % [p_base.get_instance_id(), p_mode]
	if not _variants.has(key):
		var variant := p_base.duplicate() as StandardMaterial3D
		if p_mode == "unshaded":
			variant.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		else:
			variant.disable_receive_shadows = true
		_variants[key] = variant
	return _variants[key]


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
		batch.set_meta("kind", p_kind["name"])
		batch.set_meta("leaf", p_kind.get("leaf", false))
		batch.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if p_kind["shadow"] else GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(batch)
		batches.append(batch)
		if p_kind.has("billboard_from"):
			_add_stand_ins(batch, p_kind, names[key.z], p_meshes)
	_refresh()


## The same copies as batch, drawn as stand-ins from the kind's billboard_from to its range.
func _add_stand_ins(p_batch: MultiMeshInstance3D, p_kind: Dictionary, p_mesh_name: String, p_meshes: Dictionary) -> void:
	if not _billboard_meshes.has(p_mesh_name):
		_billboard_meshes[p_mesh_name] = TreeBillboards.build(p_meshes[p_mesh_name], hash(p_mesh_name))
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = _billboard_meshes[p_mesh_name]
	multimesh.instance_count = p_batch.multimesh.instance_count
	multimesh.buffer = p_batch.multimesh.buffer
	var far := MultiMeshInstance3D.new()
	far.name = p_batch.name + "_far"
	far.multimesh = multimesh
	far.position = p_batch.position
	far.material_override = _billboard_material
	far.visibility_range_begin = p_kind["billboard_from"]
	far.visibility_range_begin_margin = 4.0
	far.visibility_range_end = p_kind["range"]
	far.visibility_range_end_margin = p_kind["range"] * 0.05
	far.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	far.set_meta("kind", p_kind["name"])
	add_child(far)
	far_batches.append(far)
	p_batch.set_meta("far", far)


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
