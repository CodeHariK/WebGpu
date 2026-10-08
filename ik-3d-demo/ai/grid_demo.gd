## Tactical grid test arena: a spider finds its way to the orb over a layered grid — no navmesh.
## The arena has walls and crates to go round, a platform reached by stairs, a balcony over the
## floor (walk under it, or drop off its edge), a ladder up the platform's back wall and a gap to
## a second platform that only a jump crosses.
##   Click        put the orb there (the spider follows)
##   G  grid debug on/off      E  walk links on/off      R  rescan
##   C  camera: overview ↔ follow the spider              L  next spider layout
## Grid legend: squares = cells (blue low … yellow high), grey = walk links, orange arrows = drops,
## magenta arcs = ladders, cyan arcs = jumps, white = the spider's current path.
extends Node3D

const ARENA := 12.0 ## half size of the floor
const PLATFORM_TOP := 2.5
var overview := Transform3D(Basis.from_euler(Vector3(-0.95, 0.0, 0.0)), Vector3(0, 21, 15)) ## the overview camera

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var grid := TacticalGrid.new()
var spider: Spider
var walker: GridWalker
var orb: Node3D
var debug: TacticalGridDebug
var follow := false


func _ready() -> void:
	_build_arena()
	orb = _add_orb(Vector3(8, PLATFORM_TOP + 0.5, -7)) # on platform A: up the stairs or the ladder
	spider = Spider.new()
	spider.name = "Spider"
	spider.auto_walk = false
	spider.show_targets = false
	spider.position = Vector3(-8, 0, 8)
	add_child(spider)
	camera.transform = overview
	await get_tree().physics_frame # colliders exist in the physics world from now on
	_scan()
	walker = GridWalker.new()
	walker.name = "GridWalker"
	walker.spider = spider
	walker.grid = grid
	walker.target = orb
	add_child(walker)
	debug = TacticalGridDebug.new()
	debug.name = "GridDebug"
	debug.grid = grid
	debug.walker = walker
	add_child(debug)


func _process(delta: float) -> void:
	if follow:
		var behind := spider.global_position + Vector3(0, 5.5, 6.5)
		camera.global_position = camera.global_position.lerp(behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(spider.global_position)
	_update_hud()


func _scan() -> void:
	grid.origin = Vector3(-ARENA, -0.5, -ARENA)
	grid.size = Vector2i(roundi(ARENA * 2.0 / grid.cell_size), roundi(ARENA * 2.0 / grid.cell_size))
	grid.scan(get_world_3d().direct_space_state, get_tree().get_nodes_in_group(GridLink.GROUP))
	if debug != null:
		debug.refresh()


func _update_hud() -> void:
	var path_info := "-"
	if walker != null:
		path_info = "%s, %d waypoints" % [walker.status, walker.path.size()]
		if walker.waypoint < walker.path.size():
			path_info += ", next by %s" % TacticalGrid.Link.keys()[walker.path[walker.waypoint].kind].to_lower()
	hud.text = "grid: %d cells, %d links, scanned in %.0f ms (%d rays)   spider: %s   layout: %s\nclick = move the orb   G grid %s   E walk links   R rescan   C camera   L layout" % [
		grid.cell_count(), grid.link_count(), grid.scan_msec, grid.ray_count, path_info,
		spider.rig.layout.name if spider.rig != null else "-", "on" if debug != null and debug.visible else "off",
	]


func _unhandled_input(event: InputEvent) -> void:
	var click := event as InputEventMouseButton
	if click != null and click.pressed and click.button_index == MOUSE_BUTTON_LEFT:
		var from := camera.project_ray_origin(click.position)
		var to := from + camera.project_ray_normal(click.position) * 200.0
		var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, to))
		if not hit.is_empty():
			orb.global_position = hit.position + Vector3.UP * 0.5
		return
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_G: debug.visible = not debug.visible
		KEY_E:
			debug.show_walk_links = not debug.show_walk_links
			debug.refresh()
		KEY_R: _scan()
		KEY_C:
			follow = not follow
			if not follow:
				camera.transform = overview
		KEY_L: spider.layout_preset = (spider.layout_preset + 1) % SpiderLayout.PRESET_NAMES.size()


