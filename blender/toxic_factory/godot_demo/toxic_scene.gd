## Toxic Factory scene: builds everything that doesn't move.
##   ground + river channel, yard, road + markings          (toxic_mesh.gd)
##   buildings, barricades, fences, barrels, trees, signs   (kit pieces, one MultiMesh per piece)
##   pipes over the road to the outfalls, sludge pours, the polluted river, the yard sludge pond
##   gate posts + arms (the arms are separate nodes: toxic_traffic.gd lifts them)
## Collision: the ground mesh, plus a box per building from its mesh bounds.
extends Node3D

const Kit := preload("res://toxic_kit.gd")
const Mesh_ := preload("res://toxic_mesh.gd")
const Layout := preload("res://toxic_layout.gd")
const Palette := preload("res://toxic_palette.gd")

const YARD_Y := 0.08
const POND := Vector3(-6.0, 0.0, -30.0)  # x, -, z of the sludge pond in the yard
const POND_RADIUS := 3.2

var kit := Kit.new()
var chimney_tops: Array[Vector3] = []  # smoke goes here (toxic_effects.gd)
var tower_tops: Array[Vector3] = []
var vent_tops: Array[Vector3] = []
var gates: Array[Dictionary] = []  # {"centre": Vector3, "arm": Node3D}

var _placed := {}
var _body: StaticBody3D
var _rng := RandomNumberGenerator.new()


func build(seed_value: int) -> void:
	_rng.seed = seed_value
	_body = StaticBody3D.new()
	_body.name = "Collision"
	add_child(_body)
	_ground()
	_yard_and_road()
	_buildings()
	_pipes()
	_water()
	_barricades()
	_fence_and_gates()
	_props()
	_build_multimeshes()


func _ground() -> void:
	var b := Mesh_.Builder.new()
	Mesh_.ground(b, _rng, 2.5)
	var mesh := _add_mesh("Ground", b)
	_add_shape(mesh.create_trimesh_shape(), Transform3D.IDENTITY)


func _yard_and_road() -> void:
	var b := Mesh_.Builder.new()
	Mesh_.slab(b, "concrete", Layout.COMPOUND, -0.3, YARD_Y)
	Mesh_.slab(b, "asphalt", Rect2(-Layout.MAP_HALF, Layout.ROAD_Z0, Layout.MAP_HALF * 2, Layout.ROAD_Z1 - Layout.ROAD_Z0), -0.3, 0.06)
	for gx in [Layout.GATE_IN_X, Layout.GATE_OUT_X]:  # driveways from the road into the yard
		Mesh_.slab(b, "asphalt", Rect2(gx - Layout.GATE_WIDTH / 2, 0.0, Layout.GATE_WIDTH, Layout.ROAD_Z0 + 0.5), -0.3, 0.1)
	var lane := Rect2(Layout.GATE_IN_X - 3.5, Layout.YARD_LANE_Z - 2.5, Layout.GATE_OUT_X - Layout.GATE_IN_X + 7.0, 3.5)
	Mesh_.slab(b, "asphalt", lane, -0.3, 0.1)
	var mid := (Layout.ROAD_Z0 + Layout.ROAD_Z1) * 0.5
	var x := -Layout.MAP_HALF
	while x < Layout.MAP_HALF:  # dashed centre line
		Mesh_.slab(b, "marking", Rect2(x, mid - 0.1, 3.0, 0.2), 0.0, 0.075)
		x += 6.0
	for z in [Layout.ROAD_Z0 + 0.25, Layout.ROAD_Z1 - 0.4]:  # edge lines
		Mesh_.slab(b, "marking", Rect2(-Layout.MAP_HALF, z, Layout.MAP_HALF * 2, 0.15), 0.0, 0.075)
	_add_mesh("YardAndRoad", b)


func _buildings() -> void:
	for item in Layout.BUILDINGS:
		var xf := Transform3D(Basis(Vector3.UP, deg_to_rad(item[3])), Vector3(item[1], YARD_Y, item[2]))
		_place(item[0], xf)
		var box: AABB = kit.mesh(item[0]).get_aabb()
		var shape := BoxShape3D.new()
		shape.size = box.size
		_add_shape(shape, xf * Transform3D(Basis.IDENTITY, box.get_center()))
		match item[0]:
			"Chimney":
				chimney_tops.append(Vector3(item[1], Layout.CHIMNEY_TOP + YARD_Y, item[2]))
			"CoolingTower":
				tower_tops.append(Vector3(item[1], Layout.TOWER_TOP + YARD_Y, item[2]))
			"Plant_main":
				for vx in [-4.5, 0.0, 4.5]:
					vent_tops.append(Vector3(item[1] + vx, 10.9, item[2] - 3.6))


