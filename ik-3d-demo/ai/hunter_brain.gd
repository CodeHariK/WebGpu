## A hunter: one spider driven by what its CreatureSenses know — never by where the player really is.
##   UNAWARE     wanders between nearby cells, slowly, pausing to stand and look
##   SUSPICIOUS  freezes and stares at what it saw / heard, then creeps over to check, then looks
##               around there until the suspicion drains away
##   ALERT       runs at the player's last known position (updated every tick while it sees you)
##   SEARCHING   goes to where you were heading (last known + velocity), then checks the spots near
##               there that it couldn't see when it lost you, looking around at each
## Paths come from the GridWalker (target = the brain's goal marker); when standing and looking the
## walker is paused (active = false) and the brain turns the spider itself.
## In a squad: while `directed`, the senses still run but the Squad drives the spider; when it
## lets go the brain picks up from whatever its senses say then.
class_name HunterBrain
extends Node

const SPEED: Array[float] = [0.45, 0.55, 0.85, 1.0] ## × move_speed per awareness (enum order)
const WANDER_RADIUS := 6.0
const WANDER_PAUSE := Vector2(1.0, 3.0) ## seconds standing between wanders
const LOOK_FIRST := 1.2 ## suspicious: stare this long before walking over
const LOOK_AROUND := 1.6 ## seconds scanning at each checked spot
const SEARCH_RADIUS := 5.0
const SEARCH_POINTS := 4
const PREDICT := 1.5 ## seconds of the player's last velocity to extrapolate when searching

var spider: Spider
var walker: GridWalker
var grid: TacticalGrid
var player: Node3D
var senses := CreatureSenses.new()
var point_offsets: Array[Vector3] = [Vector3(0, -0.4, 0), Vector3(0, 0.5, 0), Vector3(0, 1.1, 0)] ## feet, chest, head from player.global_position
var eye_height := 0.6 ## the spider's eye above its root
var enabled := true:
	set(value):
		enabled = value
		if not value:
			_release()
var directed := false: ## a Squad is steering: only the senses run
	set(value):
		if value != directed:
			_awareness = -1 # re-enter the current awareness when control comes back
		directed = value
var state := "" ## what it's doing, for the HUD
var search_points: Array[Vector3] = []
var goal := Marker3D.new() ## where the walker is sent

var _awareness := -1
var _phase := ""
var _timer := 0.0
var _rng := RandomNumberGenerator.new()
var _player_last := Vector3.INF
var _player_velocity := Vector3.ZERO
var _checking := Vector3.INF ## the interest point being checked (suspicious)
var _searched_from := Vector3.INF ## the last known position the search started from
var _look_base := Vector3.FORWARD


func _ready() -> void:
	goal.name = "HunterGoal"
	add_child(goal)
	NoiseBus.listen(senses)


func _exit_tree() -> void:
	NoiseBus.stop_listening(senses)


func _physics_process(delta: float) -> void:
	if not enabled or spider == null or walker == null or player == null:
		return
	_track_player(delta)
	var facing := -spider.global_basis.z
	facing.y = 0.0
	senses.update(delta, spider.get_world_3d().direct_space_state, eye(), facing, player_points(), _player_velocity)
	if directed:
		return
	if senses.awareness != _awareness:
		_enter(senses.awareness)
	spider.speed_scale = SPEED[senses.awareness]
	walker.target = goal
	_timer += delta
	match senses.awareness:
		CreatureSenses.Awareness.UNAWARE: _wander()
		CreatureSenses.Awareness.SUSPICIOUS: _investigate()
		CreatureSenses.Awareness.ALERT: _hunt()
		CreatureSenses.Awareness.SEARCHING: _search()


func eye() -> Vector3:
	return spider.global_position + Vector3.UP * eye_height


func player_points() -> Array[Vector3]:
	var points: Array[Vector3] = []
	for offset in point_offsets:
		points.append(player.global_position + offset)
	return points


func _track_player(delta: float) -> void:
	var now := player.global_position
	if _player_last != Vector3.INF and delta > 0.0:
		_player_velocity = _player_velocity.lerp((now - _player_last) / delta, 0.3)
	_player_last = now


func _enter(awareness: int) -> void:
	_awareness = awareness
	_timer = 0.0
	_look_base = -spider.global_basis.z
	match awareness:
		CreatureSenses.Awareness.UNAWARE:
			_phase = "pause"
		CreatureSenses.Awareness.SUSPICIOUS:
			_phase = "look"
			_checking = senses.interest
		CreatureSenses.Awareness.ALERT:
			_phase = "hunt"
		CreatureSenses.Awareness.SEARCHING:
			_start_search()


func _release() -> void:
	if walker != null:
		walker.active = true
	if spider != null:
		spider.speed_scale = 1.0
	_awareness = -1
	state = ""


# --- behaviours ------------------------------------------------------------------------------------

func _wander() -> void:
	if _phase == "walk":
		walker.active = true
		state = "wandering"
		if _goal_done():
			_phase = "pause"
			_timer = 0.0
			_look_base = -spider.global_basis.z
		return
	walker.active = false
	walker.face(_look_base)
	state = "idle"
	if _timer > _rng.randf_range(WANDER_PAUSE.x, WANDER_PAUSE.y):
		var spot := _wander_spot()
		if spot != Vector3.INF:
			_go(spot, 0.5)
			_phase = "walk"
		_timer = 0.0


