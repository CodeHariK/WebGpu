## Builds meshes from an IslandShape: a terrain grid and a water grid.
## The water's vertex colour R = depth / max_depth — exactly what Blender baked for the glb, but
## computed here at runtime from the same height function, so any generated island gets correct
## shore colours and foam with no depth texture.
class_name IslandMeshBuilder
extends RefCounted

const HALF := 16.0          # meshes cover ±HALF metres
const TERRAIN_STEP := 0.3
const WATER_STEP := 0.5
const MAX_DEPTH := 2.0      # depth that maps to colour 1.0 (keep in sync with the shaders' tuning)

## false = the first, naive version (every cell split the same way, normals sampled from the height
## function) — kept so the demo can show the jagged bank next to the fixed one (key J).
static var smooth_edges := true


static func terrain(shape: IslandShape) -> ArrayMesh:
	return _grid(TERRAIN_STEP, func(x: float, z: float) -> float: return shape.height(x, z), Callable(), shape)


static func water(shape: IslandShape) -> ArrayMesh:
	var depth_colour := func(x: float, z: float) -> Color:
		return Color(clampf(shape.depth(x, z) / MAX_DEPTH, 0.0, 1.0), 0.0, 0.0)
	return _grid(WATER_STEP, func(_x: float, _z: float) -> float: return 0.0, depth_colour, null)


## A square grid of 2·HALF metres. y_of gives each vertex's height; colour_of (optional) its colour.
## Terrain normals are averaged from its triangles (flat up for the water).
static func _grid(step: float, y_of: Callable, colour_of: Callable, shape: IslandShape) -> ArrayMesh:
	var n := int(round(HALF * 2.0 / step)) + 1
	var verts := PackedVector3Array()
	var normals := PackedVector3Array()
	var colours := PackedColorArray()
	verts.resize(n * n)
	normals.resize(n * n)
	if colour_of.is_valid():
		colours.resize(n * n)
	for row in n:
		for col in n:
			var x := -HALF + col * step
			var z := -HALF + row * step
			var i := row * n + col
			verts[i] = Vector3(x, y_of.call(x, z), z)
			normals[i] = Vector3.UP
			if colour_of.is_valid():
				colours[i] = colour_of.call(x, z)
	var indices := _triangles(verts, n)
	if shape:
		normals = _face_normals(verts, indices) if smooth_edges else _height_normals(shape, verts)
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = verts
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = indices
	if colour_of.is_valid():
		arrays[Mesh.ARRAY_COLOR] = colours
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


## Two triangles per grid cell. Each cell is split along the diagonal whose two corners are
## closest in height, so the diagonal runs ALONG a slope instead of across it. Splitting every
## cell the same way makes steep edges (the bank) zigzag like a saw blade.
static func _triangles(verts: PackedVector3Array, n: int) -> PackedInt32Array:
	var indices := PackedInt32Array()
	indices.resize((n - 1) * (n - 1) * 6)
	var k := 0
	for row in n - 1:
		for col in n - 1:
			var a := row * n + col  # a b
			var b := a + 1          # c d
			var c := a + n
			var d := c + 1
			var along_ad := not smooth_edges or absf(verts[a].y - verts[d].y) <= absf(verts[b].y - verts[c].y)
			for idx in ([a, b, d, a, d, c] if along_ad else [a, b, c, b, d, c]):
				indices[k] = idx
				k += 1
	return indices


## Smooth vertex normals from the triangles themselves: each vertex averages the normals of the
## triangles around it (weighted by their area). These match the surface we actually draw, so the
## lighting follows the mesh — normals sampled from the height function at a finer scale than the
## grid disagree with the triangles at sharp edges and make the shading jagged.
static func _face_normals(verts: PackedVector3Array, indices: PackedInt32Array) -> PackedVector3Array:
	var normals := PackedVector3Array()
	normals.resize(verts.size())
	for t in range(0, indices.size(), 3):
		var i0 := indices[t]
		var i1 := indices[t + 1]
		var i2 := indices[t + 2]
		var face := (verts[i2] - verts[i0]).cross(verts[i1] - verts[i0])  # length = 2 × area
		normals[i0] += face
		normals[i1] += face
		normals[i2] += face
	for i in normals.size():
		normals[i] = normals[i].normalized()
	return normals


## The naive normals: slope of the height function sampled 0.1 m apart — finer than the 0.3 m grid,
## so at sharp edges they describe a surface the triangles don't have.
static func _height_normals(shape: IslandShape, verts: PackedVector3Array) -> PackedVector3Array:
	var normals := PackedVector3Array()
	normals.resize(verts.size())
	var e := 0.1
	for i in verts.size():
		var x := verts[i].x
		var z := verts[i].z
		var dx := shape.height(x + e, z) - shape.height(x - e, z)
		var dz := shape.height(x, z + e) - shape.height(x, z - e)
		normals[i] = Vector3(-dx, 2.0 * e, -dz).normalized()
	return normals
