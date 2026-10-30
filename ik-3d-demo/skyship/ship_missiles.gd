## The skyship's missiles: every missile attack (StitchMissiles, …) only says where and when; this
## node flies them and lands them.
##
## launch(source, target, wait, flight, reveal):
##   `reveal` s from now a marker appears on the floor at `target` — a red ring the size of the
##   blast with a disc inside that fills up as the hit comes (so you can read how long you have);
##   `wait` s from now the missile leaves `source` (a pod on the ship), dives along a curve and
##   lands `flight` s later in a blast (sparks, a flash): `on_land(at, radius)` — the demo hurts
##   the player and the bots inside it.
## Missiles run on the game's time (tick(), from Skyship), so hit-stop and slow motion apply.
##
## By default a missile dives along a gentle curve. `snake(arc, wave, waves)` on the last launch
## gives it a cubic Bezier instead — up `arc` m out of its launcher, over and down — with a sine
## wave across it: `wave` m to either side, `waves` half-swings, none at either end (so it still
## leaves the tube and lands on its marker exactly).
class_name ShipMissiles
extends Node3D

const BLAST := 2.3 ## blast radius (m)
const LIFT := 2.0 ## m the dive curve's middle point sits above the straight line (they come nearly straight in: fast)

var sparks: OrbSparks
var on_land := Callable() ## (at: Vector3, radius: float)

var _flying: Array[Dictionary] = [] ## {source, target, age (negative = waiting), flight, reveal, body, ring, fill}
var _ring_mesh := ShipKit.ring(BLAST * 0.9, BLAST)
var _fill_mesh := ShipKit.cylinder(BLAST * 0.88, BLAST * 0.88, 0.01, 32)


func _ready() -> void:
	top_level = true


func busy() -> bool:
	return not _flying.is_empty()


func launch(
		source: Vector3,
		target: Vector3,
		wait: float,
		flight: float,
		reveal := 0.0
) -> void:
	var ring := ShipKit.add(self, _ring_mesh, ShipKit.ghost(Color(ShipKit.RED, 0.0)))
	ring.position = ShipKit.flat(target) + Vector3.UP * 0.04
	ring.scale = Vector3(1, 0.12, 1)
	ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var fill := ShipKit.add(self, _fill_mesh, ShipKit.ghost(Color(ShipKit.RED, 0.0)))
	fill.position = ShipKit.flat(target) + Vector3.UP * 0.03
	fill.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_flying.append({"source": source, "target": ShipKit.flat(target), "age": -wait, "flight": flight,
			"reveal": wait - reveal, "body": _missile(), "ring": ring, "fill": fill, "arc": 0.0, "wave": 0.0, "waves": 0.0})


## Make the missile just launched a snake: a cubic Bezier `arc` m high with a sine wave across it.
func snake(arc: float, wave: float, waves: float) -> void:
	if _flying.is_empty():
		return
	var missile := _flying[-1]
	missile.arc = arc
	missile.wave = wave
	missile.waves = waves


func clear() -> void:
	for missile in _flying:
		_free(missile)
	_flying.clear()


func tick(delta: float) -> void:
	for i in range(_flying.size() - 1, -1, -1):
		var missile := _flying[i]
		missile.age += delta
		var age: float = missile.age
		var ring: MeshInstance3D = missile.ring
		var fill: MeshInstance3D = missile.fill
		var body: Node3D = missile.body
		var until: float = -age + missile.flight # seconds to the hit
		var shown: bool = -age <= missile.reveal
		ring.visible = shown
		fill.visible = shown
		if shown:
			var t := clampf(1.0 - until / (missile.reveal + missile.flight), 0.0, 1.0) # 0 revealed … 1 hit
			var pulse := 0.5 + 0.5 * sin(age * lerpf(8.0, 30.0, t))
			(ring.material_override as StandardMaterial3D).albedo_color = Color(ShipKit.RED, lerpf(0.35, 0.95, t) * lerpf(0.6, 1.0, pulse))
			(fill.material_override as StandardMaterial3D).albedo_color = Color(ShipKit.RED, 0.12 + 0.25 * t)
			fill.scale = Vector3(t, 1.0, t) + Vector3(0.02, 0, 0.02)
		if age < 0.0:
			body.visible = false
			continue
		var u := clampf(age / missile.flight, 0.0, 1.0)
		var point := _curve(missile, u)
		var ahead := _curve(missile, minf(u + 0.02, 1.0))
		body.visible = true
		if ahead.distance_to(point) > 1e-3:
			var y := (ahead - point).normalized()
			var x := y.cross(Vector3.UP if absf(y.y) < 0.99 else Vector3.RIGHT).normalized()
			body.global_transform = Transform3D(Basis(x, y, x.cross(y)), point)
		if sparks != null:
			sparks.burst(point - (ahead - point).normalized() * 0.35, point - ahead, 1, 2.0, 0.3, 0.25, 0.04, 0.0, Color(1.0, 0.7, 0.3))
		if u >= 1.0:
			_explode(missile.target)
			_free(missile)
			_flying.remove_at(i)


func _curve(missile: Dictionary, u: float) -> Vector3:
	var a: Vector3 = missile.source
	var c: Vector3 = missile.target
	if missile.arc > 0.0: # the snake: a cubic Bezier, plus the sine wave across it
		var p1 := a + Vector3.UP * float(missile.arc)
		var p2 := c + Vector3.UP * float(missile.arc) * 0.7 + (a - c) * 0.15
		var point := Bezier.point(a, p1, p2, c, u)
		var across := Vector3(c.x - a.x, 0, c.z - a.z).cross(Vector3.UP).normalized()
		return point + across * float(missile.wave) * sin(u * PI * float(missile.waves)) * sin(u * PI)
	var b := a.lerp(c, 0.35) + Vector3.UP * LIFT
	return a.lerp(b, u).lerp(b.lerp(c, u), u)


func _explode(at: Vector3) -> void:
	if sparks != null:
		sparks.burst(at, Vector3.UP, 26, 8.0, 1.0, 0.5, 0.045, 0.0, Color(1.0, 0.8, 0.45))
		sparks.embers(at, 4, 3.0, 0.0)
	var flash := OmniLight3D.new()
	flash.light_color = Color(1.0, 0.55, 0.2)
	flash.light_energy = 6.0
	flash.omni_range = 5.0
	add_child(flash)
	flash.global_position = at + Vector3.UP * 0.6
	var tween := flash.create_tween()
	tween.tween_property(flash, "light_energy", 0.0, 0.3)
	tween.tween_callback(flash.queue_free)
	if on_land.is_valid():
		on_land.call(at, BLAST)


func _free(missile: Dictionary) -> void:
	(missile.body as Node3D).queue_free()
	(missile.ring as Node3D).queue_free()
	(missile.fill as Node3D).queue_free()


func _missile() -> Node3D:
	var root := Node3D.new()
	add_child(root)
	var hull := CapsuleMesh.new()
	hull.radius = 0.1
	hull.height = 0.65
	ShipKit.add(root, hull, ShipKit.metal(ShipKit.DARK))
	ShipKit.add(root, ShipKit.sphere(0.11, 8), ShipKit.glow(Color(2.5, 1.4, 0.5), 3.0), Vector3.DOWN * 0.36)
	root.visible = false
	return root