func _investigate() -> void:
	if senses.interest.distance_to(_checking) > 2.0 and _phase != "check": # something new: look again
		_phase = "look"
		_timer = 0.0
		_checking = senses.interest
	match _phase:
		"look":
			walker.active = false
			walker.face(senses.interest - spider.global_position)
			state = "? staring"
			if _timer > LOOK_FIRST:
				_checking = senses.interest
				_go(_checking, 0.6)
				_phase = "check"
		"check":
			walker.active = true
			state = "? checking it out"
			if senses.interest.distance_to(_checking) > 1.0: # it keeps glimpsing you: follow the glimpses
				_checking = senses.interest
				_go(_checking, 0.6)
			if _goal_done():
				_phase = "scan"
				_timer = 0.0
				_look_base = -spider.global_basis.z
		"scan":
			_look_around()
			state = "? looking around"


func _hunt() -> void:
	walker.active = true
	walker.arrive_distance = 0.9
	if goal.global_position.distance_to(senses.last_known) > 0.3:
		goal.global_position = senses.last_known
	state = "! chasing" if senses.sees_player else "! going where it saw you"


func _search() -> void:
	if senses.last_known.distance_to(_searched_from) > 1.5: # heard you somewhere else: start over there
		_start_search()
	match _phase:
		"walk":
			walker.active = true
			state = "… searching (%d spots left)" % search_points.size()
			if _goal_done():
				_phase = "scan"
				_timer = 0.0
				_look_base = -spider.global_basis.z
		"scan":
			_look_around()
			state = "… looking around (%d spots left)" % search_points.size()
			if _timer > LOOK_AROUND:
				if search_points.is_empty():
					search_points = _hidden_spots(senses.last_known, spider.global_position + Vector3.UP * eye_height)
				if not search_points.is_empty():
					_go(search_points.pop_front(), 0.5)
					_phase = "walk"
				_timer = 0.0


func _start_search() -> void:
	_searched_from = senses.last_known
	var predicted := senses.last_known + Vector3(senses.last_known_velocity.x, 0.0, senses.last_known_velocity.z) * PREDICT
	if not grid.clear_between(senses.last_known, predicted): # don't predict through a wall
		predicted = senses.last_known
	search_points = _hidden_spots(predicted, senses.eye)
	_go(predicted, 0.5)
	_phase = "walk"


# Stand and sweep the head left and right around the way it was facing.
func _look_around() -> void:
	walker.active = false
	walker.face(_look_base.rotated(Vector3.UP, sin(_timer * 2.2) * 1.4))


# --- goals -------------------------------------------------------------------------------------

# Send the walker to the cell nearest `at`.
func _go(at: Vector3, arrive: float) -> void:
	var cell := grid.nearest_cell(at)
	goal.global_position = grid.positions[cell] if cell >= 0 else at
	walker.arrive_distance = arrive
	walker.status = "walking"
	walker.replan()


func _goal_done() -> bool:
	return walker.status == "arrived" or walker.status == "no path"


# A reachable cell a few metres away, or INF.
func _wander_spot() -> Vector3:
	for attempt in 6:
		var offset := Vector3(_rng.randf_range(-1.0, 1.0), 0.0, _rng.randf_range(-1.0, 1.0)) * WANDER_RADIUS
		var cell := grid.nearest_cell(spider.global_position + offset)
		if cell >= 0 and not grid.find_path(spider.global_position, grid.positions[cell]).is_empty():
			return grid.positions[cell]
	return Vector3.INF


# Up to SEARCH_POINTS cells near `centre`, at least 2 m apart: first the ones `from_eye` couldn't
# see (where the player could have gone out of sight), nearest to `centre` first with a little
# shuffle; if there aren't enough, open spots spread around `centre` fill up the sweep.
func _hidden_spots(centre: Vector3, from_eye: Vector3) -> Array[Vector3]:
	var candidates: Array = []
	for cell in grid.cell_count():
		var distance := grid.positions[cell].distance_to(centre)
		if distance < SEARCH_RADIUS and absf(grid.positions[cell].y - centre.y) < 2.0:
			candidates.append([distance + _rng.randf() * 1.5, cell])
	candidates.sort_custom(func(a: Array, b: Array) -> bool: return a[0] < b[0])
	var hidden: Array[Vector3] = []
	var open: Array[Vector3] = []
	var rays := 0
	for candidate: Array in candidates:
		if hidden.size() >= SEARCH_POINTS or rays >= 150:
			break
		var spot: Vector3 = grid.positions[candidate[1]]
		if _near_any(hidden, spot, 2.0):
			continue
		rays += 1
		if not grid.ray(from_eye, spot + Vector3.UP * 0.6).is_empty():
			hidden.append(spot)
		elif candidate[0] > 2.5 and not _near_any(open, spot, 2.5):
			open.append(spot)
	open.shuffle()
	for spot in open:
		if hidden.size() >= SEARCH_POINTS:
			break
		if not _near_any(hidden, spot, 2.0):
			hidden.append(spot)
	return hidden


static func _near_any(spots: Array[Vector3], point: Vector3, distance: float) -> bool:
	for spot in spots:
		if spot.distance_to(point) < distance:
			return true
	return false
