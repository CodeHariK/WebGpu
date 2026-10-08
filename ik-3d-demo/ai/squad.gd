## The squad director: spiders that share what they see and close in on the player by role.
##
## Plan (SquadPlan): on going alert the squad picks PINCER or SURROUND (or the forced one) and
## hands out the parts nearest-first. It re-plans when a member is removed (dies), when the
## automatic choice changes (the player moves into / out of the open), and swaps parts round when
## the player turns more than REASSIGN_TURN from the facing the parts were handed out for (so the
## left flanker doesn't run round the player to stay on its left).
##
## Each member has its own HunterBrain (senses + solo behaviour: wander, investigate, search).
## SquadKnowledge pools their senses; once the squad is ALERT the Squad takes over (brains are
## `directed`: senses only) and steers each member by role:
##   moving      walking to its claimed cell (SquadPositions re-claims every RESCORE seconds)
##   holding     on the cell, facing where the player is. Ready to attack when:
##                 flank / behind  held HOLD_BEFORE_ATTACK and outside the player's view
##                                 (more than ATTACK_ANGLE off its facing)
##                 pressure        held PRESSURE_PATIENCE (it's in front: a lunge)
##                 surround        either of those
##               Ready members ask for an attack token (AttackTokens), best shot first
##               (outside the view before in front, then nearest). No token: keep holding
##   attacking   holds a token, runs at the player; done on reaching it or after ATTACK_TIMEOUT
##   recovering  hands the token back, pauses RECOVER seconds, then back to its spot
## Ambush (player in a car): the spot is beside the road ahead (SquadPositions / AmbushPlanner);
## holding = frozen, watching the road; when the car is BURST_LEAD seconds from its road point it
## leaps at where the car will be after the leap (BURST_FLIGHT) — no token, the car passes once.
## Within HIT_RANGE of the car in that leap = a hit. Then it finds a spot further ahead.
## (The ambushers read the car's real road position — it should come from the last sighting.)
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
const ROLE_BARKS: Array[String] = ["hold him", "go left", "go right", "behind", "close in", "get ahead"]
const BURST_LEAD := 0.75 ## seconds before the car reaches its road point: leap
const BURST_FLIGHT := 0.6 ## about how long a flat leap takes (GridWalker.leap_to)
const LEAP_RANGE := 9.0 ## metres: further than this, don't bother
const HIT_RANGE := 1.8 ## metres from the car's centre in the leap = a hit
const REPLAN := 1.5 ## seconds between checks of the plan
const REASSIGN_TURN := deg_to_rad(100.0)


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
	var slot_angle := NAN ## its own angle round the player (a surround slot); NAN = its role's
	var lead := 0.0 ## ambush: seconds ahead of the car its spot should be
	var ambush_offset := 0.0 ## ambush: the road offset (TestCar) its spot watches
	var hit := false ## ambush: this leap already hit the car


var grid: TacticalGrid
var visibility: PlayerVisibility
var player: TestPlayer
var members: Array[Member] = []
var knowledge := SquadKnowledge.new()
var tokens := AttackTokens.new()
var barks: Array[Dictionary] = [] ## {member, text, until}
var plan := SquadPlan.Kind.PINCER
var hits := 0 ## ambush leaps that reached the car
var forced_plan := -1 ## a SquadPlan.Kind to always use; −1 = choose

var _target := Marker3D.new() ## the last sighting: what attackers run at
var _rescore_left := 0.0
var _replan_left := 0.0
var _plan_facing := Vector3.FORWARD ## the player's facing when the parts were handed out
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
	_replan_left -= delta
	if _replan_left <= 0.0:
		_replan_left = REPLAN
		_check_plan()
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
	if alert:
		_replan(true)
	_rescore_left = 0.0