## The outfall pipes (orange) over the road, and the feed pipe (grey) from the tank to the plant.
func _pipes() -> void:
	var b := Mesh_.Builder.new()
	for ox: float in Layout.OUTFALL_X:
		var path := Mesh_.tube(b, "pipe", Layout.outfall_pipe(ox), Layout.PIPE_RADIUS, 10, 1.2)
		_flanges(path)
		for z in [-4.0, 1.8]:  # supports for the high run (none on the road)
			_place("PipeSupport", Transform3D(Basis.from_scale(Vector3(1, Layout.PIPE_HEIGHT - Layout.PIPE_RADIUS, 1)), Vector3(ox, YARD_Y if z < 1.0 else 0.0, z)))
		var mouth := Layout.outfall_mouth(ox)
		_place("Outfall", Transform3D(Basis.IDENTITY, mouth))
		_place("Valve", Transform3D(Basis(Vector3.RIGHT, -PI / 2), Vector3(ox, 3.5, 11.0 - Layout.PIPE_RADIUS - 0.1)))
	var feed: Array[Vector3] = []
	feed.assign(Layout.FEED_PIPE)
	var path := Mesh_.tube(b, "pipe_grey", feed, 0.35, 8, 0.9)
	_flanges(path)
	for i in feed.size() - 1:
		var a := feed[i]
		var c := feed[i + 1]
		var along_x := absf(c.x - a.x) > absf(c.z - a.z)
		var mid := (a + c) * 0.5
		_place("PipeSupport", Transform3D(Basis(Vector3.UP, PI / 2 if not along_x else 0.0) * Basis.from_scale(Vector3(1, a.y - 0.35, 1)), Vector3(mid.x, YARD_Y, mid.z)))
	_add_mesh("Pipes", b)


## Flange rings every few metres along a pipe's centre line.
func _flanges(path: Array[Vector3]) -> void:
	var next := 1.5
	var travelled := 0.0
	for i in range(1, path.size()):
		var seg := path[i] - path[i - 1]
		travelled += seg.length()
		if travelled >= next:
			var t := seg.normalized()
			var up := Vector3.UP if absf(t.y) < 0.9 else Vector3.BACK
			_place("Flange", Transform3D(Basis.looking_at(t, up), path[i]))
			next = travelled + 3.5


## The polluted river, the sludge pouring out of each outfall, and the yard's sludge pond.
func _water() -> void:
	var b := Mesh_.Builder.new()
	var points: Array[Vector3] = []
	var widths: Array[float] = []
	var x := -Layout.MAP_HALF
	while x <= Layout.MAP_HALF + 0.1:
		points.append(Vector3(x, Layout.WATER_Y, Layout.river_z(x)))
		widths.append(Layout.WATER_HALF * 2.0)
		x += 2.5
	Mesh_.ribbon(b, "river", points, widths, 0.3)
	var sources: Array[Vector2] = []
	for ox: float in Layout.OUTFALL_X:
		var m := Layout.outfall_mouth(ox)
		var water_z := Layout.river_near(ox) - 1.5
		var pour: Array[Vector3] = [
			m + Vector3(0, 0, 0.1), m + Vector3(0, -0.12, 0.7), m + Vector3(0, -0.7, 1.3),
			Vector3(ox, Layout.WATER_Y + 0.3, water_z), Vector3(ox + 0.6, Layout.WATER_Y + 0.12, water_z + 1.6),
		]
		var w: Array[float] = [0.75, 0.85, 1.1, 1.8, 3.2]
		Mesh_.ribbon(b, "pour", pour, w, 0.25, Vector3(-1, 0, 0), 0.1)
		sources.append(Vector2(ox, water_z + 1.0))
	Palette.set_outfalls(sources)
	# the sludge pond: a wobbly disc of sludge inside a concrete rim
	var ring := PackedVector2Array()
	for i in 12:
		var a := TAU * i / 12
		ring.append(Vector2(cos(a), sin(a)) * POND_RADIUS * (1.0 + _rng.randf_range(-0.08, 0.08)))
	var st := b.tool("pond")
	var c := Vector3(POND.x, YARD_Y + 0.25, POND.z)
	for i in 12:
		var p := ring[i]
		var q := ring[(i + 1) % 12]
		Mesh_.tri(st, c, c + Vector3(p.x, 0, p.y), c + Vector3(q.x, 0, q.y))
	_add_mesh("Water", b)
	var rim := Mesh_.Builder.new()
	Mesh_.slab(rim, "concrete", Rect2(POND.x - POND_RADIUS - 0.8, POND.z - POND_RADIUS - 0.8, (POND_RADIUS + 0.8) * 2, (POND_RADIUS + 0.8) * 2), YARD_Y, YARD_Y + 0.15)
	_add_mesh("PondRim", rim)