# --- the arena -----------------------------------------------------------------------------------

func _build_arena() -> void:
	var floor_color := Color(0.42, 0.5, 0.44)
	var wall_color := Color(0.55, 0.5, 0.45)
	var stone := Color(0.62, 0.58, 0.55)
	_box(Vector3(0, -0.5, 0), Vector3(ARENA * 2.0, 1.0, ARENA * 2.0), floor_color) # floor
	for side: float in [-1.0, 1.0]: # outer walls
		_box(Vector3(0, 1.0, side * (ARENA + 0.25)), Vector3(ARENA * 2.0 + 1.0, 2.0, 0.5), wall_color)
		_box(Vector3(side * (ARENA + 0.25), 1.0, 0), Vector3(0.5, 2.0, ARENA * 2.0), wall_color)
	_box(Vector3(-2.0, 1.0, 2.0), Vector3(8.0, 2.0, 0.4), wall_color) # a long wall to go round
	_box(Vector3(-3.0, 1.0, -0.5), Vector3(0.4, 2.0, 3.0), wall_color) # and a short one across it
	for spot: Vector3 in [Vector3(-6, 0.4, -3), Vector3(-5, 0.4, -4), Vector3(5, 0.4, 6), Vector3(-8, 0.4, 4)]:
		_box(spot, Vector3(0.8, 0.8, 0.8), Color(0.6, 0.45, 0.3)) # crates
	# Platform A (north-east) with stairs climbing to its west edge.
	var platform := Vector3(7.0, PLATFORM_TOP * 0.5, -6.0)
	_box(platform, Vector3(6.0, PLATFORM_TOP, 6.0), stone)
	for step in 10: # 0.25 up and 0.5 across each
		var rise := 0.25 * (step + 1)
		_box(Vector3(3.75 - (9 - step) * 0.5, rise * 0.5, -3.5), Vector3(0.5, rise, 2.0), stone.darkened(0.1))
	# Balcony off platform A's south side: walk under it, or drop off its edge.
	_box(Vector3(7.0, PLATFORM_TOP - 0.15, -1.5), Vector3(4.0, 0.3, 3.0), stone.lightened(0.1))
	# Platform B, north-west of A, across a 3 m gap.
	_box(Vector3(-1.0, PLATFORM_TOP * 0.5, -9.0), Vector3(4.0, PLATFORM_TOP, 4.0), stone)
	# Jump link: A's west edge (north part) ↔ B's east edge, over the gap.
	var jump_a := _link(Vector3(4.4, PLATFORM_TOP, -8.2), GridLink.Kind.JUMP)
	var jump_b := _link(Vector3(0.6, PLATFORM_TOP, -8.2), GridLink.Kind.JUMP)
	jump_a.other = jump_b
	# Ladder up the back (east) wall of platform A.
	var ladder_foot := _link(Vector3(10.6, 0.0, -6.0), GridLink.Kind.LADDER)
	var ladder_top := _link(Vector3(9.4, PLATFORM_TOP, -6.0), GridLink.Kind.LADDER)
	ladder_foot.other = ladder_top
	_box(Vector3(10.05, PLATFORM_TOP * 0.5, -6.0), Vector3(0.1, PLATFORM_TOP, 0.8), Color(0.5, 0.3, 0.15)) # the ladder


func _box(at: Vector3, size: Vector3, color: Color) -> void:
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
	add_child(body)


func _link(at: Vector3, kind: GridLink.Kind) -> GridLink:
	var link := GridLink.new()
	link.kind = kind
	link.position = at
	add_child(link)
	return link


func _add_orb(at: Vector3) -> Node3D:
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
	add_child(ball)
	return ball