## Take a member out (it died): its token goes back, the nearest squadmate calls it, and the rest
## re-plan. The caller frees the spider, walker and brain nodes.
func remove_member(member: Member) -> void:
	members.erase(member)
	tokens.release(member)
	barks = barks.filter(func(old: Dictionary) -> bool: return old.member != member)
	member.goal.queue_free()
	if members.is_empty():
		return
	bark(SquadKnowledge._nearest(members, member.spider.global_position), "man down!")
	if knowledge.knows():
		_replan(false)


# Re-plan if the automatic choice changed; swap parts round if the player turned a lot.
func _check_plan() -> void:
	var wanted := _wanted_plan()
	var turned := plan != SquadPlan.Kind.AMBUSH and visibility.facing.angle_to(_plan_facing) > REASSIGN_TURN
	if wanted != plan or turned:
		_replan(false)


func _wanted_plan() -> SquadPlan.Kind:
	if forced_plan >= 0:
		return forced_plan as SquadPlan.Kind
	return SquadPlan.choose(members.size(), grid, visibility, player, _player_feet())


# Pick the plan and hand out its parts. Members whose part changed (everyone with `call_all`, or on
# a new plan) call out their new part and pick a new spot.
func _replan(call_all: bool) -> void:
	var wanted := _wanted_plan()
	var new_plan := wanted != plan
	plan = wanted
	_plan_facing = visibility.facing
	var changed := SquadPlan.assign(plan, members, player, _player_feet(), visibility.facing)
	for member in members:
		if call_all or new_plan or changed.has(member):
			bark(member, ROLE_BARKS[member.role])
		if changed.has(member):
			member.cell = -1
			if member.state == "holding":
				_set_state(member, "moving")
	_rescore_left = 0.0


func _player_feet() -> Vector3:
	var cell := grid.nearest_cell(knowledge.last_known)
	return grid.positions[cell] if cell >= 0 else knowledge.last_known + Vector3.DOWN * 0.9


func _update_member(member: Member) -> void:
	if member.role == PositionScorer.Role.AMBUSH and member.state != "moving":
		_update_ambusher(member)
		return
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


# Waiting beside the road → leap at the car as it passes → land → find the next spot.
func _update_ambusher(member: Member) -> void:
	var car := player as TestCar
	var walker := member.walker
	walker.active = false
	if car == null:
		return
	match member.state:
		"holding":
			var road_point := car.road.sample_baked(member.ambush_offset)
			walker.face(road_point - member.spider.global_position)
			var arrives := car.time_to_offset(member.ambush_offset)
			if arrives <= BURST_LEAD and arrives > -0.3:
				var landing := car.predict(BURST_FLIGHT)
				if landing.distance_to(member.spider.global_position) < LEAP_RANGE and not member.spider.airborne:
					walker.leap_to(landing)
					member.hit = false
					bark(member, "!!")
					_set_state(member, "attacking")
		"attacking":
			if not member.hit and member.spider.global_position.distance_to(car.orb.global_position) < HIT_RANGE:
				member.hit = true
				hits += 1
				bark(member, "got it!")
			if member.timer > 0.2 and not member.spider.airborne:
				_set_state(member, "recovering")
		"recovering":
			if member.timer > RECOVER:
				member.cell = -1
				_set_state(member, "moving")
				_rescore_left = 0.0


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
	if member.role == PositionScorer.Role.AMBUSH:
		return 0.0 # ambushers leap on their own, no token
	var sight: int = PositionScorer.PROFILES[member.role].sight
	var unseen := member.timer >= HOLD_BEFORE_ATTACK and visibility.facing.angle_to(from_player) > ATTACK_ANGLE
	if sight <= 0 and unseen:
		return 3.0 + nearness # flank, behind, surround: outside the player's view
	if sight >= 0 and member.timer >= PRESSURE_PATIENCE:
		return 1.0 + nearness # pressure, surround: in front, a lunge
	return 0.0


func _set_state(member: Member, state: String) -> void:
	member.state = state
	member.timer = 0.0
	if state == "moving" or state == "attacking":
		member.walker.status = "walking"
	member.walker.replan()
