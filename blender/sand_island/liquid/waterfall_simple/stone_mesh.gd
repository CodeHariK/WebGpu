## A stone's mesh, made the way the Blender file makes its 45 stones, with a seed so each one
## differs: an icosphere (20 triangles) split `subdivisions` times and pulled back onto the
## sphere (a smooth ball, radius 1), each vertex then pushed along its normal by a 3D noise (lumps
## `lump_size` across, `lump_strength` deep, 3 sizes), the bottom squashed flat so it sits.
## Normals are rebuilt from the new shape: smooth (round stones) or flat (faceted).
## Built at radius about 1: scale it with its node (the stone shaders read its local position as
## Blender's Generated: local * 0.5 + 0.5).

const GOLDEN := 1.618034


static func build(p_seed: int, p_subdivisions := 3, p_lump_size := 0.73, p_lump_strength := 0.5,
		p_flat_bottom := -0.55, p_flat_shading := false) -> ArrayMesh:
	var points: Array[Vector3] = []
	for p in [Vector3(-1, GOLDEN, 0), Vector3(1, GOLDEN, 0), Vector3(-1, -GOLDEN, 0), Vector3(1, -GOLDEN, 0),
			Vector3(0, -1, GOLDEN), Vector3(0, 1, GOLDEN), Vector3(0, -1, -GOLDEN), Vector3(0, 1, -GOLDEN),
			Vector3(GOLDEN, 0, -1), Vector3(GOLDEN, 0, 1), Vector3(-GOLDEN, 0, -1), Vector3(-GOLDEN, 0, 1)]:
		points.append(p.normalized())
	var faces := [[0, 11, 5], [0, 5, 1], [0, 1, 7], [0, 7, 10], [0, 10, 11], [1, 5, 9], [5, 11, 4],
			[11, 10, 2], [10, 7, 6], [7, 1, 8], [3, 9, 4], [3, 4, 2], [3, 2, 6], [3, 6, 8], [3, 8, 9],
			[4, 9, 5], [2, 4, 11], [6, 2, 10], [8, 6, 7], [9, 8, 1]]
	for i in p_subdivisions:
		faces = _split(points, faces)

	# the lumps: a Perlin fBM (3 sizes), like Blender's Clouds texture on its stones
	var noise := FastNoiseLite.new()
	noise.seed = p_seed
	noise.noise_type = FastNoiseLite.TYPE_PERLIN
	noise.frequency = 1.0 / p_lump_size
	noise.fractal_type = FastNoiseLite.FRACTAL_FBM
	noise.fractal_octaves = 3
	for i in points.size():
		var p := points[i]
		p += p * noise.get_noise_3dv(p * 1.7) * p_lump_strength # 1.7: the sphere's size in Blender units
		if p.y < p_flat_bottom: # squash the bottom: it sits on the ground
			p.y = p_flat_bottom + (p.y - p_flat_bottom) * 0.15
		points[i] = p

	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	if p_flat_shading:
		for f in faces:
			for k in [0, 2, 1]: # the list winds counter-clockwise from outside; Godot's front: clockwise
				tool.add_vertex(points[f[k]])
	else:
		for p in points:
			tool.add_vertex(p)
		for f in faces:
			tool.add_index(f[0])
			tool.add_index(f[2])
			tool.add_index(f[1])
	tool.generate_normals()
	return tool.commit()


# Each triangle into four, the new corners (the edges' middles) pushed out onto the sphere.
static func _split(p_points: Array[Vector3], p_faces: Array) -> Array:
	var middles := {} # edge (lower index, higher) -> the new point's index
	var result := []
	for f in p_faces:
		var m := []
		for k in 3:
			var a: int = f[k]
			var b: int = f[(k + 1) % 3]
			var key := Vector2i(mini(a, b), maxi(a, b))
			if not middles.has(key):
				middles[key] = p_points.size()
				p_points.append(((p_points[a] + p_points[b]) * 0.5).normalized())
			m.append(middles[key])
		result.append([f[0], m[0], m[2]])
		result.append([f[1], m[1], m[0]])
		result.append([f[2], m[2], m[1]])
		result.append([m[0], m[1], m[2]])
	return result
