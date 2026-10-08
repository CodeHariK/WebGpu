## Car test: the player drives round a looping road; four spiders hear the engine, spot the car
## and — since you can't catch a car — get ahead of it: each waits beside the road at its own lead
## time, hidden behind the walls and rocks, and leaps at the car as it passes (SquadPlan.AMBUSH,
## AmbushPlanner). The HUD counts the hits.
##   W / S  faster / slower      Space  stop / go
##   Tab  next member      K  vision cones on/off      G  grid on/off
##   X  kill that member   C  camera: overview ↔ follow the car
extends Node3D

const SPAWNS: Array[Vector3] = [Vector3(-5, 0, -5), Vector3(5, 0, -6), Vector3(6, 0, 6), Vector3(-6, 0, 7)]
var overview := Transform3D(Basis.from_euler(Vector3(-1.0, 0.0, 0.0)), Vector3(0, 36, 24)) ## the overview camera

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var grid := TacticalGrid.new()
var visibility: PlayerVisibility
var car := TestCar.new()
var squad: Squad
var debug: TacticalGridDebug
var squad_debug: SquadDebug
var senses_debug: Array[SensesDebug] = []
var follow := false
var _time := 0.0


func _ready() -> void:
	car.road = TestRoadArena.build(self)
	car.name = "Car"
	car.orb = TestRoadArena.add_car(self)
	add_child(car)
	camera.transform = overview
	await get_tree().physics_frame # colliders exist in the physics world from now on
	TestRoadArena.scan_grid(grid, self)
	visibility = PlayerVisibility.new()
	visibility.grid = grid
	visibility.rays_per_frame = 128 # a bigger field
	squad = Squad.new()
	squad.name = "Squad"
	squad.grid = grid
	squad.visibility = visibility
	squad.player = car
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
	squad_debug.show_heatmap = false
	add_child(squad_debug)


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
	var member := squad.add_member(spider, walker, PositionScorer.Role.AMBUSH)
	member.brain.point_offsets = [Vector3.ZERO, Vector3(0, 0.7, 0)] # the car's body and cab
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
	visibility.update(_time, car.eye(), car.facing)
	if follow:
		var behind := car.orb.global_position - car.facing * 9.0 + Vector3(0, 6.0, 0)
		camera.global_position = camera.global_position.lerp(behind, 1.0 - exp(-3.0 * delta))
		camera.look_at(car.orb.global_position)
	_update_hud()


func _update_hud() -> void:
	var lines: Array[String] = []
	for i in squad.members.size():
		var member := squad.members[i]
		var doing: String = member.state if squad.knowledge.knows() else member.brain.state
		var arrives := car.time_to_offset(member.ambush_offset) if member.cell >= 0 else INF
		lines.append("%s%d %-10s %-26s car there in %4.1f s" % [">" if i == squad_debug.selected else " ", i,
			PositionScorer.Role.keys()[member.role].to_lower(), doing, arrives])
	hud.text = "car %.1f m/s   squad: %s, plan %s   hits %d   grid %d cells (scan %.0f ms)\n%s\nW/S speed   Space stop/go   Tab member   K cones   G grid   X kill   C camera" % [
		car.speed, SquadKnowledge.State.keys()[squad.knowledge.state].to_lower(), SquadPlan.Kind.keys()[squad.plan].to_lower(),
		squad.hits, grid.cell_count(), grid.scan_msec, "\n".join(lines)]


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo or squad == null:
		return
	match key.keycode:
		KEY_TAB: squad_debug.selected = (squad_debug.selected + 1) % squad.members.size()
		KEY_G: debug.visible = not debug.visible
		KEY_K:
			for cones in senses_debug:
				cones.visible = not cones.visible
		KEY_X: _kill(squad.members[squad_debug.selected % squad.members.size()])
		KEY_C:
			follow = not follow
			if not follow:
				camera.transform = overview


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
	squad_debug.selected = squad_debug.selected % squad.members.size()
