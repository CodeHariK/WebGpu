## Chain Bind (G): `chains` chains burst out of the ground spread round the enemy nearest the mouse,
## each coiling a helix round it at its own height (alternating direction, a few frames apart),
## hold it still, then sink back (ChainBind). It doesn't use the blades: it has its own chains,
## drawn by its own renderer so they come and go without touching the blades'.
class_name BindMove
extends BladeMove

var chains := 5
const RANGE := 12.0
var spread := 2.4 ## the chains come out of the ground this far from the enemy
var coil_radius := 0.6 ## and coil round it this tight (the enemy is 0.4 wide)
var coil_turns := 1.25
const LINKS := 60

var _binds: Array[ChainBind] = []
var _renderer: LimbRenderer
var _enemy := -1
var _gripped := false


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "bind"
	_renderer = LimbRenderer.new()
	demo.add_child(_renderer)


## The tuning panel's sliders for this move: [var name, min, max, step].
func tunables() -> Array:
	return [
		["chains", 1, 10, 1],
		["spread", 0.5, 5, 0.1],
		["coil_radius", 0.3, 1.5, 0.05],
		["coil_turns", 0.25, 3, 0.05],
	]


func reset() -> void:
	_binds.clear()
	_renderer.clear()
	_enemy = -1


func start() -> void:
	if not _binds.is_empty():
		return
	var target := demo.enemy_near_mouse(RANGE, false)
	if target < 0:
		return
	var enemy := demo.enemies[target]
	var foot := Vector3(enemy.position.x, 0.0, enemy.position.z)
	var longest := 0.0
	for i in chains:
		var angle := TAU * i / chains + randf_range(-0.3, 0.3)
		var anchor := foot + Vector3(cos(angle), 0.0, sin(angle)) * spread
		var h0 := 0.35 + 0.25 * i
		var direction := 1.0 if i % 2 == 0 else -1.0
		var route := ChainBind.coil_points(anchor, foot, coil_radius, angle, coil_turns, h0, h0 + 0.5, direction)
		var bind := ChainBind.new(route, LINKS, Color(0.32, 0.2, 0.15))
		bind.delay = i * 0.06
		longest = maxf(longest, bind.delay + bind.grow_time + bind.hold_time)
		_binds.append(bind)
		_renderer.add(bind.chain)
	_enemy = target
	_gripped = false
	enemy.bound = longest
	enemy.velocity = Vector3.ZERO


func tick(delta: float) -> void:
	if _binds.is_empty():
		return
	var done := true
	for bind in _binds:
		bind.update(delta)
		if bind.phase >= ChainBind.HOLD and not _gripped: # the first chain closes: grip
			_gripped = true
			demo.enemies[_enemy].flash = 1.0
			demo.hitstop = BladesDemo.HITSTOP
			demo.shake = 0.2
		done = done and bind.phase == ChainBind.DONE
	if done:
		reset()


func draw() -> void:
	_renderer.draw()


## Each bind chain: its points (crosses + dim polyline) and the smooth path through them.
func draw_debug(debug: BladeDebug) -> void:
	var violet := Color(0.75, 0.45, 1.0)
	for bind in _binds:
		for i in bind.points.size():
			debug.cross(bind.points[i], Color.WHITE, 0.06)
			if i > 0:
				debug.line(bind.points[i - 1], bind.points[i], Color(violet, 0.4))
		var last := bind.path[0]
		for i in range(1, 121):
			var p := Bezier.path_point(bind.path, i / 120.0)
			debug.line(last, p, violet)
			last = p
