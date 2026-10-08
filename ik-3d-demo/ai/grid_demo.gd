## Tactical grid test arena: a spider finds its way over a layered grid — no navmesh. The orb is
## the player: the grid knows what it can see (PlayerVisibility), so the spider can chase it or
## take cover from it. In hunt mode the spider has its own senses (HunterBrain + CreatureSenses):
## it only knows what it has seen or heard.
## The arena has walls and crates to go round, a platform reached by stairs, a balcony over the
## floor (walk under it, or drop off its edge), a ladder up the platform's back wall and a gap to
## a second platform that only a jump crosses.
##   Click        walk the orb (player) there (footsteps carry 3 m)   Shift-click  put it there
##   Right-click  turn the player to look there      N  make a loud noise (10 m)
##   F  spider: hunt (own senses) → chase the player → take cover from it (nearest cover, peek preferred)
##   V  cells coloured by what the player sees ↔ by height
##   G  grid debug on/off      E  walk links on/off      R  rescan
##   C  camera: overview ↔ follow the spider              L  next spider layout
## Grid legend: grey = walk links, orange arrows = drops, magenta arcs = ladders, cyan arcs = jumps,
## white = the spider's current path. Cells: red = in the player's view, orange = in line of sight
## but outside the view, dark green = hidden, bright green (inner square) = cover, cyan = peek,
## grey = not checked yet (see tactical_grid_debug.gd). Hunt mode: see senses_debug.gd.
extends Node3D

enum Mode { HUNT, CHASE, COVER }

const ARENA := 12.0 ## half size of the floor
const PLATFORM_TOP := 2.5
const EYE_HEIGHT := 1.1 ## the player's eye above the orb
const COVER_REPICK := 0.5 ## seconds between choosing a cover spot
const PLAYER_SPEED := 2.2 ## m/s the orb walks
const FOOTSTEP_EVERY := 0.45 ## seconds
const FOOTSTEP_RADIUS := 3.0
const SHOUT_RADIUS := 10.0
var overview := Transform3D(Basis.from_euler(Vector3(-0.95, 0.0, 0.0)), Vector3(0, 21, 15)) ## the overview camera

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var grid := TacticalGrid.new()
var spider: Spider
var walker: GridWalker
var orb: Node3D
var debug: TacticalGridDebug
var visibility: PlayerVisibility
var brain: HunterBrain
var senses_debug: SensesDebug
var follow := false
var mode := Mode.HUNT
var player_facing := Vector3(-1, 0, 1).normalized()
var player_goal := Vector3.INF ## where the orb is walking to
var cover_goal: Marker3D ## where the spider is heading when taking cover
var _cover_left := 0.0
var _footstep_left := 0.0
var _time := 0.0


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
	visibility = PlayerVisibility.new()
	visibility.grid = grid
	cover_goal = Marker3D.new()
	cover_goal.name = "CoverGoal"
	add_child(cover_goal)
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
	debug.visibility = visibility
	add_child(debug)
	brain = HunterBrain.new()
	brain.name = "HunterBrain"
	brain.spider = spider
	brain.walker = walker
	brain.grid = grid
	brain.player = orb
	add_child(brain)
	senses_debug = SensesDebug.new()
	senses_debug.name = "SensesDebug"
	senses_debug.brain = brain
	add_child(senses_debug)