## Jersey barriers along the river side of the road (alternating red/white), cones by the gates,
## sawhorses and hazard signs blocking the bank round each outfall.
func _barricades() -> void:
	var x := -Layout.MAP_HALF + 1.5
	var i := 0
	while x < Layout.MAP_HALF:
		_place("Jersey_red" if i % 2 == 0 else "Jersey_white", Transform3D(Basis.IDENTITY, Vector3(x, 0, Layout.ROAD_Z1 + 0.6)))
		x += 3.0
		i += 1
	for gx in [Layout.GATE_IN_X, Layout.GATE_OUT_X]:
		for k in 4:
			_place("Cone", Transform3D(Basis(Vector3.UP, _rng.randf() * TAU), Vector3(gx - 5.0 - k * 1.2, 0.06, Layout.ROAD_Z0 + 0.6)))
			_place("Cone", Transform3D(Basis(Vector3.UP, _rng.randf() * TAU), Vector3(gx + 5.0 + k * 1.2, 0.06, Layout.ROAD_Z0 + 0.6)))
	for ox: float in Layout.OUTFALL_X:
		var top := Layout.river_near(ox) - Layout.BANK
		for s in [-1.0, 1.0]:
			_place("Sawhorse", Transform3D(Basis.IDENTITY, Vector3(ox + s * 2.6, 0, top - 1.2)))
			_place("Cone", Transform3D(Basis.IDENTITY, Vector3(ox + s * 1.6, 0, top - 1.6)))
		_place("HazardSign", Transform3D(Basis(Vector3.UP, PI), Vector3(ox + 1.8, 0, top - 2.6)))


## The compound fence (4 m bays) with openings for the two gates; gate posts and lifting arms.
func _fence_and_gates() -> void:
	var r := Layout.COMPOUND
	var front := r.end.y
	var x := r.position.x + 2.0
	while x < r.end.x:
		var in_gate := absf(x - Layout.GATE_IN_X) < Layout.GATE_WIDTH / 2 + 1.9 or absf(x - Layout.GATE_OUT_X) < Layout.GATE_WIDTH / 2 + 1.9
		if not in_gate:
			_place("Fence", Transform3D(Basis.IDENTITY, Vector3(x, YARD_Y, front)))
			if int(x) % 12 == 0:
				_place("HazardSign", Transform3D(Basis.IDENTITY, Vector3(x, YARD_Y, front + 0.4)))
		_place("Fence", Transform3D(Basis.IDENTITY, Vector3(x, YARD_Y, r.position.y)))
		x += 4.0
	var z := r.position.y + 2.0
	while z < front:
		for side in [r.position.x, r.end.x]:
			_place("Fence", Transform3D(Basis(Vector3.UP, PI / 2), Vector3(side, YARD_Y, z)))
		z += 4.0
	for gx in [Layout.GATE_IN_X, Layout.GATE_OUT_X]:
		var hinge := Vector3(gx - Layout.GATE_WIDTH / 2 - 0.2, 0.1, front + 0.6)
		_place("GatePost", Transform3D(Basis.IDENTITY, hinge))
		_place("GatePost", Transform3D(Basis.IDENTITY, hinge + Vector3(Layout.GATE_WIDTH + 0.4, 0, 0)))
		var arm := MeshInstance3D.new()
		arm.name = "GateArm"
		arm.mesh = kit.mesh("GateArm")
		arm.position = hinge + Vector3(0, 1.1, 0)
		add_child(arm)
		gates.append({"centre": Vector3(gx, 0, front), "arm": arm})


