## Procedural flat-shaded meshes for the parts whose shape comes from the layout: the island cake
## layers, wobbly discs (pond, pool, patches), syrup ribbons (falls, stream) and the faceted sea.
## Everything goes into a Builder, which keeps one SurfaceTool per palette colour.
extends RefCounted

const Palette := preload("res://choco_palette.gd")


## Collects triangles per colour and turns them into one ArrayMesh (one surface per colour).
class Builder:
	var _tools := {}

	func tool(color_name: String) -> SurfaceTool:
		if not _tools.has(color_name):
			var st := SurfaceTool.new()
			st.begin(Mesh.PRIMITIVE_TRIANGLES)
			_tools[color_name] = st
		return _tools[color_name]

	func commit() -> ArrayMesh:
		var mesh := ArrayMesh.new()
		for color_name: String in _tools:
			_tools[color_name].generate_tangents()  # the river shader's ripples need them
			_tools[color_name].commit(mesh)
			mesh.surface_set_material(mesh.get_surface_count() - 1, Palette.material(color_name))
		return mesh


## One flat triangle. The normal follows the winding; with culling off Godot flips it for the
## back side, so either winding lights correctly. UVs come from the world position (x / 4, z):
## that's the "flow" coordinate the river shader uses on the pool, pond and sea (drifting +Z).
static func tri(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3) -> void:
	tri_uv(st, a, b, c, world_uv(a), world_uv(b), world_uv(c))


## A flat triangle with explicit UVs (the ribbons: x across, y metres downstream).
static func tri_uv(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, ua: Vector2, ub: Vector2, uc: Vector2) -> void:
	st.set_normal((c - a).cross(b - a).normalized())
	st.set_uv(ua)
	st.add_vertex(a)
	st.set_uv(ub)
	st.add_vertex(b)
	st.set_uv(uc)
	st.add_vertex(c)


static func world_uv(p: Vector3) -> Vector2:
	return Vector2(p.x * 0.25, p.z)


static func quad(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, d: Vector3) -> void:
	tri(st, a, b, c)
	tri(st, a, c, d)


## A ring of points (x, z) lifted to height y.
static func lift(ring: PackedVector2Array, y: float, scale := 1.0, offset := Vector2.ZERO) -> Array[Vector3]:
	var out: Array[Vector3] = []
	for p in ring:
		var q := offset + p * scale
		out.append(Vector3(q.x, y, q.y))
	return out


## The side wall between two rings (same point count).
static func wall(b: Builder, color_name: String, low: Array[Vector3], high: Array[Vector3]) -> void:
	var st := b.tool(color_name)
	var n := low.size()
	for i in n:
		var j := (i + 1) % n
		quad(st, low[i], low[j], high[j], high[i])


## A flat cap over a ring (fan from its centre; fine for the star-shaped rings used here).
static func cap(b: Builder, color_name: String, ring: Array[Vector3]) -> void:
	var st := b.tool(color_name)
	var centre := Vector3.ZERO
	for p in ring:
		centre += p
	centre /= ring.size()
	for i in ring.size():
		tri(st, centre, ring[i], ring[(i + 1) % ring.size()])


## A wobbly disc ring of `sides` points around the origin, radius 1.
static func wobbly_ring(rng: RandomNumberGenerator, sides: int, wobble: float) -> PackedVector2Array:
	var ring := PackedVector2Array()
	for i in sides:
		var a := TAU * i / sides
		ring.append(Vector2(cos(a), sin(a)) * (1.0 + rng.randf_range(-wobble, wobble)))
	return ring


## A wobbly flat-topped disc (pond, pool, crumb patch) from y0 to y1.
static func disc(b: Builder, color_name: String, centre: Vector2, radius: float, y0: float, y1: float,
		ring: PackedVector2Array) -> void:
	var low := lift(ring, y0, radius, centre)
	var high := lift(ring, y1, radius, centre)
	wall(b, color_name, low, high)
	cap(b, color_name, high)


