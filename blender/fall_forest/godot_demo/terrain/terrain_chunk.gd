class_name TerrainChunk
extends Node3D
## One square of ground, three parts:
##   near mesh   a vertex every NEAR_STEP metres, drawn closer than SWITCH_DISTANCE
##   far mesh    a vertex every FAR_STEP metres (1/16 of the triangles), drawn beyond it, so the
##               far ground is cheap. The far mesh's visibility range starts at the switch; the
##               near mesh has the far one as its visibility parent, which draws it exactly when
##               the far one is out of its range (Godot's HLOD): always one of the two, never
##               both, never neither
##   occluder    the coarse ground sunk OCCLUDER_SINK metres: the renderer skips whatever is
##               fully hidden behind it (props behind a hill), see Terrain
## Both meshes have a skirt: their edge pulled SKIRT metres down, so where a fine chunk meets a
## coarse one (their edges don't line up between the coarse vertices) no gap shows through.

const NEAR_STEP := 1.0
const FAR_STEP := 4.0
const SWITCH_DISTANCE := 96.0 ## metres from the camera where the near mesh hands over
const SWITCH_MARGIN := 8.0 ## hysteresis, so a chunk on the line doesn't flicker
const SKIRT := 3.0
const OCCLUDER_SINK := 0.5 ## below the ground, so a prop standing on it is never hidden by it

var near_mesh: MeshInstance3D
var far_mesh: MeshInstance3D
var occluder: OccluderInstance3D


## p_origin: the chunk's corner (x, z); p_size: metres on a side. Call it once in the tree.
func build(p_origin: Vector2, p_size: float, p_material: Material) -> void:
	position = Vector3(p_origin.x, 0.0, p_origin.y)
	near_mesh = _mesh_instance(_grid_mesh(p_size, NEAR_STEP), p_material)
	# the ground casts no shadow: double-sided (for the skirts), it would shadow itself; the
	# props' shadows still fall on it
	near_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	far_mesh = _mesh_instance(_grid_mesh(p_size, FAR_STEP), p_material)
	far_mesh.visibility_range_begin = SWITCH_DISTANCE
	far_mesh.visibility_range_begin_margin = SWITCH_MARGIN
	far_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	near_mesh.visibility_parent = near_mesh.get_path_to(far_mesh)
	occluder = OccluderInstance3D.new()
	occluder.occluder = _occluder_shape(p_size)
	add_child(occluder)


func _mesh_instance(p_mesh: ArrayMesh, p_material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = p_mesh
	instance.material_override = p_material
	add_child(instance)
	return instance


## A grid of heights with one more point each side: the inner ones are the ground, the outer
## ring is the skirt (moved onto the edge and pulled down) and gives the edge its slope.
func _grid_mesh(p_size: float, p_step: float) -> ArrayMesh:
	var n := int(round(p_size / p_step))
	var count := n + 3
	var heights := _heights(n, count, p_step)
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var colors := PackedColorArray()
	for j in count:
		for i in count:
			var gi := clampi(i - 1, 0, n)
			var gj := clampi(j - 1, 0, n)
			var h := heights[(gj + 1) * count + gi + 1]
			var skirt := gi != i - 1 or gj != j - 1
			vertices.append(Vector3(gi * p_step, h - (SKIRT if skirt else 0.0), gj * p_step))
			var across := heights[(gj + 1) * count + gi + 2] - heights[(gj + 1) * count + gi]
			var along := heights[(gj + 2) * count + gi + 1] - heights[gj * count + gi + 1]
			var normal := Vector3(-across, 2.0 * p_step, -along).normalized()
			normals.append(normal)
			colors.append(_ground_colour(position.x + gi * p_step, position.z + gj * p_step, normal))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors
	arrays[Mesh.ARRAY_INDEX] = _grid_indices(count)
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


## Heights for grid points -1 .. n + 1 (row by row), in world metres.
func _heights(p_n: int, p_count: int, p_step: float) -> PackedFloat32Array:
	var heights := PackedFloat32Array()
	heights.resize(p_count * p_count)
	for j in range(-1, p_n + 2):
		for i in range(-1, p_n + 2):
			heights[(j + 1) * p_count + i + 1] = TerrainHeight.height(position.x + i * p_step, position.z + j * p_step)
	return heights


## Two triangles a square, clockwise seen from above (Godot's front faces).
func _grid_indices(p_count: int) -> PackedInt32Array:
	var indices := PackedInt32Array()
	for j in p_count - 1:
		for i in p_count - 1:
			var a := j * p_count + i
			var b := a + 1
			var c := a + p_count
			var d := c + 1
			indices.append_array([a, b, c, b, d, c])
	return indices


## Fall ground: olive grass in the clearings, orange leaf litter in the woods, brown on slopes.
func _ground_colour(p_x: float, p_z: float, p_normal: Vector3) -> Color:
	var grass := Color(0.44, 0.5, 0.2)  # the far grass's green: past the grass, the ground is the grass
	var litter := Color(0.72, 0.42, 0.2)
	var dirt := Color(0.45, 0.33, 0.24)
	var colour := grass.lerp(litter, TerrainHeight.forest(p_x, p_z))
	return colour.lerp(dirt, smoothstep(0.8, 0.6, p_normal.y))


## The coarse ground, sunk a little, as an occluder (no skirt: it only has to hide things).
func _occluder_shape(p_size: float) -> ArrayOccluder3D:
	var n := int(round(p_size / FAR_STEP))
	var vertices := PackedVector3Array()
	for j in n + 1:
		for i in n + 1:
			var h := TerrainHeight.height(position.x + i * FAR_STEP, position.z + j * FAR_STEP)
			vertices.append(Vector3(i * FAR_STEP, h - OCCLUDER_SINK, j * FAR_STEP))
	var shape := ArrayOccluder3D.new()
	shape.set_arrays(vertices, _grid_indices(n + 1))
	return shape


## The far mesh on (by distance), or off: then the near mesh is drawn at any distance.
func set_ranges(p_on: bool) -> void:
	far_mesh.visible = p_on
	near_mesh.visibility_parent = near_mesh.get_path_to(far_mesh) if p_on else NodePath()
