## A procedural island built as a loft: rings of points around the island, stacked from the far
## seabed inwards to the top of the plateau, then skinned with triangles between neighbouring rings.
##
##   far seabed → seabed → beach edge → beach slope rings → bank (BankProfile) → plateau top → centre
##
## Unlike a heightmap grid, rings can sit straight above each other (vertical walls) or even lean
## out (overhangs), edges are exactly where we put a ring (no grid stair-steps), and the triangle
## count only depends on SIDES × rings (about 3k triangles vs 22k for the 0.3 m grid).
## The water is made from the same columns, so its depth is exact (see water_mesh()).
class_name IslandLoft
extends RefCounted

const SIDES := 128            # points per ring (outline smoothness)
const WATER_ROWS := 16        # water rings from the shore out to the far edge
const MAX_DEPTH := 2.0        # depth that maps to vertex colour 1.0

var beach_top := 0.75         # beach height at the foot of the bank
var beach_edge := -0.4        # height at the beach outline (already under water)
var seabed := -2.2
var plateau_height := 1.7
var style: BankProfile.Style = BankProfile.Style.WALL

var beach: IslandOutline
var plateau: IslandOutline
var rings: Array[PackedVector3Array] = []   # outer → inner, SIDES points each
var creases: Array[bool] = []               # hard edge at this ring?
var centre_top := Vector3.ZERO


func _init(island_seed: int, p_style: BankProfile.Style) -> void:
	style = p_style
	var rng := RandomNumberGenerator.new()
	rng.seed = island_seed
	var angle := rng.randf_range(-PI, PI)
	var plateau_centre := Vector2(cos(angle), sin(angle)) * rng.randf_range(1.5, 3.5)
	beach = IslandOutline.new(Vector2.ZERO, 10.5, 0.1, island_seed, 0.0)
	plateau = IslandOutline.new(plateau_centre, rng.randf_range(3.8, 5.5), 0.14, island_seed, 50.0)
	_build_rings()


# --- rings -------------------------------------------------------------------------------------

func _build_rings() -> void:
	for scale in [2.3, 1.5, 1.15]:  # flat far seabed, then rising towards the beach
		var y := seabed if scale > 1.4 else lerpf(beach_edge, seabed, 0.45)
		_ring(func(a: float) -> Vector3: return _on_ground(_scaled(beach, a, scale), y))
	_ring(func(a: float) -> Vector3: return _on_ground(beach.point(a), beach_edge))
	for t in [0.2, 0.4, 0.6, 0.8]:  # the beach rises gently to the foot of the bank
		var y := beach_top + (beach_edge - beach_top) * pow(1.0 - t, 1.6)
		_ring(func(a: float) -> Vector3: return _on_ground(beach.point(a).lerp(_bank_point(a, 0.0), t), y))
	for s in BankProfile.samples(style):
		var y := beach_top + (plateau_height - beach_top) * BankProfile.at(style, s).y
		_ring(func(a: float) -> Vector3: return _on_ground(_bank_point(a, s), y), BankProfile.crease_at(style, s))
	for scale in [0.8, 0.55, 0.3]:  # grass top, with a few gentle lumps
		_ring(func(a: float) -> Vector3: return _top_point(a, scale))
	centre_top = _on_ground(plateau.centre, plateau_height)


func _top_point(angle: float, scale: float) -> Vector3:
	var p := _scaled(plateau, angle, scale)
	return _on_ground(p, plateau_height + 0.05 * sin(p.x * 1.3) * cos(p.y * 1.1))


func _ring(position_at: Callable, crease: bool = false) -> void:
	var ring := PackedVector3Array()
	ring.resize(SIDES)
	for j in SIDES:
		ring[j] = position_at.call(TAU * j / SIDES)
	rings.append(ring)
	creases.append(crease)


## How far the bank sits outside the plateau outline at `height` (follows the bank style), so a
## layer wrapped around the body hugs it. Above the top it's the top edge's offset.
func body_offset_at(_angle: float, height: float) -> float:
	var s := clampf((height - beach_top) / (plateau_height - beach_top), 0.0, 1.0)
	return BankProfile.at(style, s).x


## A point on the bank at profile position s, in direction `angle` from the plateau centre.
func _bank_point(angle: float, s: float) -> Vector2:
	return plateau.point(angle, BankProfile.at(style, s).x)


