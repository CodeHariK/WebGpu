## The road test arena (car player): a big flat field with a looping road, low walls and rocks
## along both sides of it (cover to ambush from) and a few crates in the field.
## build() returns the road as a world-space Curve3D.
class_name TestRoadArena
extends RefCounted

const ARENA := 22.0 ## half size of the field
const ROAD_WIDTH := 4.0
const LOOP: Array[Vector3] = [
	Vector3(-16, 0, -14), Vector3(0, 0, -17), Vector3(16, 0, -12), Vector3(17, 0, 4),
	Vector3(8, 0, 16), Vector3(-8, 0, 14), Vector3(-17, 0, 6), Vector3(-14, 0, -6)]
const COVER_COUNT := 16 ## cover pieces along the road, sides alternating
const COVER_OFF_ROAD := 4.2 ## metres from the road's centre line


static func build(parent: Node3D) -> Curve3D:
	_box(parent, Transform3D(Basis(), Vector3(0, -0.5, 0)), Vector3(ARENA * 2.0, 1.0, ARENA * 2.0), Color(0.42, 0.52, 0.4))
	var road := _loop_curve()
	parent.add_child(_road_mesh(road))
	var length := road.get_baked_length()
	for i in COVER_COUNT:
		var at := length * (i + 0.5) / COVER_COUNT
		var point := road.sample_baked(at)
		var tangent := (road.sample_baked(wrapf(at + 0.5, 0.0, length)) - point).normalized()
		var side := tangent.cross(Vector3.UP) * (1.0 if i % 2 == 0 else -1.0)
		var basis := Basis.looking_at(tangent, Vector3.UP)
		if i % 3 == 2: # a rock
			_box(parent, Transform3D(basis, point + side * COVER_OFF_ROAD + Vector3.UP * 1.1), Vector3(1.4, 2.2, 1.4), Color(0.55, 0.53, 0.5))
		else: # a low wall along the road
			_box(parent, Transform3D(basis, point + side * COVER_OFF_ROAD + Vector3.UP * 0.7), Vector3(0.5, 1.4, 2.6), Color(0.6, 0.5, 0.42))
	for spot: Vector3 in [Vector3(-4, 0.4, -4), Vector3(5, 0.4, 3), Vector3(0, 0.4, 7), Vector3(-7, 0.4, 2)]:
		_box(parent, Transform3D(Basis(), spot), Vector3(0.8, 0.8, 0.8), Color(0.6, 0.45, 0.3))
	return road


## Fit `grid` to the field and scan it (after the colliders are in the physics world).
static func scan_grid(grid: TacticalGrid, parent: Node3D) -> void:
	grid.origin = Vector3(-ARENA, -0.5, -ARENA)
	grid.size = Vector2i(roundi(ARENA * 2.0 / grid.cell_size), roundi(ARENA * 2.0 / grid.cell_size))
	grid.scan(parent.get_world_3d().direct_space_state, [])


# A closed smooth curve through LOOP (handles from the neighbours, Catmull-Rom style).
static func _loop_curve() -> Curve3D:
	var curve := Curve3D.new()
	curve.bake_interval = 0.5
	var count := LOOP.size()
	for i in count + 1:
		var point := LOOP[i % count]
		var handle := (LOOP[(i + 1) % count] - LOOP[(i - 1 + count) % count]) * 0.2
		curve.add_point(point, -handle, handle)
	return curve


# A flat dark strip along the road, just above the ground (no collider: the field is the ground).
static func _road_mesh(road: Curve3D) -> MeshInstance3D:
	var vertices := PackedVector3Array()
	var length := road.get_baked_length()
	var steps := ceili(length)
	for k in steps + 1:
		var at := length * k / steps
		var point := road.sample_baked(wrapf(at, 0.0, length))
		var tangent := (road.sample_baked(wrapf(at + 0.5, 0.0, length)) - point).normalized()
		var side := tangent.cross(Vector3.UP) * ROAD_WIDTH * 0.5
		vertices.append(point + side + Vector3.UP * 0.02)
		vertices.append(point - side + Vector3.UP * 0.02)
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLE_STRIP, arrays)
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.25, 0.25, 0.28)
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var instance := MeshInstance3D.new()
	instance.name = "Road"
	instance.mesh = mesh
	instance.material_override = material
	return instance


static func _box(parent: Node3D, place: Transform3D, size: Vector3, color: Color) -> void:
	var body := StaticBody3D.new()
	body.transform = place
	var shape := BoxShape3D.new()
	shape.size = size
	var collision := CollisionShape3D.new()
	collision.shape = shape
	body.add_child(collision)
	var mesh := BoxMesh.new()
	mesh.size = size
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	body.add_child(visual)
	parent.add_child(body)


## The car's body (no collider: the spiders' rays shouldn't hit it). Its centre sits on the road.
static func add_car(parent: Node3D) -> Node3D:
	var root := Node3D.new()
	root.name = "Car"
	var body := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(1.8, 0.7, 3.4)
	body.mesh = box
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.95, 0.35, 0.3)
	body.material_override = material
	root.add_child(body)
	var cab := MeshInstance3D.new()
	var cab_box := BoxMesh.new()
	cab_box.size = Vector3(1.5, 0.55, 1.6)
	cab.mesh = cab_box
	cab.position = Vector3(0, 0.6, 0.2)
	cab.material_override = material
	root.add_child(cab)
	parent.add_child(root)
	return root