func _process(delta: float) -> void:
	_time += delta
	_walk_player(delta)
	if visibility != null:
		visibility.update(_time, orb.global_position + Vector3.UP * EYE_HEIGHT, player_facing)
		_update_mode(delta)
	if follow:
		var behind := spider.global_position + Vector3(0, 5.5, 6.5)
		camera.global_position = camera.global_position.lerp(behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(spider.global_position)
	_update_hud()


# The orb walks toward player_goal: stopped by walls, it steps up stairs and falls off edges (it
# can't climb a platform — shift-click puts it there). Walking makes footstep noises.
func _walk_player(delta: float) -> void:
	if player_goal == Vector3.INF:
		return
	var flat := player_goal - orb.global_position
	flat.y = 0.0
	if flat.length() < 0.05:
		player_goal = Vector3.INF
		return
	var space := get_world_3d().direct_space_state
	var step := flat.normalized() * minf(PLAYER_SPEED * delta, flat.length())
	var next := orb.global_position + step
	if not space.intersect_ray(PhysicsRayQueryParameters3D.create(orb.global_position, next + step.normalized() * 0.2)).is_empty():
		player_goal = Vector3.INF # walked into a wall
		return
	var ground := space.intersect_ray(PhysicsRayQueryParameters3D.create(next + Vector3.UP * 0.6, next + Vector3.DOWN * 4.0))
	if not ground.is_empty():
		next.y = ground.position.y + 0.5
	orb.global_position = next
	player_facing = flat.normalized()
	_footstep_left -= delta
	if _footstep_left <= 0.0:
		_footstep_left = FOOTSTEP_EVERY
		NoiseBus.emit(orb.global_position + Vector3.DOWN * 0.4, FOOTSTEP_RADIUS)


func _update_mode(delta: float) -> void:
	var hunting := mode == Mode.HUNT
	if brain.enabled != hunting:
		brain.enabled = hunting
	if hunting:
		return
	var take_cover := mode == Mode.COVER
	walker.target = cover_goal if take_cover else orb
	walker.arrive_distance = 0.3 if take_cover else 0.9 # cover means standing right on the spot
	if not take_cover:
		return
	_cover_left -= delta
	if _cover_left > 0.0:
		return
	_cover_left = COVER_REPICK
	var cell := CoverPicker.nearest_cover(grid, visibility, spider.global_position)
	if cell >= 0:
		cover_goal.global_position = grid.positions[cell]


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
	var sight_info := "-"
	if visibility != null:
		sight_info = "%d seen, %d hidden, %d cover, sweep %.0f ms" % [
			visibility.count(PlayerVisibility.Sight.SEEN), visibility.count(PlayerVisibility.Sight.HIDDEN),
			visibility.cover_count(), visibility.sweep_msec]
	var senses_info := ""
	if brain != null and brain.enabled:
		var senses := brain.senses
		senses_info = "\nsenses: %s, suspicion %.2f, sees %.0f%% of you, %s" % [
			CreatureSenses.Awareness.keys()[senses.awareness].to_lower(), senses.suspicion, senses.visible_fraction * 100.0, brain.state]
	hud.text = "grid: %d cells, %d links, scanned in %.0f ms   player sees: %s\nspider (%s): %s   layout: %s%s\nclick = walk the player   shift-click = put it there   right-click = look there   N noise   F hunt/chase/cover   V sight colours   G grid %s   E walk links   R rescan   C camera   L layout" % [
		grid.cell_count(), grid.link_count(), grid.scan_msec, sight_info, Mode.keys()[mode].to_lower(), path_info,
		spider.rig.layout.name if spider.rig != null else "-", senses_info, "on" if debug != null and debug.visible else "off",
	]


func _unhandled_input(event: InputEvent) -> void:
	var click := event as InputEventMouseButton
	if click != null and click.pressed and click.button_index == MOUSE_BUTTON_LEFT:
		var from := camera.project_ray_origin(click.position)
		var to := from + camera.project_ray_normal(click.position) * 200.0
		var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, to))
		if not hit.is_empty():
			if click.shift_pressed: # teleport (onto a platform, say)
				var flat: Vector3 = hit.position - orb.global_position
				flat.y = 0.0
				if flat.length() > 0.1:
					player_facing = flat.normalized() # look the way you went
				orb.global_position = hit.position + Vector3.UP * 0.5
				player_goal = Vector3.INF
			else:
				player_goal = hit.position
		return
	if click != null and click.pressed and click.button_index == MOUSE_BUTTON_RIGHT:
		var from := camera.project_ray_origin(click.position)
		var to := from + camera.project_ray_normal(click.position) * 200.0
		var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, to))
		if not hit.is_empty():
			var flat: Vector3 = hit.position - orb.global_position
			flat.y = 0.0
			if flat.length() > 0.1:
				player_facing = flat.normalized()
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
		KEY_F: mode = ((mode + 1) % Mode.size()) as Mode
		KEY_N: NoiseBus.emit(orb.global_position, SHOUT_RADIUS)
		KEY_V:
			debug.show_visibility = not debug.show_visibility
			debug.refresh()
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
