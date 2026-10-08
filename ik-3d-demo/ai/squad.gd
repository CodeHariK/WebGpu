## The squad director: spiders that share what they see and close in on the player by role.
##
## Each member has its own HunterBrain (senses + solo behaviour: wander, investigate, search).
## SquadKnowledge pools their senses; once the squad is ALERT the Squad takes over (brains are
## `directed`: senses only) and steers each member by role:
##   moving      walking to its claimed cell (SquadPositions re-claims every RESCORE seconds)
##   holding     on the cell, facing where the player is. Ready to attack when:
##                 flank / behind  held HOLD_BEFORE_ATTACK and outside the player's view
##                                 (more than ATTACK_ANGLE off its facing)
##                 pressure        held PRESSURE_PATIENCE (it's in front: a lunge)
##               Ready members ask for an attack token (AttackTokens), best shot first
##               (outside the view before in front, then nearest). No token: keep holding
##   attacking   holds a token, runs at the player; done on reaching it or after ATTACK_TIMEOUT
##   recovering  hands the token back, pauses RECOVER seconds, then back to its spot
## Barks (short call-outs over a member, BARK_TIME seconds) show the plan to the player: "there!"
## on spotting, each role on the squad going alert, "mine!" on taking a token, "lost it".
## The squad only knows what its members saw — it aims at the last sighting, not the player.
## (It does know which way the player faces: visibility.facing.)
class_name Squad
extends Node

const RESCORE := 0.4 ## seconds
const HOLD_BEFORE_ATTACK := 0.6
const PRESSURE_PATIENCE := 2.0
const ATTACK_ANGLE := deg_to_rad(70.0)
const ATTACK_TIMEOUT := 4.0
const RECOVER := 0.8
const BARK_TIME := 1.4
const SPEED := {"moving": 0.85, "holding": 1.0, "attacking": 1.0, "recovering": 0.7}
const ROLE_BARKS: Array[String] = ["hold him", "go left", "go right", "behind"]


class Member:
	var spider: Spider
	var walker: GridWalker
	var brain: HunterBrain
	var role := PositionScorer.Role.PRESSURE
	var goal := Marker3D.new()
	var cell := -1 ## claimed cell
	var state := "moving"
	var timer := 0.0
	var scores := {} ## last re-score (for the debug heatmap)
	var route_cost := PackedFloat32Array() ## A* cell costs for getting to its spot


var grid: TacticalGrid
var visibility: PlayerVisibility
var player: TestPlayer
var members: Array[Member] = []
var knowledge := SquadKnowledge.new()
var tokens := AttackTokens.new()
var barks: Array[Dictionary] = [] ## {member, text, until}

var _target := Marker3D.new() ## the last sighting: what attackers run at
var _rescore_left := 0.0
var _was_alert := false
var _time := 0.0


func _ready() -> void:
	_target.name = "LastSighting"
	add_child(_target)


## Add a spider (with its walker) to the squad; it gets its own brain for when the squad isn't alert.
func add_member(spider: Spider, walker: GridWalker, role: PositionScorer.Role) -> Member:
	var member := Member.new()
	member.spider = spider
	member.walker = walker
	member.role = role
	member.goal.name = "%sGoal" % spider.name
	add_child(member.goal)
	member.brain = HunterBrain.new()
	member.brain.name = "%sBrain" % spider.name
	member.brain.spider = spider
	member.brain.walker = walker
	member.brain.grid = grid
	member.brain.player = player.orb
	add_child(member.brain)
	members.append(member)
	return member


func bark(member: Object, text: String) -> void:
	if member == null:
		return
	barks = barks.filter(func(old: Dictionary) -> bool: return old.member != member)
	barks.append({"member": member, "text": text, "until": _time + BARK_TIME})


## Claim `cell` for `member` (from SquadPositions); a holding member walks to its new spot.
func claim(member: Member, cell: int) -> void:
	var moved := member.cell < 0 or grid.positions[cell].distance_to(grid.positions[member.cell]) > 1.0
	member.cell = cell
	member.goal.global_position = grid.positions[cell]
	if moved and member.state == "holding":
		_set_state(member, "moving")


func _physics_process(delta: float) -> void:
	if grid == null or player == null or visibility == null:
		return
	_time += delta
	barks = barks.filter(func(old: Dictionary) -> bool: return old.until > _time)
	knowledge.update(delta, members, bark)
	var alert := knowledge.knows()
	if alert != _was_alert:
		_on_alert_changed(alert)
	if not alert:
		return
	_target.global_position = knowledge.last_known
	tokens.update(delta)
	_rescore_left -= delta
	if _rescore_left <= 0.0:
		_rescore_left = RESCORE
		SquadPositions.rescore(self, _player_feet())
	for member in members:
		member.timer += delta
		_update_member(member)
	_hand_out_tokens()


func _on_alert_changed(alert: bool) -> void:
	_was_alert = alert
	for member in members:
		member.brain.directed = alert
		member.cell = -1
		tokens.release(member)
		_set_state(member, "moving")
		member.walker.active = true
		member.walker.cell_cost = PackedFloat32Array()
		if alert:
			member.walker.target = member.goal
			bark(member, ROLE_BARKS[member.role])
	_rescore_left = 0.0


func _player_feet() -> Vector3:
	var cell := grid.nearest_cell(knowledge.last_known)
	return grid.positions[cell] if cell >= 0 else knowledge.last_known + Vector3.DOWN * 0.9


func _update_member(member: Member) -> void:
	var walker := member.walker
	member.spider.speed_scale = SPEED[member.state]
	var to_player := knowledge.last_known - member.spider.global_position
	match member.state:
		"moving":
			walker.active = true
			walker.target = member.goal
			walker.arrive_distance = 0.35
			walker.cell_cost = member.route_cost
			if walker.status == "arrived":
				_set_state(member, "holding")
		"holding":
			walker.active = false
			walker.face(to_player)
		"attacking":
			walker.active = true
			walker.target = _target
			walker.arrive_distance = 0.9
			walker.cell_cost = PackedFloat32Array()
			if walker.status == "arrived" or member.timer > ATTACK_TIMEOUT:
				tokens.release(member)
				_set_state(member, "recovering")
		"recovering":
			walker.active = false
			walker.face(to_player)
			if member.timer > RECOVER:
				_set_state(member, "moving")


# Holding members that are ready to attack ask for a token, best shot first.
func _hand_out_tokens() -> void:
	var ready: Array[Member] = []
	for member in members:
		if member.state == "holding" and _attack_priority(member) > 0.0:
			ready.append(member)
	ready.sort_custom(func(a: Member, b: Member) -> bool: return _attack_priority(a) > _attack_priority(b))
	for member in ready:
		if not tokens.request(member):
			break
		bark(member, "mine!")
		_set_state(member, "attacking")


# 0 = not ready. Flankers outside the player's view come first, then pressure; nearer is better.
func _attack_priority(member: Member) -> float:
	var from_player := member.spider.global_position - knowledge.last_known
	from_player.y = 0.0
	var nearness := 1.0 / (1.0 + from_player.length())
	if PositionScorer.PROFILES[member.role].sight > 0:
		return 1.0 + nearness if member.timer >= PRESSURE_PATIENCE else 0.0
	if member.timer < HOLD_BEFORE_ATTACK or visibility.facing.angle_to(from_player) <= ATTACK_ANGLE:
		return 0.0
	return 3.0 + nearness


func _set_state(member: Member, state: String) -> void:
	member.state = state
	member.timer = 0.0
	if state == "moving" or state == "attacking":
		member.walker.status = "walking"
	member.walker.replan()