## Barrel piles (some tipped over, some leaking), dead trees and rocks on the banks.
func _props() -> void:
	var piles := [Vector2(8.5, -6.0), Vector2(20.5, -6.0), Vector2(12.5, -21.0), Vector2(-21.0, -3.5), Vector2(-22.0, -30.0),
		Vector2(-30.0, 14.0), Vector2(30.0, 13.5), Vector2(-48.0, 15.0)]
	for pile: Vector2 in piles:
		var count := _rng.randi_range(3, 7)
		for k in count:
			var at := pile + Vector2(_rng.randf_range(-1.8, 1.8), _rng.randf_range(-1.4, 1.4))
			var piece: String = ["Barrel_hazard", "Barrel_toxic", "Barrel_rust"][_rng.randi() % 3]
			var y := YARD_Y if Layout.COMPOUND.has_point(at) else Layout.ground_y(at.x, at.y)
			if _rng.randf() < 0.25:  # tipped over
				_place(piece, Transform3D(Basis(Vector3.UP, _rng.randf() * TAU) * Basis(Vector3.RIGHT, PI / 2), Vector3(at.x, y + 0.45, at.y + 0.6)))
				_place("Spill", Transform3D(Basis.from_scale(Vector3(1.2, 1, 1.2)), Vector3(at.x, y + 0.01, at.y + 1.4)))
			else:
				_place(piece, Transform3D(Basis(Vector3.UP, _rng.randf() * TAU), Vector3(at.x, y, at.y)))
	for k in 34:  # dead trees on the far bank, the near bank and out the back
		var tx := _rng.randf_range(-Layout.MAP_HALF + 5, Layout.MAP_HALF - 5)
		var tz: float
		match k % 3:
			0: tz = Layout.river_z(tx) + Layout.RIVER_HALF + Layout.BANK + _rng.randf_range(1.0, 14.0)
			1: tz = Layout.river_near(tx) - Layout.BANK - _rng.randf_range(0.5, 1.5)
			_: tz = _rng.randf_range(-70.0, -38.0)
		if absf(tx - Layout.OUTFALL_X[0]) < 4.0 or absf(tx - Layout.OUTFALL_X[1]) < 4.0:
			continue
		var s := _rng.randf_range(0.8, 1.4)
		_place("DeadTree_a" if k % 2 else "DeadTree_b", Transform3D(Basis(Vector3.UP, _rng.randf() * TAU) * Basis.from_scale(Vector3.ONE * s), Vector3(tx, 0, tz)))
	for k in 30:  # rocks down the banks
		var rx := _rng.randf_range(-Layout.MAP_HALF, Layout.MAP_HALF)
		var side := -1.0 if k % 2 == 0 else 1.0
		var rz := Layout.river_z(rx) + side * (Layout.RIVER_HALF + _rng.randf_range(1.0, 2.5))
		_place("Rock", Transform3D(Basis(Vector3.UP, _rng.randf() * TAU) * Basis.from_scale(Vector3.ONE * _rng.randf_range(0.6, 1.4)), Vector3(rx, Layout.ground_y(rx, rz) - 0.2, rz)))


func _place(piece: String, xform: Transform3D) -> void:
	if not _placed.has(piece):
		_placed[piece] = []
	_placed[piece].append(xform)


func _add_mesh(node_name: String, builder: Mesh_.Builder) -> ArrayMesh:
	var mesh := builder.commit()
	var node := MeshInstance3D.new()
	node.name = node_name
	node.mesh = mesh
	add_child(node)
	return mesh


## One MultiMeshInstance3D per kit piece, holding every copy of it.
func _build_multimeshes() -> void:
	for piece: String in _placed:
		var list: Array = _placed[piece]
		var multi := MultiMesh.new()
		multi.transform_format = MultiMesh.TRANSFORM_3D
		multi.mesh = kit.mesh(piece)
		multi.instance_count = list.size()
		for i in list.size():
			multi.set_instance_transform(i, list[i])
		var node := MultiMeshInstance3D.new()
		node.name = piece
		node.multimesh = multi
		add_child(node)


func _add_shape(shape: Shape3D, xform: Transform3D) -> void:
	var node := CollisionShape3D.new()
	node.shape = shape
	node.transform = xform
	_body.add_child(node)
