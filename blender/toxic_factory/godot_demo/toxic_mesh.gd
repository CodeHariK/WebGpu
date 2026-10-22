## Procedural flat-shaded meshes for the parts whose shape comes from the layout: the ground with
## its river channel, slabs (yard, road), road markings, pipes (tubes with rounded bends) and
## ribbons (the river surface, sludge pours). Everything goes into a Builder (one SurfaceTool per
## material name, see toxic_palette.gd).
extends RefCounted

const Palette := preload("res://toxic_palette.gd")
const Layout := preload("res://toxic_layout.gd")


## Collects triangles per material name and turns them into one ArrayMesh.
class Builder:
	var _tools := {}

	func tool(name: String) -> SurfaceTool:
		if not _tools.has(name):
			var st := SurfaceTool.new()
			st.begin(Mesh.PRIMITIVE_TRIANGLES)
			_tools[name] = st
		return _tools[name]

	func commit() -> ArrayMesh:
		var mesh := ArrayMesh.new()
		for name: String in _tools:
			_tools[name].generate_tangents()  # the river shader's ripples need them
			_tools[name].commit(mesh)
			mesh.surface_set_material(mesh.get_surface_count() - 1, Palette.material(name))
		return mesh


## One flat triangle; UVs default to the world position (x, z) / 4.
static func tri(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3) -> void:
	tri_uv(st, a, b, c, Vector2(a.x, a.z) * 0.25, Vector2(b.x, b.z) * 0.25, Vector2(c.x, c.z) * 0.25)


static func tri_uv(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, ua: Vector2, ub: Vector2, uc: Vector2) -> void:
	st.set_normal((c - a).cross(b - a).normalized())
	st.set_uv(ua)
	st.add_vertex(a)
	st.set_uv(ub)
	st.add_vertex(b)
	st.set_uv(uc)
	st.add_vertex(c)


static func quad(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, d: Vector3) -> void:
	tri(st, a, b, c)
	tri(st, a, c, d)


## A flat box slab (yard, road, markings): x/z rectangle from y0 to y1, top and sides.
static func slab(b: Builder, name: String, rect: Rect2, y0: float, y1: float) -> void:
	var st := b.tool(name)
	var p := rect.position
	var q := rect.end
	var top := [Vector3(p.x, y1, p.y), Vector3(q.x, y1, p.y), Vector3(q.x, y1, q.y), Vector3(p.x, y1, q.y)]
	var low := [Vector3(p.x, y0, p.y), Vector3(q.x, y0, p.y), Vector3(q.x, y0, q.y), Vector3(p.x, y0, q.y)]
	quad(st, top[0], top[1], top[2], top[3])
	for i in 4:
		var j := (i + 1) % 4
		quad(st, low[i], low[j], top[j], top[i])


## The ground: a jittered grid following Layout.ground_y, flat dead-grass on top, mud on the banks.
static func ground(b: Builder, rng: RandomNumberGenerator, cell: float) -> void:
	var n := int(Layout.MAP_HALF * 2.0 / cell)
	var pts: Array = []
	for j in n + 1:
		var row: Array[Vector3] = []
		for i in n + 1:
			var edge := i == 0 or j == 0 or i == n or j == n
			var x := -Layout.MAP_HALF + i * cell + (0.0 if edge else rng.randf_range(-0.25, 0.25) * cell)
			var z := -Layout.MAP_HALF + j * cell + (0.0 if edge else rng.randf_range(-0.25, 0.25) * cell)
			var y := Layout.ground_y(x, z)
			if y == 0.0 and not edge:
				y += rng.randf_range(-0.06, 0.06)
			row.append(Vector3(x, y, z))
		pts.append(row)
	for j in n:
		for i in n:
			var a: Vector3 = pts[j][i]
			var c: Vector3 = pts[j + 1][i + 1]
			for t in [[a, pts[j][i + 1], c], [a, c, pts[j + 1][i]]]:
				var low := minf(t[0].y, minf(t[1].y, t[2].y))
				var name := "dirt" if low > -0.3 else ("mud" if low > Layout.WATER_Y - 0.3 else "riverbed")
				tri(b.tool(name), t[0], t[1], t[2])


## A thick strip along `points` with river UVs: x 0 → 1 across, y metres along. The cross-section
## is a flat pentagon bulged by `bulge` on its front. `side` fixes the across direction; zero
## means tangent x up (lying flat).
static func ribbon(b: Builder, name: String, points: Array[Vector3], widths: Array[float], thick: float,
		side := Vector3.ZERO, bulge := 0.0) -> void:
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
		var nrm := s.cross(t).normalized()
		var p := points[i]
		var h := widths[i] * 0.5
		rings.append([p - s * h + nrm * thick * 0.5, p + nrm * (thick * 0.5 + bulge), p + s * h + nrm * thick * 0.5,
			p + s * h - nrm * thick * 0.5, p - s * h - nrm * thick * 0.5])
	var st := b.tool(name)
	for i in rings.size() - 1:
		for k in 5:
			var k1 := (k + 1) % 5
			var u0 := Vector2(ACROSS[k], along[i])
			var u1 := Vector2(ACROSS[k], along[i + 1])
			var u2 := Vector2(ACROSS[k1], along[i + 1])
			var u3 := Vector2(ACROSS[k1], along[i])
			tri_uv(st, rings[i][k], rings[i + 1][k], rings[i + 1][k1], u0, u1, u2)
			tri_uv(st, rings[i][k], rings[i + 1][k1], rings[i][k1], u0, u2, u3)


## A pipe along a polyline: corners are rounded (bend metres each side), the tube has `sides`
## faces and its rings follow the path without twisting. Returns the smoothed centre line.
static func tube(b: Builder, name: String, corners: Array[Vector3], radius: float, sides := 8, bend := 1.0) -> Array[Vector3]:
	var path: Array[Vector3] = [corners[0]]
	for i in range(1, corners.size() - 1):
		var a := corners[i] + (corners[i - 1] - corners[i]).normalized() * minf(bend, corners[i].distance_to(corners[i - 1]) * 0.45)
		var c := corners[i] + (corners[i + 1] - corners[i]).normalized() * minf(bend, corners[i].distance_to(corners[i + 1]) * 0.45)
		for k in 5:  # quadratic bezier a → corner → c
			var t := k / 4.0
			path.append(a.lerp(corners[i], t).lerp(corners[i].lerp(c, t), t))
	path.append(corners[-1])
	var st := b.tool(name)
	var normal := Vector3.ZERO
	var rings: Array = []
	for i in path.size():
		var t := (path[mini(i + 1, path.size() - 1)] - path[maxi(i - 1, 0)]).normalized()
		if normal == Vector3.ZERO:
			normal = t.cross(Vector3.UP if absf(t.y) < 0.9 else Vector3.RIGHT).normalized()
		normal = (normal - t * normal.dot(t)).normalized()  # carry the frame along (no twist)
		var binormal := t.cross(normal)
		var ring: Array[Vector3] = []
		for k in sides:
			var a := TAU * k / sides
			ring.append(path[i] + (normal * cos(a) + binormal * sin(a)) * radius)
		rings.append(ring)
	for i in rings.size() - 1:
		for k in sides:
			var k1 := (k + 1) % sides
			quad(st, rings[i][k], rings[i + 1][k], rings[i + 1][k1], rings[i][k1])
	return path
