## Clock hands: the ship stops, drives a beam straight down into the floor under it, and laser
## walls sweep round that point like the hands of a clock, out to the arena's wall — fast, far
## too fast to outrun. Each hand has a door: a gap a few metres wide at some distance from the
## middle. You can't jump the wall, so you find the door's ring (the telegraph shows it) and stand
## in it while the hands go round — or run all the way in to the hub, the cyan circle right under
## the ship that no hand reaches. Later phases: more hands, doors at different distances (stand
## on one ring and another hand's wall goes through you), faster turns. Each hand needs a living
## turret: shoot turrets down and the clock loses hands.
##
## The telegraph draws each hand as a thin red line where it starts, with its door cut out and
## a tick mark showing which way it will turn, and rings the hub and every door's distance on the
## floor; the walls shoot up at FIRE and sink at RECOVER.
##
## params: hands — [[start offset °, speed °/s, door (m nearer / further than you stand now)], …]
##         (the first hand starts a quarter turn off you), door (m wide), hub (m, the safe
##         circle), width (m, how close to a wall hurts), damage, + the stage lengths.
class_name ClockLaser
extends ShipAttack

const WALL_HEIGHT := 1.6
const WALL_THICK := 0.25
const MAX_HANDS := 4

var _hands: Array = [] ## [angle (rad), speed (rad/s), door middle (m from the hub's centre)]
var _hub := Vector3.ZERO
var _hub_radius := 3.0
var _door := 4.0
var _walls: Array[MeshInstance3D] = [] ## two per hand: inside and outside the door
var _lines: Array[MeshInstance3D] = []
var _ticks: Array[MeshInstance3D] = []
var _door_rings: Array[MeshInstance3D] = []
var _hub_ring: MeshInstance3D
var _beam: MeshInstance3D
var _wall_material := ShipKit.glow(Color(1.0, 0.22, 0.08), 5.0)
var _line_material := ShipKit.ghost(Color(ShipKit.RED, 0.0))
var _safe_material := ShipKit.ghost(Color(ShipKit.CYAN, 0.0))


func _init() -> void:
	key = "clock"
	title = "clock hands"


func _build() -> void:
	_hub_ring = _flat(ShipKit.add(self, ShipKit.ring(0.92, 1.0), _safe_material))
	_beam = ShipKit.add(self, ShipKit.cylinder(0.35, 0.35, 1.0, 12), ShipKit.glow(Color(1.0, 0.35, 0.1), 6.0))
	_beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	for i in MAX_HANDS:
		for part in 2:
			_walls.append(_flat(ShipKit.add(self, ShipKit.box(Vector3.ONE), _wall_material)))
			_lines.append(_flat(ShipKit.add(self, ShipKit.box(Vector3.ONE), _line_material)))
		_ticks.append(_flat(ShipKit.add(self, ShipKit.box(Vector3.ONE), _line_material)))
		_door_rings.append(_flat(ShipKit.add(self, ShipKit.ring(0.992, 1.0), _safe_material)))
	_on_stop()


func _flat(instance: MeshInstance3D) -> MeshInstance3D:
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return instance


func _on_begin(p: Dictionary) -> void:
	_hub = ship.ground_point()
	_hub_radius = float(p.get("hub", 3.0))
	_door = float(p.get("door", 4.0))
	var reach := _reach()
	var to_player := ShipKit.flat(demo.player) - _hub
	var base := atan2(to_player.x, to_player.z) + PI * 0.5 # a quarter turn off you
	_hands.clear()
	var hands: Array = p.get("hands", [[0.0, 60.0, 0.0]])
	hands = hands.slice(0, maxi(ship.turrets.living().size(), 1)) # one hand per turret still firing
	for hand: Array in hands:
		var off: float = hand[2] if hand.size() > 2 else randf_range(-6.0, 6.0)
		var door := clampf(to_player.length() + off, _hub_radius + _door * 0.5, minf(_hub_radius + reach, SkyshipArena.RADIUS + _hub.length()) - _door)
		_hands.append([base + deg_to_rad(float(hand[0])), deg_to_rad(float(hand[1])), door])
	_hub_ring.visible = true
	_hub_ring.position = _hub + Vector3.UP * 0.05
	_hub_ring.scale = Vector3(_hub_radius, 0.12, _hub_radius)
	for i in _door_rings.size(): # a faint ring at each door's distance: stand on it
		var on := i < _hands.size()
		_door_rings[i].visible = on
		if on:
			var r: float = _hands[i][2]
			_door_rings[i].position = _hub + Vector3.UP * 0.04
			_door_rings[i].scale = Vector3(r, 0.12, r)
	ship.hold(true)


