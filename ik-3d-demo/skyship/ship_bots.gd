## The skyship's little bots (dropped by BotDrop). Each is a ShipTarget (BOT), so every blade
## move works on them. They come for you; close enough, they zap you (and hop back).
##
## They keep a little apart from each other and stay inside the arena. A dead bot's slot (its
## index in demo.enemies) is reused by the next drop, so the list never grows past the most that
## were ever alive at once.
class_name ShipBots
extends RefCounted

const HEIGHT := 0.8
const HP := 2

var speed := 4.5 ## m/s walking
var damage := 8 ## a zap
var reach := 1.0 ## m: zap range

var _demo: SkyshipDemo
var _ship: Skyship
var _bots: Array[ShipTarget] = []
var _time := 0.0


func setup(owner: SkyshipDemo, boss: Skyship) -> void:
	_demo = owner
	_ship = boss


## Forget every bot (the demo's reset clears demo.enemies itself).
func clear() -> void:
	_bots.clear()


func living() -> Array[ShipTarget]:
	var alive: Array[ShipTarget] = []
	for bot in _bots:
		if bot.alive:
			alive.append(bot)
	return alive


## Drop a bot at `at` (in the air), thrown with `toss`.
func spawn(at: Vector3, toss: Vector3) -> void:
	var bot: ShipTarget = null
	for each in _bots:
		if not each.alive:
			bot = each
			break
	if bot == null:
		bot = ShipTarget.new(_demo, ShipTarget.Kind.BOT, at, HEIGHT, HP)
		bot.on_break = _on_dead
		_build(bot)
		_bots.append(bot)
		_demo.enemies.append(bot)
	bot.revive(at)
	bot.position = at
	bot.velocity = toss + Vector3.UP * 1.0
	bot.stun = 0.4
	bot.node.visible = true


func tick(delta: float) -> void:
	_time += delta
	var bots := living()
	for bot in bots:
		bot.walk = Vector3.ZERO
		if bot.stun > 0.0 or bot.yanked or bot.bound > 0.0 or bot.position.y > bot.stand + 0.05:
			continue
		var to := ShipKit.flat(_demo.player) - ShipKit.flat(bot.position)
		var distance := to.length()
		if distance < reach: # at you: zap
			if bot.cooldown <= 0.0:
				bot.cooldown = 1.2
				_demo.hurt(damage, "bot zap")
				bot.velocity = -to.normalized() * 2.0 + Vector3.UP * 3.0 # hop back
				if _ship.sparks != null:
					_ship.sparks.burst(bot.position, to, 8, 4.0, 0.5, 0.25, 0.03, 0.0, Color(0.6, 0.95, 1.0))
		else:
			bot.walk = to.normalized() * speed
		bot.walk += _apart(bot, bots)
		var out := ShipKit.flat(bot.position).length() - (SkyshipArena.RADIUS - 0.5)
		if out > 0.0: # keep inside the wall
			bot.position -= ShipKit.flat(bot.position).normalized() * out


## Place each living bot's meshes: facing where it walks, legs ticking.
func draw() -> void:
	for bot in _bots:
		bot.node.visible = bot.alive
		if not bot.alive:
			continue
		var heading := Vector3(bot.velocity.x, 0, bot.velocity.z)
		var basis := bot.node.global_transform.basis.orthonormalized()
		if heading.length() > 0.2:
			basis = basis.slerp(Basis.looking_at(heading.normalized(), Vector3.UP), 0.25)
		var bob := absf(sin(_time * 14.0 + bot.position.x)) * 0.06 * clampf(heading.length() / speed, 0.0, 1.0)
		bot.node.global_transform = Transform3D(basis, bot.position + Vector3.UP * bob)
		var legs := bot.node.get_child(bot.node.get_child_count() - 1)
		for i in legs.get_child_count():
			(legs.get_child(i) as Node3D).rotation.x = sin(_time * 14.0 + i * 2.1) * 0.5 * clampf(heading.length(), 0.0, 1.0)
		bot.draw()


# Push away from bots closer than a metre.
func _apart(bot: ShipTarget, bots: Array[ShipTarget]) -> Vector3:
	var push := Vector3.ZERO
	for other in bots:
		if other == bot:
			continue
		var away := ShipKit.flat(bot.position - other.position)
		if away.length() < 1.0 and away.length() > 1e-3:
			push += away.normalized() * (1.0 - away.length()) * 3.0
	return push


func _on_dead(bot: ShipTarget) -> void:
	if _ship.sparks != null:
		_ship.sparks.burst(bot.position, Vector3.UP, 20, 5.0, 1.0, 0.45, 0.04, 0.0)
	bot.node.visible = false


# A bot: a squat dome with a red eye on a ring of three little legs (last child: the legs).
func _build(bot: ShipTarget) -> void:
	bot.shell = ShipKit.metal(Color(0.36, 0.37, 0.4))
	bot.glow = ShipKit.glow(ShipKit.RED, 3.0)
	var dome := ShipKit.add(bot.node, ShipKit.sphere(0.3, 16), bot.shell, Vector3.UP * 0.05)
	dome.scale = Vector3(1.0, 0.7, 1.0)
	ShipKit.add(bot.node, ShipKit.cylinder(0.32, 0.26, 0.12, 16), ShipKit.metal(ShipKit.DARK), Vector3.DOWN * 0.08)
	ShipKit.add(bot.node, ShipKit.sphere(0.075, 8), bot.glow, Vector3(0, 0.1, -0.27))
	var legs := Node3D.new()
	bot.node.add_child(legs)
	for i in 3:
		var hip := Node3D.new()
		legs.add_child(hip)
		var angle := TAU * i / 3.0
		hip.position = Vector3(sin(angle) * 0.22, -0.12, cos(angle) * 0.22)
		hip.rotation.y = angle
		ShipKit.add(hip, ShipKit.box(Vector3(0.05, 0.3, 0.05)), ShipKit.metal(ShipKit.DARK), Vector3(0, -0.13, 0.04))
