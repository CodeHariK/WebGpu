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

const PLATFORM_TOP := TestArena.PLATFORM_TOP
const COVER_REPICK := 0.5 ## seconds between choosing a cover spot
const SHOUT_RADIUS := 10.0
var overview := Transform3D(Basis.from_euler(Vector3(-0.95, 0.0, 0.0)), Vector3(0, 21, 15)) ## the overview camera

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var grid := TacticalGrid.new()
var spider: Spider
var walker: GridWalker
var orb: Node3D
var player := TestPlayer.new() ## walks the orb
var debug: TacticalGridDebug
var visibility: PlayerVisibility
var brain: HunterBrain
var senses_debug: SensesDebug
var follow := false
var mode := Mode.HUNT
var cover_goal: Marker3D ## where the spider is heading when taking cover
var _cover_left := 0.0
var _time := 0.0


func _ready() -> void:
	TestArena.build(self)
	orb = TestArena.add_orb(self, Vector3(8, PLATFORM_TOP + 0.5, -7)) # on platform A: up the stairs or the ladder
	player.name = "Player"
	player.orb = orb
	add_child(player)
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
	if visibility != null:
		visibility.update(_time, player.eye(), player.facing)
		_update_mode(delta)
	if follow:
		var behind := spider.global_position + Vector3(0, 5.5, 6.5)
		camera.global_position = camera.global_position.lerp(behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(spider.global_position)
	_update_hud()


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
	TestArena.scan_grid(grid, self)
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
	if player.handle_click(event, camera):
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
