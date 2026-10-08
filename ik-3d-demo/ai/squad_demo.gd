## Squad test: four spiders close in on the player by role (Squad + PositionScorer) in the AI test
## arena. Pressure stays in front and in sight; the flankers and the one going behind take routes
## the player can't see, wait on their spot, and pounce when the player isn't looking their way.
##   Click        walk the player there       Shift-click  put it there
##   Right-click  turn the player to look there (turn your back on a flanker and it comes)
##   Tab  next member (its heatmap + path)    Q  change that member's role
##   H  heatmap on/off      G  grid on/off    V  grid colours: what the player sees ↔ height
##   P  squad on/off (spiders stop)           C  camera: overview ↔ follow the selected member
## See squad_debug.gd for the colours.
extends Node3D

const SPAWNS: Array[Vector3] = [Vector3(-8, 0, 8), Vector3(9, 0, 9), Vector3(-9, 0, -6), Vector3(9, 0, 2)]
const ROLES: Array[PositionScorer.Role] = [PositionScorer.Role.PRESSURE, PositionScorer.Role.FLANK_LEFT, PositionScorer.Role.FLANK_RIGHT, PositionScorer.Role.BEHIND]
var overview := Transform3D(Basis.from_euler(Vector3(-0.95, 0.0, 0.0)), Vector3(0, 21, 15)) ## the overview camera

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var grid := TacticalGrid.new()
var visibility: PlayerVisibility
var player := TestPlayer.new()
var squad: Squad
var debug: TacticalGridDebug
var squad_debug: SquadDebug
var follow := false
var _time := 0.0


func _ready() -> void:
	TestArena.build(self)
	player.name = "Player"
	player.orb = TestArena.add_orb(self, Vector3(1, 0.5, 6))
	player.facing = Vector3(-1, 0, 0.3).normalized()
	add_child(player)
	camera.transform = overview
	await get_tree().physics_frame # colliders exist in the physics world from now on
	TestArena.scan_grid(grid, self)
	visibility = PlayerVisibility.new()
	visibility.grid = grid
	squad = Squad.new()
	squad.name = "Squad"
	squad.grid = grid
	squad.visibility = visibility
	squad.player = player
	add_child(squad)
	for i in SPAWNS.size():
		_add_member(i)
	debug = TacticalGridDebug.new()
	debug.name = "GridDebug"
	debug.grid = grid
	debug.visibility = visibility
	debug.show_walk_links = false
	debug.visible = false
	add_child(debug)
	squad_debug = SquadDebug.new()
	squad_debug.name = "SquadDebug"
	squad_debug.squad = squad
	add_child(squad_debug)
	_select(0)


func _add_member(index: int) -> void:
	var spider := Spider.new()
	spider.name = "Spider%d" % index
	spider.auto_walk = false
	spider.show_targets = false
	spider.position = SPAWNS[index]
	add_child(spider)
	var walker := GridWalker.new()
	walker.name = "Walker%d" % index
	walker.spider = spider
	walker.grid = grid
	add_child(walker)
	squad.add_member(spider, walker, ROLES[index])


func _process(delta: float) -> void:
	_time += delta
	if visibility == null:
		return
	visibility.update(_time, player.eye(), player.facing)
	if follow:
		var spider := _selected().spider
		var behind := spider.global_position + Vector3(0, 5.5, 6.5)
		camera.global_position = camera.global_position.lerp(behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(spider.global_position)
	_update_hud()


func _selected() -> Squad.Member:
	return squad.members[squad_debug.selected % squad.members.size()]


func _select(index: int) -> void:
	squad_debug.selected = index % squad.members.size()
	debug.walker = _selected().walker


func _update_hud() -> void:
	var lines: Array[String] = []
	for i in squad.members.size():
		var member := squad.members[i]
		var score: float = member.scores.get(member.cell, 0.0)
		lines.append("%s%d %-11s %-10s score %5.1f  %s" % [">" if i == squad_debug.selected else " ", i,
			PositionScorer.Role.keys()[member.role].to_lower(), member.state, score, member.walker.status])
	hud.text = "squad %s   grid: %d cells   player sees %d cells   sweep %.0f ms\n%s\nclick = walk   shift-click = put   right-click = look   Tab member   Q role   H heatmap   G grid   V colours   P squad on/off   C camera" % [
		"on" if squad.enabled else "off", grid.cell_count(), visibility.count(PlayerVisibility.Sight.SEEN), visibility.sweep_msec, "\n".join(lines)]


func _unhandled_input(event: InputEvent) -> void:
	if player.handle_click(event, camera):
		return
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo or squad == null:
		return
	match key.keycode:
		KEY_TAB: _select(squad_debug.selected + 1)
		KEY_Q:
			var member := _selected()
			member.role = ((member.role + 1) % PositionScorer.Role.size()) as PositionScorer.Role
			member.cell = -1
		KEY_H: squad_debug.show_heatmap = not squad_debug.show_heatmap
		KEY_G: debug.visible = not debug.visible
		KEY_V:
			debug.show_visibility = not debug.show_visibility
			debug.refresh()
		KEY_P: squad.enabled = not squad.enabled
		KEY_C:
			follow = not follow
			if not follow:
				camera.transform = overview
