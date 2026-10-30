## Snipe: a thin red targeting line drops from one of the ship's laser turrets onto you and follows you (a little behind,
## so running bends it), blinking faster and faster. Then it LOCKS — it goes white and stops
## following — and a beat later a fat beam slams down that line and burns through to the floor.
## Run sideways the moment it locks (or dash through it). Fires `shots` times in a row; the
## locks come quicker in later phases.
##
## params: shots, track (s it follows you), lock (s between the lock and the shot), lag (s the
##         line trails you by), width (m: how close to the beam hurts), damage, + the stage
##         lengths (FIRE should be about shots × (track + lock + BLAST_TIME)).
class_name SnipeLaser
extends ShipAttack

const BLAST_TIME := 0.18 ## s the big beam lasts

var _line: MeshInstance3D
var _blast: MeshInstance3D
var _spot: MeshInstance3D
var _line_material := ShipKit.ghost(Color(ShipKit.RED, 0.8))
var _aim := Vector3.ZERO
var _shot_time := 0.0
var _hit_this_shot := false
var _shot := 0 ## which shot of the volley
var _turret: ShipTarget ## the turret firing this shot (null: the belly)


func _init() -> void:
	key = "snipe"
	title = "snipe"


func _build() -> void:
	_line = ShipKit.add(self, ShipKit.cylinder(0.03, 0.03, 1.0, 6), _line_material)
	_blast = ShipKit.add(self, ShipKit.cylinder(1.0, 1.0, 1.0, 16), ShipKit.glow(Color(1.0, 0.85, 0.7), 8.0))
	_spot = ShipKit.add(self, ShipKit.ring(0.8, 1.0), ShipKit.ghost(Color(ShipKit.RED, 0.9)))
	for each: GeometryInstance3D in [_line, _blast, _spot]:
		each.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_on_stop()


func _on_begin(_p: Dictionary) -> void:
	_aim = ShipKit.flat(demo.player)
	_shot_time = 0.0
	_shot = 0
	_hit_this_shot = false
	_turret = ship.laser_turret()


func _tick(now: Stage, t: float, delta: float) -> void:
	var track := float(params.get("track", 1.0))
	var lock := float(params.get("lock", 0.3))
	var cycle := track + lock + BLAST_TIME
	var follow := 1.0 - exp(-delta / maxf(float(params.get("lag", 0.25)), 0.01))
	var phase := ""
	match now:
		Stage.TELEGRAPH:
			phase = "track"
			_line_material.albedo_color = Color(ShipKit.RED, 0.8 * t)
		Stage.FIRE:
			var index := int(time / cycle)
			if index != _shot:
				_shot = index
				_hit_this_shot = false
				_turret = ship.laser_turret() # each shot from another turret
			_shot_time = time - index * cycle
			if index >= int(params.get("shots", 3)):
				phase = ""
			elif _shot_time < track:
				phase = "track"
			elif _shot_time < track + lock:
				phase = "lock"
			else:
				phase = "blast"
		Stage.RECOVER:
			phase = ""
	if phase == "track":
		_aim = _aim.lerp(ShipKit.flat(demo.player), follow)
		var blink := 0.5 + 0.5 * sin(_shot_time * lerpf(10.0, 40.0, _shot_time / track))
		if now == Stage.FIRE:
			_line_material.albedo_color = Color(ShipKit.RED, 0.5 + 0.5 * blink)
	elif phase == "lock":
		_line_material.albedo_color = Color(1, 1, 1, 0.95)
	var from := _turret.anchor if _turret != null and _turret.alive else ship.belly()
	var to := _aim + (_aim - from).normalized() * 0.0
	_line.visible = phase == "track" or phase == "lock"
	_spot.visible = _line.visible
	_blast.visible = phase == "blast"
	if _line.visible:
		ShipKit.beam(_line, from, to, 1.0 if phase == "track" else 2.0)
		_spot.position = to + Vector3.UP * 0.05
		_spot.scale = Vector3(1, 0.1, 1) * (1.6 if phase == "lock" else 1.0)
	if _blast.visible:
		var grow := clampf((_shot_time - track - lock) / 0.05, 0.1, 1.0)
		ShipKit.beam(_blast, from, to, float(params.get("width", 1.1)) * grow)
		if not _hit_this_shot:
			_hit_this_shot = true
			demo.shake = maxf(demo.shake, 0.6)
			if ship.sparks != null:
				ship.sparks.burst(to, Vector3.UP, 40, 10.0, 1.0, 0.6, 0.05, 0.0, Color(1.0, 0.85, 0.6))
				ship.sparks.embers(to, 6, 4.0, 0.0)
		if _distance_to_beam(demo.player + Vector3.UP, from, to) < float(params.get("width", 1.1)):
			demo.hurt(int(params.get("damage", 30)), "snipe")


static func _distance_to_beam(point: Vector3, from: Vector3, to: Vector3) -> float:
	return point.distance_to(Geometry3D.get_closest_point_to_segment(point, from, to))


func _on_stop() -> void:
	_line.visible = false
	_blast.visible = false
	_spot.visible = false
