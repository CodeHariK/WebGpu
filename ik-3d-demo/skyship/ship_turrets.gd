## The skyship's laser turrets: one ball turret under each arm. They are what fires the ship's
## lasers (the clock hands, the snipe) and what you're there to destroy. Each is a ShipTarget
## (TURRET) placed on the hull every frame. Throw the chains at one (aim assist leads it) or shoot
## it with the revolver — only while the shield is down (ShipShield cycles it, and the ship hangs
## still meanwhile; a shot into the shield just sparks off it). Destroy them all and the ship staggers (Skyship); when it climbs
## back up, they're rebuilt.
##
## A turret turns its barrel toward you; destroyed, it's a black stump that smokes.
class_name ShipTurrets
extends RefCounted

const HEIGHT := 3.0 ## its target size (the revolver's hit radius grows with it)

var hp := 5 ## shots each one takes
var turrets: Array[ShipTarget] = []

var _demo: SkyshipDemo
var _ship: Skyship
var _locals: Array[Vector3] = [] ## where each sits on the hull (ship space)


func setup(owner: SkyshipDemo, boss: Skyship) -> void:
	_demo = owner
	_ship = boss
	for i in Skyship.ARMS:
		var angle := TAU * i / Skyship.ARMS + PI / Skyship.ARMS
		_locals.append(Vector3(sin(angle), 0, cos(angle)) * Skyship.ARM_LENGTH * 0.55 + Vector3.DOWN * 0.55)


## New turrets (after the demo's reset cleared demo.enemies).
func reset() -> void:
	turrets.clear()
	for i in _locals.size():
		var turret := ShipTarget.new(_demo, ShipTarget.Kind.TURRET, Vector3.ZERO, HEIGHT, hp)
		turret.on_break = _on_destroyed
		_build(turret)
		turrets.append(turret)
		_demo.enemies.append(turret)
	rebuild()


func rebuild() -> void:
	for turret in turrets:
		turret.max_hp = hp
		turret.revive(Vector3.ZERO)
	tick()


func living() -> Array[ShipTarget]:
	var alive: Array[ShipTarget] = []
	for turret in turrets:
		if turret.alive:
			alive.append(turret)
	return alive


## Follow the hull; open to damage only while the shield is down (and the ship is flying).
func tick() -> void:
	var open := not _ship.shield.on and _ship.state == Skyship.State.FIGHT
	for i in turrets.size():
		turrets[i].anchor = _ship.global_transform * _locals[i]
		turrets[i].exposed = open
		if turrets[i].alive:
			turrets[i].position = turrets[i].anchor


func draw() -> void:
	var aim := _demo.player + Vector3.UP
	for i in turrets.size():
		var turret := turrets[i]
		var at := _ship.global_transform * _locals[i]
		var look := aim - at
		var basis := Basis.looking_at(look.normalized(), Vector3.UP) if look.length() > 0.1 and absf(look.normalized().y) < 0.999 else Basis.IDENTITY
		turret.node.global_transform = Transform3D(basis.scaled(Vector3.ONE * Skyship.SIZE), at)
		(turret.node.get_child(1) as Node3D).visible = turret.alive # the barrel
		(turret.node.get_child(2) as Node3D).visible = turret.alive # the eye
		turret.shell.albedo_color = Color(0.3, 0.31, 0.35) if turret.alive else Color(0.05, 0.05, 0.05)
		if not turret.alive and _ship.sparks != null and randf() < 0.15:
			_ship.sparks.burst(at, Vector3.UP, 2, 2.0, 0.8, 0.6, 0.05, 0.0, Color(0.5, 0.45, 0.4))
		if turret.shielded_hits > 0: # shots into the shield: cyan sparks off it
			turret.shielded_hits = 0
			if _ship.sparks != null:
				_ship.sparks.burst(at + (aim - at).normalized() * 2.0, aim - at, 10, 5.0, 0.7, 0.3, 0.04, 0.0, Color(0.6, 0.95, 1.0))
		turret.draw()


func _on_destroyed(turret: ShipTarget) -> void:
	if _ship.sparks != null:
		_ship.sparks.burst(turret.anchor, Vector3.DOWN, 50, 9.0, 1.0, 0.8, 0.06, 0.0)
		_ship.sparks.embers(turret.anchor, 8, 4.0, 0.0)
	_demo.shake = maxf(_demo.shake, 0.4)
	if living().is_empty():
		_ship.turrets_destroyed()


# A turret: [0] the ball, [1] the barrel (along −Z, toward you), [2] its red eye.
func _build(turret: ShipTarget) -> void:
	turret.shell = ShipKit.metal(Color(0.3, 0.31, 0.35))
	turret.glow = ShipKit.glow(ShipKit.RED, 3.0)
	ShipKit.add(turret.node, ShipKit.sphere(0.42, 16), turret.shell)
	var barrel := ShipKit.add(turret.node, ShipKit.cylinder(0.08, 0.12, 0.9, 8), ShipKit.metal(ShipKit.DARK), Vector3(0, 0, -0.55))
	barrel.rotation.x = PI * 0.5
	ShipKit.add(turret.node, ShipKit.sphere(0.12, 8), turret.glow, Vector3(0, 0.12, -0.36))