## Outline point moved towards (scale < 1) or away from (scale > 1) its centre.
static func _scaled(outline: IslandOutline, angle: float, scale: float) -> Vector2:
	return outline.centre + (outline.point(angle) - outline.centre) * scale


static func _on_ground(p: Vector2, y: float) -> Vector3:
	return Vector3(p.x, y, p.y)


# --- terrain mesh ------------------------------------------------------------------------------

## Triangles between every pair of neighbouring rings, then a fan to the centre. Each band gets a
## smooth group; at a crease ring the group changes, so SurfaceTool keeps that edge sharp.
func terrain_mesh() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var group := 0
	for k in rings.size() - 1:
		st.set_smooth_group(group)
		for j in SIDES:
			var jn := (j + 1) % SIDES
			_quad(st, rings[k][j], rings[k][jn], rings[k + 1][j], rings[k + 1][jn])
		if creases[k + 1]:
			group += 1
	st.set_smooth_group(group)
	var last := rings[rings.size() - 1]
	for j in SIDES:
		_triangle(st, last[j], last[(j + 1) % SIDES], centre_top)
	st.generate_normals()
	st.index()
	return st.commit()


## a, b on the outer ring; c, d on the inner ring (b, d one step further round).
static func _quad(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, d: Vector3) -> void:
	_triangle(st, a, b, c)
	_triangle(st, b, d, c)


static func _triangle(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3) -> void:
	st.add_vertex(a)
	st.add_vertex(b)
	st.add_vertex(c)


# --- water mesh --------------------------------------------------------------------------------

## The sea as rings too. Each column j of the terrain (the same angle on every ring) is a path from
## the island top down to the far seabed. We find where that path crosses y = 0 (the shoreline),
## then place WATER_ROWS points along the path from there outwards — denser near the shore — and
## store the terrain height under each point as depth. Because water vertices sit exactly on the
## terrain's columns, the depth is exact: the foam band lines up with the real shoreline.
func water_mesh() -> ArrayMesh:
	var verts := PackedVector3Array()
	var colours := PackedColorArray()
	verts.resize(WATER_ROWS * SIDES)
	colours.resize(WATER_ROWS * SIDES)
	for j in SIDES:
		var path := _underwater_path(j)
		var length := _path_length(path)
		for i in WATER_ROWS:
			var along := pow(float(i) / (WATER_ROWS - 1), 1.8) * length
			var p := _point_along(path, along)
			verts[i * SIDES + j] = Vector3(p.x, 0.0, p.z)
			colours[i * SIDES + j] = Color(clampf(-p.y / MAX_DEPTH, 0.0, 1.0), 0.0, 0.0)
	var indices := PackedInt32Array()
	for i in WATER_ROWS - 1:  # row i is nearer the shore (inner), row i + 1 further out (outer)
		for j in SIDES:
			var jn := (j + 1) % SIDES
			var a := (i + 1) * SIDES + j
			var b := (i + 1) * SIDES + jn
			var c := i * SIDES + j
			var d := i * SIDES + jn
			indices.append_array([a, b, c, b, d, c])
	var normals := PackedVector3Array()
	normals.resize(verts.size())
	normals.fill(Vector3.UP)
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = verts
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colours
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


## Column j from the shoreline crossing outwards to the far seabed (terrain points, y < 0).
func _underwater_path(j: int) -> PackedVector3Array:
	var path := PackedVector3Array()
	for k in range(rings.size() - 1, 0, -1):  # inner → outer
		var inner := rings[k][j]
		var outer := rings[k - 1][j]
		if path.is_empty() and inner.y >= 0.0 and outer.y < 0.0:
			path.append(inner.lerp(outer, inner.y / (inner.y - outer.y)))  # where it crosses y = 0
		if not path.is_empty():
			path.append(outer)
	return path


static func _path_length(path: PackedVector3Array) -> float:
	var total := 0.0
	for i in path.size() - 1:
		total += Vector2(path[i].x, path[i].z).distance_to(Vector2(path[i + 1].x, path[i + 1].z))
	return total


## The point `along` metres (measured on the ground plane) down the path, height interpolated.
static func _point_along(path: PackedVector3Array, along: float) -> Vector3:
	for i in path.size() - 1:
		var step := Vector2(path[i].x, path[i].z).distance_to(Vector2(path[i + 1].x, path[i + 1].z))
		if along <= step or i == path.size() - 2:
			return path[i].lerp(path[i + 1], clampf(along / maxf(step, 1e-6), 0.0, 1.0))
		along -= step
	return path[path.size() - 1]
