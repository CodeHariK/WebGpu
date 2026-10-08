## The AI test arena, shared by the grid and squad demos: a floor with outer walls, a long wall and
## a short one across it, crates, platform A (north-east) with stairs up its west side, a balcony
## off its south side (walk under, or drop off), a ladder up its east wall, and platform B across
## a 3 m gap that only a jump crosses (GridLink markers for the ladder and the jump).
class_name TestArena
extends RefCounted

const ARENA := 12.0 ## half size of the floor
const PLATFORM_TOP := 2.5


## Fit `grid` to the arena floor and scan it (after the colliders are in the physics world).
static func scan_grid(grid: TacticalGrid, parent: Node3D) -> void:
	grid.origin = Vector3(-ARENA, -0.5, -ARENA)
	grid.size = Vector2i(roundi(ARENA * 2.0 / grid.cell_size), roundi(ARENA * 2.0 / grid.cell_size))
	grid.scan(parent.get_world_3d().direct_space_state, parent.get_tree().get_nodes_in_group(GridLink.GROUP))


static func build(parent: Node3D) -> void:
	var floor_color := Color(0.42, 0.5, 0.44)
	var wall_color := Color(0.55, 0.5, 0.45)
	var stone := Color(0.62, 0.58, 0.55)
	_box(parent, Vector3(0, -0.5, 0), Vector3(ARENA * 2.0, 1.0, ARENA * 2.0), floor_color) # floor
	for side: float in [-1.0, 1.0]: # outer walls
		_box(parent, Vector3(0, 1.0, side * (ARENA + 0.25)), Vector3(ARENA * 2.0 + 1.0, 2.0, 0.5), wall_color)
		_box(parent, Vector3(side * (ARENA + 0.25), 1.0, 0), Vector3(0.5, 2.0, ARENA * 2.0), wall_color)
	_box(parent, Vector3(-2.0, 1.0, 2.0), Vector3(8.0, 2.0, 0.4), wall_color) # a long wall to go round
	_box(parent, Vector3(-3.0, 1.0, -0.5), Vector3(0.4, 2.0, 3.0), wall_color) # and a short one across it
	for spot: Vector3 in [Vector3(-6, 0.4, -3), Vector3(-5, 0.4, -4), Vector3(5, 0.4, 6), Vector3(-8, 0.4, 4)]:
		_box(parent, spot, Vector3(0.8, 0.8, 0.8), Color(0.6, 0.45, 0.3)) # crates
	# Platform A (north-east) with stairs climbing to its west edge.
	var platform := Vector3(7.0, PLATFORM_TOP * 0.5, -6.0)
	_box(parent, platform, Vector3(6.0, PLATFORM_TOP, 6.0), stone)
	for step in 10: # 0.25 up and 0.5 across each
		var rise := 0.25 * (step + 1)
		_box(parent, Vector3(3.75 - (9 - step) * 0.5, rise * 0.5, -3.5), Vector3(0.5, rise, 2.0), stone.darkened(0.1))
	# Balcony off platform A's south side: walk under it, or drop off its edge.
	_box(parent, Vector3(7.0, PLATFORM_TOP - 0.15, -1.5), Vector3(4.0, 0.3, 3.0), stone.lightened(0.1))
	# Platform B, north-west of A, across a 3 m gap.
	_box(parent, Vector3(-1.0, PLATFORM_TOP * 0.5, -9.0), Vector3(4.0, PLATFORM_TOP, 4.0), stone)
	# Jump link: A's west edge (north part) ↔ B's east edge, over the gap.
	var jump_a := _link(parent, Vector3(4.4, PLATFORM_TOP, -8.2), GridLink.Kind.JUMP)
	var jump_b := _link(parent, Vector3(0.6, PLATFORM_TOP, -8.2), GridLink.Kind.JUMP)
	jump_a.other = jump_b
	# Ladder up the back (east) wall of platform A.
	var ladder_foot := _link(parent, Vector3(10.6, 0.0, -6.0), GridLink.Kind.LADDER)
	var ladder_top := _link(parent, Vector3(9.4, PLATFORM_TOP, -6.0), GridLink.Kind.LADDER)
	ladder_foot.other = ladder_top
	_box(parent, Vector3(10.05, PLATFORM_TOP * 0.5, -6.0), Vector3(0.1, PLATFORM_TOP, 0.8), Color(0.5, 0.3, 0.15)) # the ladder


static func _box(parent: Node3D, at: Vector3, size: Vector3, color: Color) -> void:
	var body := StaticBody3D.new()
	body.position = at
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


static func _link(parent: Node3D, at: Vector3, kind: GridLink.Kind) -> GridLink:
	var link := GridLink.new()
	link.kind = kind
	link.position = at
	parent.add_child(link)
	return link


static func add_orb(parent: Node3D, at: Vector3) -> Node3D:
	var sphere := SphereMesh.new()
	sphere.radius = 0.2
	sphere.height = 0.4
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.6, 1.0, 0.7)
	material.emission_enabled = true
	material.emission = Color(0.4, 1.0, 0.6)
	material.emission_energy_multiplier = 2.0
	var ball := MeshInstance3D.new()
	ball.mesh = sphere
	ball.material_override = material
	ball.position = at
	parent.add_child(ball)
	return ball
