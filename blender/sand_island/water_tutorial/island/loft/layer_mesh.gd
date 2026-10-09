## Builds the mesh of one IslandLayer around an IslandLoft's plateau, as a loft of rings like the
## island itself. The cross-section ("profile") goes: top centre (or an inner rim) → out across the
## top → round the top bevel → down the side → round the bottom bevel → back in underneath.
## Vertex colour = the layer's colour (layer.gdshader reads it), so one material serves every layer.
class_name LayerMesh
extends RefCounted

const TUCK := 0.3   # how far inside the body the underside / inner rim ends (hidden, closes the gap)


static func build(loft: IslandLoft, layer: IslandLayer) -> ArrayMesh:
	var profile := _profile(layer)
	var rings: Array[PackedVector3Array] = []
	for point in profile:
		rings.append(_ring(loft, layer, point))
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	st.set_color(layer.colour)
	var group := 0
	if layer.cap:
		_cap(st, loft, layer, rings[0])
	for k in rings.size() - 1:
		st.set_smooth_group(group)
		for j in IslandLoft.SIDES:
			var jn := (j + 1) % IslandLoft.SIDES
			# ring k+1 is further along the profile (outwards, then down, then back in)
			_triangle(st, rings[k + 1][j], rings[k + 1][jn], rings[k][j])
			_triangle(st, rings[k + 1][jn], rings[k][jn], rings[k][j])
		if profile[k + 1].z > 0.5:  # a sharp corner: start a new smooth group
			group += 1
	st.generate_normals()
	st.index()
	return st.commit()


## The cross-section as (offset from the body's edge in metres, height, crease?) points.
## Drips are added per angle in _ring (they only move the bottom points down).
static func _profile(layer: IslandLayer) -> Array[Vector3]:
	var pts: Array[Vector3] = []
	var r := layer.overhang
	var b := clampf(layer.bevel, 0.0, minf(layer.thickness * 0.6, r + TUCK))
	var bb := clampf(layer.bottom_bevel, 0.0, layer.thickness - b)
	var bottom := layer.top - layer.thickness
	var sharp := 1.0 if b < 0.01 else 0.0
	var sharp_bottom := 1.0 if bb < 0.01 else 0.0
	pts.append(Vector3(0.0 if layer.cap else -TUCK, layer.top, 0.0))  # cap: joins the flat top; band: starts inside
	pts.append(Vector3(r - b, layer.top, sharp))
	if b >= 0.01:  # top bevel: a quarter circle from the top surface round to the side
		for i in range(1, layer.segments + 1):
			var a := PI / 2.0 * (1.0 - float(i) / layer.segments)
			pts.append(Vector3(r - b + b * cos(a), layer.top - b + b * sin(a), 0.0))
	pts.append(Vector3(r, bottom + bb, sharp_bottom))
	if bb >= 0.01:  # bottom bevel: from the side round to the underside
		for i in range(1, layer.segments + 1):
			var a := -PI / 2.0 * float(i) / layer.segments
			pts.append(Vector3(r - bb + bb * cos(a), bottom + bb + bb * sin(a), 0.0))
	pts.append(Vector3(-TUCK, bottom, 1.0))  # underside, back inside the body
	return pts


## One ring of the profile point `p`, all the way round. Points below the top get pulled down by
## the drip noise (more at the bottom edge, none at the top), so the lower edge looks drippy.
static func _ring(loft: IslandLoft, layer: IslandLayer, p: Vector3) -> PackedVector3Array:
	var ring := PackedVector3Array()
	ring.resize(IslandLoft.SIDES)
	var depth_share := clampf((layer.top - p.y) / layer.thickness, 0.0, 1.0)
	for j in IslandLoft.SIDES:
		var angle := TAU * j / IslandLoft.SIDES
		var body := loft.body_offset_at(angle, layer.top)
		var flat := loft.plateau.point(angle, body + p.x)
		var drip := layer.drip * _drip_shape(angle) * depth_share * depth_share
		ring[j] = Vector3(flat.x, p.y - drip, flat.y)
	return ring


## 0..1 around the island: a few bumps of different widths, like icing running down.
static func _drip_shape(angle: float) -> float:
	var v := sin(angle * 7.0) * 0.5 + sin(angle * 13.0 + 1.3) * 0.3 + sin(angle * 23.0 + 0.4) * 0.2
	return clampf(v, 0.0, 1.0)


## The flat top: rings shrinking towards the plateau centre, then a fan to the centre.
static func _cap(st: SurfaceTool, loft: IslandLoft, layer: IslandLayer, edge: PackedVector3Array) -> void:
	var rings: Array[PackedVector3Array] = []
	for scale in [0.3, 0.55, 0.8]:
		var ring := PackedVector3Array()
		ring.resize(IslandLoft.SIDES)
		for j in IslandLoft.SIDES:
			var p := loft.plateau.centre.lerp(Vector2(edge[j].x, edge[j].z), scale)
			ring[j] = Vector3(p.x, layer.top + 0.04 * sin(p.x * 1.3) * cos(p.y * 1.1), p.y)
		rings.append(ring)
	rings.append(edge)
	var centre := Vector3(loft.plateau.centre.x, layer.top, loft.plateau.centre.y)
	st.set_smooth_group(0)
	for j in IslandLoft.SIDES:
		_triangle(st, rings[0][j], rings[0][(j + 1) % IslandLoft.SIDES], centre)
	for k in rings.size() - 1:
		for j in IslandLoft.SIDES:
			var jn := (j + 1) % IslandLoft.SIDES
			_triangle(st, rings[k + 1][j], rings[k + 1][jn], rings[k][j])
			_triangle(st, rings[k + 1][jn], rings[k][jn], rings[k][j])


static func _triangle(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3) -> void:
	st.add_vertex(a)
	st.add_vertex(b)
	st.add_vertex(c)