## The island cake: frosting, sponge, cream stripe and chocolate base, stacked down from y = 0.
## `outline` is the frosting edge (x, z) at full size.
static func island_cake(b: Builder, outline: PackedVector2Array, bottom: float) -> void:
	var layers := [  # colour, top y, bottom y, scale of the outline
		["frost_pink", 0.0, -0.6, 1.0],
		["sponge", -0.6, -1.9, 0.98],
		["cream", -1.9, -2.3, 0.99],
		["choco", -2.3, bottom, 0.97],
	]
	for layer in layers:
		var top := lift(outline, layer[1], layer[3])
		wall(b, layer[0], lift(outline, layer[2], layer[3]), top)
		cap(b, layer[0], top)


## A thick strip along `points` (a syrup fall or stream). The cross-section is a flat pentagon
## whose middle is pushed out by `bulge` on the front, so the strip catches light in facets.
## `side` fixes the across direction (falls); when it is zero the side is tangent x up (streams).
## UVs tell the river shader which way is downstream: x goes 0 → 1 across (left bank, middle,
## right bank; the underside repeats the banks), y is metres along the strip from its start.
static func ribbon(b: Builder, color_name: String, points: Array[Vector3], widths: Array[float],
		thick: float, side := Vector3.ZERO, bulge := 0.0) -> void:
	const ACROSS := [0.0, 0.5, 1.0, 1.0, 0.0]
	var rings: Array = []
	var along: Array[float] = []
	var length := 0.0
	for i in points.size():
		if i > 0:
			length += points[i].distance_to(points[i - 1])
		along.append(length)
		var t := (points[mini(i + 1, points.size() - 1)] - points[maxi(i - 1, 0)]).normalized()
		var s := side.normalized() if side != Vector3.ZERO else t.cross(Vector3.UP).normalized()
		var n := s.cross(t).normalized()
		var p := points[i]
		var h := widths[i] * 0.5
		rings.append([
			p - s * h + n * thick * 0.5, p + n * (thick * 0.5 + bulge), p + s * h + n * thick * 0.5,
			p + s * h - n * thick * 0.5, p - s * h - n * thick * 0.5,
		])
	var st := b.tool(color_name)
	for i in rings.size() - 1:
		for k in 5:
			var k1 := (k + 1) % 5
			var u0 := Vector2(ACROSS[k], along[i])
			var u1 := Vector2(ACROSS[k], along[i + 1])
			var u2 := Vector2(ACROSS[k1], along[i + 1])
			var u3 := Vector2(ACROSS[k1], along[i])
			tri_uv(st, rings[i][k], rings[i + 1][k], rings[i + 1][k1], u0, u1, u2)
			tri_uv(st, rings[i][k], rings[i + 1][k1], rings[i][k1], u0, u2, u3)
	for e in 2:
		var end: Array = rings[0] if e == 0 else rings[-1]
		var v := along[0] if e == 0 else along[-1]
		for k in range(1, 4):
			tri_uv(st, end[0], end[k], end[k + 1], Vector2(ACROSS[0], v), Vector2(ACROSS[k], v), Vector2(ACROSS[k + 1], v))


## The syrup sea: a jittered grid of triangles around the origin at height y.
static func sea(b: Builder, rng: RandomNumberGenerator, half_size: float, cells: int, y: float) -> void:
	var step := half_size * 2.0 / cells
	var grid: Array = []
	for j in cells + 1:
		var row: Array[Vector3] = []
		for i in cells + 1:
			var edge := i == 0 or j == 0 or i == cells or j == cells
			var jx := 0.0 if edge else rng.randf_range(-0.3, 0.3) * step
			var jz := 0.0 if edge else rng.randf_range(-0.3, 0.3) * step
			row.append(Vector3(-half_size + i * step + jx, y + rng.randf_range(-0.09, 0.09), -half_size + j * step + jz))
		grid.append(row)
	var st := b.tool("sea")
	for j in cells:
		for i in cells:
			quad(st, grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i])