# The walls reach from the hub to a little past the arena's wall, wherever the hub is.
func _reach() -> float:
	return SkyshipArena.RADIUS + _hub.length() + 1.0 - _hub_radius


func _tick(now: Stage, t: float, delta: float) -> void:
	var reach := _reach()
	var height := 0.0
	match now:
		Stage.TELEGRAPH:
			_line_material.albedo_color = Color(ShipKit.RED, 0.3 + 0.6 * t * (0.6 + 0.4 * sin(time * 22.0)))
			_safe_material.albedo_color = Color(ShipKit.CYAN, 0.85 * t)
		Stage.FIRE:
			height = minf(time / 0.12, 1.0) # up in a blink
			if time < 0.12:
				demo.shake = maxf(demo.shake, 0.3)
			for hand: Array in _hands:
				hand[0] += hand[1] * delta
			_line_material.albedo_color = Color(ShipKit.RED, 0.0)
			_hurt()
		Stage.RECOVER:
			height = 1.0 - t
			_safe_material.albedo_color = Color(ShipKit.CYAN, 0.85 * (1.0 - t))
	for i in MAX_HANDS:
		var on := i < _hands.size()
		for part in 2:
			_walls[i * 2 + part].visible = on and height > 0.01
			_lines[i * 2 + part].visible = on and now == Stage.TELEGRAPH
		_ticks[i].visible = on and now == Stage.TELEGRAPH
		if not on:
			continue
		var angle: float = _hands[i][0]
		var door: float = _hands[i][2]
		var direction := Vector3(sin(angle), 0, cos(angle))
		var side := direction.cross(Vector3.UP) * signf(_hands[i][1])
		var spans := [[_hub_radius, door - _door * 0.5], [door + _door * 0.5, _hub_radius + reach]]
		for part in 2:
			var from: float = spans[part][0]
			var to: float = spans[part][1]
			var middle := _hub + direction * (from + to) * 0.5
			_walls[i * 2 + part].global_transform = Transform3D(Basis(direction * (to - from), Vector3.UP * WALL_HEIGHT * maxf(height, 0.01), side * WALL_THICK),
					middle + Vector3.UP * WALL_HEIGHT * height * 0.5)
			_lines[i * 2 + part].global_transform = Transform3D(Basis(direction * (to - from), Vector3.UP * 0.02, side * 0.12), middle + Vector3.UP * 0.03)
		_ticks[i].global_transform = Transform3D(Basis(direction * 0.12, Vector3.UP * 0.02, side * 2.5), _hub + direction * (_hub_radius + reach * 0.35) - side * 1.25 + Vector3.UP * 0.03) # "this way"
		if now == Stage.FIRE and ship.sparks != null and height >= 1.0:
			ship.sparks.burst(_hub + direction * minf(_hub_radius + reach, SkyshipArena.RADIUS), -direction + Vector3.UP, 2, 6.0, 0.6, 0.3)
	_beam.visible = height > 0.01
	if _beam.visible:
		ShipKit.beam(_beam, ship.belly(), _hub, maxf(height, 0.2))


# A wall touching you (outside the hub, not in its door) hurts; it cuts the ship's own bots too.
func _hurt() -> void:
	var width := float(params.get("width", 0.5))
	if _touches(demo.player, width):
		demo.hurt(int(params.get("damage", 20)), "clock hands")
	for bot in ship.bots.living():
		if bot.cooldown <= 0.0 and _touches(bot.position, 0.4):
			bot.cooldown = 0.5
			bot.flash = 1.0
			bot.damage(1)


func _touches(point: Vector3, width: float) -> bool:
	var offset := ShipKit.flat(point) - _hub
	var r := offset.length()
	if r < _hub_radius - 0.1:
		return false
	for hand: Array in _hands:
		var direction := Vector3(sin(hand[0]), 0, cos(hand[0]))
		var in_door: bool = absf(r - float(hand[2])) < _door * 0.5 - 0.3
		if not in_door and offset.dot(direction) > 0.0 and absf(offset.dot(direction.cross(Vector3.UP))) < width:
			return true
	return false


func _on_stop() -> void:
	if ship != null:
		ship.hold(false)
	_hub_ring.visible = false
	_beam.visible = false
	for each in _walls + _lines:
		each.visible = false
	for each in _ticks + _door_rings:
		each.visible = false
