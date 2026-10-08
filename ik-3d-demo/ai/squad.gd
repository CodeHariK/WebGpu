## A squad of spiders closing in on the player by role (the start of the squad director).
## Every RESCORE seconds each member, in turn, scores the cells around the player for its role
## (PositionScorer) and claims the best one; later members keep their distance from earlier
## claims. A member only moves its claim when the new cell is clearly better (HYSTERESIS).
## Hidden roles route round what the player can see (exposure as A* cell cost).
##
## Member states:
##   moving      walking to its claimed cell
##   holding     on the cell, facing the player. A flanker / behind member that has held
##               HOLD_BEFORE_ATTACK and is outside the player's view (more than ATTACK_ANGLE off
##               its facing) pounces
##   attacking   runs straight at the player
##   recovering  reached the player: pauses RECOVER seconds, then goes back to its spot
## Routes (except the attack run) go round the player (PERSONAL_SPACE), so a member getting to the
## other side doesn't walk through it.
## For now the squad simply knows where the player is (shared memory comes with the director).
class_name Squad
extends Node

const RESCORE := 0.4 ## seconds
const HYSTERESIS := 0.75 ## a new cell must beat the current claim's score by this much
const HOLD_BEFORE_ATTACK := 0.6
const ATTACK_ANGLE := deg_to_rad(70.0)
const RECOVER := 0.8
const PERSONAL_SPACE := 2.0 ## metres round the player that routes avoid …
const PERSONAL_SPACE_COST := 6.0 ## … at this cost per cell
const SEEN_COST := 4.0 ## route cost per cell the player can see (hidden roles only) …
const IN_VIEW_COST := 10.0 ## … and per cell in its view right now
const SPEED := {"moving": 0.85, "holding": 1.0, "attacking": 1.0, "recovering": 0.7}


class Member:
	var spider: Spider
	var walker: GridWalker
	var role := PositionScorer.Role.PRESSURE
	var goal := Marker3D.new()
	var cell := -1 ## claimed cell
	var state := "moving"
	var timer := 0.0
	var scores := {} ## last re-score (for the debug heatmap)


var grid: TacticalGrid
var visibility: PlayerVisibility
var player: TestPlayer
var members: Array[Member] = []
var enabled := true:
	set(value):
		enabled = value
		for member in members:
			member.walker.active = true
			member.spider.speed_scale = 1.0

var _rescore_left := 0.0


func add_member(spider: Spider, walker: GridWalker, role: PositionScorer.Role) -> Member:
	var member := Member.new()
	member.spider = spider
	member.walker = walker
	member.role = role
	member.goal.name = "%sGoal" % spider.name
	add_child(member.goal)
	walker.target = member.goal
	members.append(member)
	return member


func _physics_process(delta: float) -> void:
	if not enabled or grid == null or player == null or visibility == null:
		return
	_rescore_left -= delta
	if _rescore_left <= 0.0:
		_rescore_left = RESCORE
		_rescore()
	for member in members:
		member.timer += delta
		_update_member(member)


# Each member in turn claims its best cell; claims made earlier this round push later ones apart.
func _rescore() -> void:
	var context := PositionScorer.Context.new()
	context.grid = grid
	context.visibility = visibility
	context.player = player.orb.global_position + Vector3.DOWN * 0.5
	context.facing = visibility.facing
	var exposure := visibility.exposure_costs(SEEN_COST, IN_VIEW_COST)
	var around := _personal_space_costs(context.player)
	if exposure.size() != around.size(): # visibility not swept yet
		exposure.resize(around.size())
		exposure.fill(0.0)
	for cell in exposure.size():
		exposure[cell] += around[cell]
	var claimed: Array[Vector3] = []
	for member in members:
		context.from = member.spider.global_position
		context.others = claimed
		member.scores = PositionScorer.score_all(member.role, context)
		var best := PositionScorer.best(member.scores)
		var current: float = member.scores.get(member.cell, -INF)
		if best >= 0 and (member.cell < 0 or member.scores[best] > current + HYSTERESIS):
			_claim(member, best)
		if member.cell >= 0:
			claimed.append(grid.positions[member.cell])
		var hidden_role: bool = PositionScorer.PROFILES[member.role].sight < 0
		if member.state != "attacking":
			member.walker.cell_cost = exposure if hidden_role else around


func _personal_space_costs(player_feet: Vector3) -> PackedFloat32Array:
	var costs := PackedFloat32Array()
	costs.resize(grid.cell_count())
	for cell in grid.cell_count():
		if grid.positions[cell].distance_to(player_feet) < PERSONAL_SPACE:
			costs[cell] = PERSONAL_SPACE_COST
	return costs


func _claim(member: Member, cell: int) -> void:
	var moved := member.cell < 0 or grid.positions[cell].distance_to(grid.positions[member.cell]) > 1.0
	member.cell = cell
	member.goal.global_position = grid.positions[cell]
	if moved and member.state == "holding":
		_set_state(member, "moving")


func _update_member(member: Member) -> void:
	var walker := member.walker
	member.spider.speed_scale = SPEED[member.state]
	var to_player := player.orb.global_position - member.spider.global_position
	match member.state:
		"moving":
			walker.active = true
			walker.target = member.goal
			walker.arrive_distance = 0.35
			if walker.status == "arrived":
				_set_state(member, "holding")
		"holding":
			walker.active = false
			walker.face(to_player)
			if _may_pounce(member):
				_set_state(member, "attacking")
		"attacking":
			walker.active = true
			walker.target = player.orb
			walker.arrive_distance = 0.9
			walker.cell_cost = PackedFloat32Array()
			if walker.status == "arrived":
				_set_state(member, "recovering")
		"recovering":
			walker.active = false
			walker.face(to_player)
			if member.timer > RECOVER:
				_set_state(member, "moving")


func _may_pounce(member: Member) -> bool:
	if PositionScorer.PROFILES[member.role].sight > 0 or member.timer < HOLD_BEFORE_ATTACK:
		return false
	var from_player := member.spider.global_position - player.orb.global_position
	from_player.y = 0.0
	return visibility.facing.angle_to(from_player) > ATTACK_ANGLE


func _set_state(member: Member, state: String) -> void:
	member.state = state
	member.timer = 0.0
	member.walker.status = "walking" if state == "moving" or state == "attacking" else member.walker.status
	member.walker.replan()
