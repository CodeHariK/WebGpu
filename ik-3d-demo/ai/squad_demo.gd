## Squad test: four spiders that share what they see and close in on the player by role (Squad,
## SquadKnowledge, PositionScorer, AttackTokens) in the AI test arena. They start unaware and
## wander; one spots you → "there!" → the squad goes alert. Pressure stays in front and in sight;
## the flankers and the one going behind take routes the player can't see, wait on their spot, and
## attack when they get a token — first those outside the player's view. Lose them and they search.
##   Click        walk the player there (footsteps)   Shift-click  put it there
##   Right-click  turn the player to look there (turn your back on a flanker and it comes)
##   N  make a loud noise      T  attack tokens 1 → 2 → 3
##   J  plan: auto → pincer → surround (auto: surround with 4+ members when you stand in the open)
##   Tab  next member (its heatmap + path)    Q  change that member's role (until the next re-plan)
##   X  kill that member (the squad re-plans)
##   H  heatmap on/off      K  vision cones on/off      G  grid on/off
##   V  grid colours: what the player sees ↔ height    C  camera: overview ↔ follow the selected member
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
var senses_debug: Array[SensesDebug] = []
var follow := false
var _time := 0.0


func _ready() -> void:
	TestArena.build(self)
	player.name = "Player"
	player.orb = TestArena.add_orb(self, Vector3(-1, TestArena.PLATFORM_TOP + 0.5, -9)) # on platform B, out of the way
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
	var member := squad.add_member(spider, walker, ROLES[index])
	var cones := SensesDebug.new()
	cones.name = "Senses%d" % index
	cones.brain = member.brain
	cones.visible = false
	add_child(cones)
	senses_debug.append(cones)


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
		var senses := member.brain.senses
		var doing: String = member.state if squad.knowledge.knows() else member.brain.state
		lines.append("%s%d %-11s %-26s %-10s suspicion %.2f%s" % [">" if i == squad_debug.selected else " ", i,
			PositionScorer.Role.keys()[member.role].to_lower(), doing, CreatureSenses.Awareness.keys()[senses.awareness].to_lower(),
			senses.suspicion, "  [token]" if squad.tokens.has(member) else ""])
	var knowledge := squad.knowledge
	var plan_name: String = SquadPlan.Kind.keys()[squad.plan].to_lower() + (" (auto)" if squad.forced_plan < 0 else " (forced)")
	hud.text = "squad: %s, plan %s, unseen %.1f s   tokens %d (%d free)   grid %d cells   player sees %d cells\n%s\nclick = walk   shift-click = put   right-click = look   N noise   T tokens   J plan   X kill   Tab member   Q role   H heatmap   K cones   G grid   V colours   C camera" % [
		SquadKnowledge.State.keys()[knowledge.state].to_lower(), plan_name, knowledge.unseen_for, squad.tokens.count, squad.tokens.free_count(),
		grid.cell_count(), visibility.count(PlayerVisibility.Sight.SEEN), "\n".join(lines)]


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
			member.slot_angle = NAN
			member.cell = -1
		KEY_J:
			squad.forced_plan = squad.forced_plan + 1 if squad.forced_plan < SquadPlan.Kind.size() - 1 else -1
		KEY_X: _kill(_selected())
		KEY_H: squad_debug.show_heatmap = not squad_debug.show_heatmap
		KEY_G: debug.visible = not debug.visible
		KEY_V:
			debug.show_visibility = not debug.show_visibility
			debug.refresh()
		KEY_N: NoiseBus.emit(player.orb.global_position, 10.0)
		KEY_T: squad.tokens.count = squad.tokens.count % 3 + 1
		KEY_K:
			for cones in senses_debug:
				cones.visible = not cones.visible
		KEY_C:
			follow = not follow
			if not follow:
				camera.transform = overview


# The selected member dies: out of the squad, its nodes freed.
func _kill(member: Squad.Member) -> void:
	if squad.members.size() <= 1:
		return
	squad.remove_member(member)
	for cones in senses_debug.duplicate():
		if cones.brain == member.brain:
			senses_debug.erase(cones)
			cones.queue_free()
	member.brain.queue_free()
	member.walker.queue_free()
	member.spider.queue_free()
	_select(squad_debug.selected)
