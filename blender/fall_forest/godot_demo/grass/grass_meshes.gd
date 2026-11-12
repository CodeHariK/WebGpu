class_name GrassMeshes
extends RefCounted
## The two grass shapes, built in code, both opaque (no texture, no alpha: the shape is the
## triangles' own outline).
##   blade   one blade for near the camera: 7 vertices, 5 triangles, 1 wide and 1 tall (the
##           shader sizes, turns and bends it); three rows so it can curve when it bends
##   tuft    a clump of 3 single-triangle blades for far away: 9 vertices, 3 triangles, about
##           0.5 m across and 1 tall, its shading in the vertex colours (darker root, light tip)
## UV.y is how far up the blade a vertex is (0 root .. 1 tip): the shaders bend by it.

const BLADE_ROWS := [0.0, 0.4, 0.75] ## heights of the paired vertices; the tip is at 1
const BLADE_WIDTHS := [1.0, 0.75, 0.45] ## width at each row (x the shader's blade_width)


static func blade() -> ArrayMesh:
	var vertices := PackedVector3Array()
	var uvs := PackedVector2Array()
	for row in BLADE_ROWS.size():
		var half: float = BLADE_WIDTHS[row] * 0.5
		var y: float = BLADE_ROWS[row]
		vertices.append_array([Vector3(-half, y, 0.0), Vector3(half, y, 0.0)])
		uvs.append_array([Vector2(0.0, y), Vector2(1.0, y)])
	vertices.append(Vector3(0.0, 1.0, 0.0))
	uvs.append(Vector2(0.5, 1.0))
	var indices := PackedInt32Array([0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4, 4, 5, 6])
	return _mesh(vertices, uvs, PackedColorArray(), indices)


static func tuft() -> ArrayMesh:
	var vertices := PackedVector3Array()
	var uvs := PackedVector2Array()
	var colors := PackedColorArray()
	var heights := [1.0, 0.8, 0.9]
	for i in 3:
		var out := Vector3.FORWARD.rotated(Vector3.UP, i * TAU / 3.0 + 0.4)
		var side := out.cross(Vector3.UP)
		var base := out * 0.1
		vertices.append_array([base - side * 0.08, base + side * 0.08, out * 0.25 + Vector3.UP * heights[i]])
		uvs.append_array([Vector2(0.0, 0.0), Vector2(1.0, 0.0), Vector2(0.5, 1.0)])
		colors.append_array([Color(0.7, 0.7, 0.7), Color(0.7, 0.7, 0.7), Color(1.0, 1.0, 1.0)])
	return _mesh(vertices, uvs, colors, PackedInt32Array([0, 1, 2, 3, 4, 5, 6, 7, 8]))


static func _mesh(p_vertices: PackedVector3Array, p_uvs: PackedVector2Array, p_colors: PackedColorArray,
		p_indices: PackedInt32Array) -> ArrayMesh:
	var normals := PackedVector3Array()
	normals.resize(p_vertices.size())
	normals.fill(Vector3.UP) # the shaders make their own
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = p_vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = p_uvs
	if not p_colors.is_empty():
		arrays[Mesh.ARRAY_COLOR] = p_colors
	arrays[Mesh.ARRAY_INDEX] = p_indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh
