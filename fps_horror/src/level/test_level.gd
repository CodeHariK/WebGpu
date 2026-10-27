## Step 2 test level: a hall to try every kind of movement, opening into a stretch of mine.
##
##   hall (30 x 30 m, x -15..15, z -15..15)
##     stairs   8 steps of 0.25 m going west up to a 2 m platform (step-up test)
##     ramp     from the platform's south edge down to the floor (14°)
##     tunnel   1.25 m high, 12 m long: only fits crouched (crouch + can't-stand-up test)
##     crate    0.7 m high, 2.5 m long, to jump onto; a crate pile in a corner for scale
##   mine     through a gap in the south wall: their corridor wall meshes either side, gravel
##            floor, timber frames with bulbs, a rail cart
##
## Collision is plain boxes (cheap); the corridor's bumpy wall meshes are visual only and get a
## flat box behind their faces. All lights are unshadowed OmniLights (the flashlight in step 5
## will be the only shadowed light).
extends Node3D

const Env := preload("res://src/look/env_look.gd")

const HALL := 15.0
const WALL_HEIGHT := 5.5
const CORRIDOR_HALF := 2.2  # half the walkable corridor width
const CORRIDOR_LENGTH := 45.0
const SPAWN := Vector3(0, 0.05, 10)


func _ready() -> void:
	_hall()
	_stairs_platform_ramp()
	_tunnel()
	_crates()
	_corridor()


func _hall() -> void:
	_box(Vector3(0, -0.5, 0), Vector3(HALL * 2, 1, HALL * 2), Env.gravel())  # floor
	_box(Vector3(0, WALL_HEIGHT + 0.25, 0), Vector3(HALL * 2, 0.5, HALL * 2), Env.rock())  # ceiling
	_box(Vector3(0, WALL_HEIGHT / 2, -HALL - 0.5), Vector3(HALL * 2 + 2, WALL_HEIGHT, 1), Env.rock())  # north
	_box(Vector3(-HALL - 0.5, WALL_HEIGHT / 2, 0), Vector3(1, WALL_HEIGHT, HALL * 2), Env.rock())  # west
	_box(Vector3(HALL + 0.5, WALL_HEIGHT / 2, 0), Vector3(1, WALL_HEIGHT, HALL * 2), Env.rock())  # east
	var side := (HALL - CORRIDOR_HALF) / 2 + CORRIDOR_HALF  # south wall, with the corridor gap
	for x in [-side, side]:
		_box(Vector3(x, WALL_HEIGHT / 2, HALL + 0.5), Vector3(HALL - CORRIDOR_HALF + 1, WALL_HEIGHT, 1), Env.rock())
	_box(Vector3(0, 4.4, HALL + 0.5), Vector3(CORRIDOR_HALF * 2, 2.2, 1), Env.rock())  # over the gap
	for at in [Vector3(-8, 4.6, 8), Vector3(8, 4.6, 8), Vector3(-8, 4.6, -8), Vector3(8, 4.6, -8), Vector3(0, 4.6, 0)]:
		_lamp(at, 13.0)


## Stairs (east side) up to a platform, a ramp (south side) back down.
func _stairs_platform_ramp() -> void:
	_box(Vector3(-10, 1, -8), Vector3(8, 2, 8), Env.rock())  # platform x -14..-6, z -12..-4, top 2 m
	for k in range(1, 9):  # step k: 0.6 m deep, top at 0.25 k, from x = -1.2 westward
		var top := 0.25 * k
		var x := -1.2 - 0.6 * k + 0.3
		_box(Vector3(x, top / 2, -8), Vector3(0.6, top, 4), Env.planks())
	var angle := atan2(2.0, 8.0)
	var length := sqrt(2.0 * 2.0 + 8.0 * 8.0)
	_box(Vector3(-10, 1.0 - 0.2 * cos(angle), 0), Vector3(4, 0.4, length), Env.planks(), Vector3(angle, 0, 0))


## A long low tunnel along -Z at x 6..10: two side walls and a roof at 1.25 m.
func _tunnel() -> void:
	_box(Vector3(5.8, 1.5, -6), Vector3(0.4, 3, 12), Env.rock())
	_box(Vector3(10.2, 1.5, -6), Vector3(0.4, 3, 12), Env.rock())
	_box(Vector3(8, 2.125, -6), Vector3(4.8, 1.75, 12), Env.rock())  # roof from 1.25 m up to 3 m


func _crates() -> void:
	_box(Vector3(0, 0.35, -2.75), Vector3(1.2, 0.7, 2.5), Env.planks())  # the one to jump onto
	for c in [[Vector3(12, 0.5, 11), 1.0], [Vector3(13.1, 0.5, 11.3), 1.0], [Vector3(12.4, 1.5, 11.1), 1.0], [Vector3(13.3, 0.3, 9.8), 0.6]]:
		_box(c[0], Vector3.ONE * c[1], Env.planks(), Vector3(0, c[0].x * 0.3, 0))


## The mine corridor: their bumpy wall meshes on both sides, gravel floor, rock ceiling, timber
## frames with bulbs every 9 m, a rail cart.
func _corridor() -> void:
	var z0 := HALL + 1.0
	var z1 := z0 + CORRIDOR_LENGTH
	var mid := (z0 + z1) / 2
	_box(Vector3(0, -0.5, mid), Vector3(8, 1, CORRIDOR_LENGTH), Env.gravel())
	_box(Vector3(0, WALL_HEIGHT + 0.25, mid), Vector3(8, 0.5, CORRIDOR_LENGTH), Env.rock())
	_box(Vector3(0, WALL_HEIGHT / 2, z1 + 0.5), Vector3(8, WALL_HEIGHT, 1), Env.rock())  # dead end
	for side in [-1, 1]:  # flat collision just behind the wall meshes' faces
		_collider(Vector3(side * (CORRIDOR_HALF + 0.25), WALL_HEIGHT / 2, mid), Vector3(0.5, WALL_HEIGHT, CORRIDOR_LENGTH))
		_wall_mesh(side, z0)
	var z := z0 + 4.0
	while z < z1 - 2.0:
		_frame(z)
		_lamp(Vector3(0, 4.3, z), 9.0)
		z += 9.0
	_prop("res://assets/env/RailCart.gltf", Vector3(1.0, 0, z0 + 14.0), Env.rust(), 0.0, Vector3(1.0, 1.2, 2.2))


## One of their Corridor_Wall meshes (46 m long, rough rock face) along a corridor side,
## turned so its face points into the corridor and its front lines up with z0.
func _wall_mesh(side: int, z0: float) -> void:
	var mesh: Node3D = load("res://assets/env/MinesCorridor/Corridor_Wall.gltf").instantiate()
	add_child(mesh)
	mesh.rotation.y = 0.0 if side < 0 else PI
	var box := _bounds(mesh)
	var inner := box.end.x if side < 0 else box.position.x  # the face towards the corridor
	mesh.position = Vector3(side * CORRIDOR_HALF - inner, -0.1 - box.position.y, z0 - box.position.z)
	for m: MeshInstance3D in mesh.find_children("*", "MeshInstance3D", true, false):
		m.material_override = Env.rock()


## A mine support: two posts and a beam across the corridor.
func _frame(z: float) -> void:
	for x in [-CORRIDOR_HALF + 0.15, CORRIDOR_HALF - 0.15]:
		_box(Vector3(x, 2.2, z), Vector3(0.3, 4.4, 0.3), Env.planks())
	_box(Vector3(0, 4.55, z), Vector3(CORRIDOR_HALF * 2, 0.3, 0.35), Env.planks())


## A hanging bulb with an unshadowed warm light.
func _lamp(at: Vector3, light_range: float) -> void:
	var bulb := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = 0.08
	sphere.height = 0.16
	sphere.radial_segments = 8
	sphere.rings = 4
	bulb.mesh = sphere
	bulb.material_override = Env.bulb()
	bulb.position = at
	add_child(bulb)
	var light := OmniLight3D.new()
	light.position = at
	light.omni_range = light_range
	light.light_energy = 1.6
	light.light_color = Color(1.0, 0.78, 0.5)
	light.omni_attenuation = 1.2
	add_child(light)


## A glTF prop with one material and a simple box collider of `size` (centred, standing on the floor).
func _prop(path: String, at: Vector3, mat: Material, yaw: float, size: Vector3) -> void:
	var node: Node3D = load(path).instantiate()
	add_child(node)
	node.rotation.y = yaw
	var box := _bounds(node)
	node.position = at - Vector3(box.get_center().x, box.position.y, box.get_center().z)
	for m: MeshInstance3D in node.find_children("*", "MeshInstance3D", true, false):
		m.material_override = mat
	_collider(at + Vector3(0, size.y / 2, 0), size, Vector3(0, yaw, 0))


## A visible box with a matching collider.
func _box(at: Vector3, size: Vector3, mat: Material, rot := Vector3.ZERO) -> void:
	var mesh := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	mesh.mesh = box
	mesh.material_override = mat
	mesh.position = at
	mesh.rotation = rot
	add_child(mesh)
	_collider(at, size, rot)


func _collider(at: Vector3, size: Vector3, rot := Vector3.ZERO) -> void:
	var body := StaticBody3D.new()
	body.position = at
	body.rotation = rot
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	body.add_child(shape)
	add_child(body)


## The meshes' bounds around the root's position (its rotation included).
func _bounds(root: Node3D) -> AABB:
	var out := AABB()
	var first := true
	for m: MeshInstance3D in root.find_children("*", "MeshInstance3D", true, false):
		var b := m.global_transform * m.get_aabb()
		b.position -= root.global_position
		out = b if first else out.merge(b)
		first = false
	return out
