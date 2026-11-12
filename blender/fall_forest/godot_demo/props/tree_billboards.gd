class_name TreeBillboards
extends RefCounted
## A flat stand-in for a tree, for far away (tree_billboard.gdshader turns it to the camera).
## No texture, no alpha: the outline is the triangles' own, so it is opaque and cheap.
##   trunk     a rectangle
##   canopy    four blobs filling the top 2/3 of the tree: circles whose edge goes in and out
##             with a sine wave (puffy), each a fan of triangles with a lighter rim round it (the
##             outline); each blob a little nearer the camera than the one before, so where they
##             overlap they never flicker
##   colour    vertex colours: lighter in the middle of a blob, darker at its edge
## Sized from the 3D tree it stands in for (its bounding box), about 200 triangles against the
## tree's 1 500 - 3 300.

const SEGMENTS := 16 ## triangles round a blob
const BUMPS := 7 ## waves round a blob's edge
const BUMP_DEPTH := 0.07 ## of the radius
const RIM := 0.07 ## the outline's width, of the radius
const TRUNK := Color(0.93, 0.9, 0.86)
const LEAVES_MIDDLE := Color(1.0, 0.76, 0.53)
const LEAVES_EDGE := Color(0.97, 0.58, 0.45)
const LEAVES_RIM := Color(1.0, 0.86, 0.66)


## The stand-in for p_tree (a mesh from the glb), its blobs placed by p_seed.
static func build(p_tree: Mesh, p_seed: int) -> ArrayMesh:
	var box := p_tree.get_aabb()
	var width := maxf(box.size.x, box.size.z)
	var height := box.size.y
	var base := box.position.y
	var rng := RandomNumberGenerator.new()
	rng.seed = p_seed
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	_rectangle(surface, width * 0.09, base, base + height * 0.55, TRUNK)
	# [x, y, radius] as parts of the width and height: two side blobs, one under, one on top
	var blobs := [[-0.24, 0.6, 0.26], [0.24, 0.62, 0.26], [0.0, 0.5, 0.24], [0.0, 0.8, 0.3]]
	for i in blobs.size():
		var blob: Array = blobs[i]
		var radius: float = width * blob[2] * rng.randf_range(0.9, 1.1)
		var at := Vector2(width * blob[0] * rng.randf_range(0.85, 1.15), base + height * blob[1])
		_blob(surface, at, radius, width * 0.02 * i, rng.randf() * TAU)
	surface.index() # share the vertices the triangles have in common
	var mesh := surface.commit()
	mesh.custom_aabb = AABB(Vector3(-width * 0.5, base, -width * 0.5), Vector3(width, height, width))
	return mesh


## A fan from the middle out to a wavy edge, then a ring outside it in the rim colour.
static func _blob(p_surface: SurfaceTool, p_at: Vector2, p_radius: float, p_depth: float, p_phase: float) -> void:
	var edge: Array[Vector3] = []
	var rim: Array[Vector3] = []
	for i in SEGMENTS:
		var angle := TAU * i / SEGMENTS
		var r := p_radius * (1.0 + BUMP_DEPTH * sin(BUMPS * angle + p_phase))
		var out := Vector2(cos(angle), sin(angle))
		edge.append(Vector3(p_at.x + out.x * r, p_at.y + out.y * r, p_depth))
		rim.append(Vector3(p_at.x + out.x * r * (1.0 + RIM), p_at.y + out.y * r * (1.0 + RIM), p_depth))
	var centre := Vector3(p_at.x, p_at.y, p_depth)
	for i in SEGMENTS:
		var j := (i + 1) % SEGMENTS
		_triangle(p_surface, [centre, edge[i], edge[j]], [LEAVES_MIDDLE, LEAVES_EDGE, LEAVES_EDGE])
		_triangle(p_surface, [edge[i], rim[i], rim[j]], [LEAVES_RIM, LEAVES_RIM, LEAVES_RIM])
		_triangle(p_surface, [edge[i], rim[j], edge[j]], [LEAVES_RIM, LEAVES_RIM, LEAVES_RIM])


static func _rectangle(p_surface: SurfaceTool, p_width: float, p_bottom: float, p_top: float, p_colour: Color) -> void:
	var h := p_width * 0.5
	var corners := [Vector3(-h, p_bottom, 0.0), Vector3(h, p_bottom, 0.0), Vector3(h, p_top, 0.0), Vector3(-h, p_top, 0.0)]
	_triangle(p_surface, [corners[0], corners[1], corners[2]], [p_colour, p_colour, p_colour])
	_triangle(p_surface, [corners[0], corners[2], corners[3]], [p_colour, p_colour, p_colour])


static func _triangle(p_surface: SurfaceTool, p_points: Array, p_colours: Array) -> void:
	for k in 3:
		p_surface.set_color((p_colours[k] as Color).srgb_to_linear())
		p_surface.set_normal(Vector3.BACK)
		p_surface.add_vertex(p_points[k])
