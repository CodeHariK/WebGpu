## Something of the skyship's that the blades can hit: a bot, a laser turret or the ship's core.
## It is a DemoEnemy, so every blade move (throw, pull, lasso, whirl, bind, slam …) works on it
## unchanged — the moves only read and write `position`, `velocity`, `flash`, `yanked`, `bound`
## and `head_top()`. What it shows and how it moves is its own:
##
##   BOT    a small walker: falls with gravity, slides to a stop, walks where ShipBots steers it
##          (`walk`), can be bound, pulled, lassoed and thrown. Dies after `hp` hits.
##   TURRET one of the ship's laser turrets, up on the hull (ShipTurrets puts it at `anchor`
##          each frame). Hit by a chain throw or the revolver — only while the shield is down
##          (`exposed`). Any fresh hit (its `flash` jumping up: the blades' hit() and the
##          revolver both set it) costs it a hit point.
##   CORE   the ship's belly core. Pinned to the ship (`anchor`); only out where the blades can
##          reach while the ship is staggered (`exposed`), else parked far below the floor so no
##          move can find it.
##
## A dead / hidden target is parked at HIDDEN (moves keep indices into demo.enemies, so targets
## are never removed from that list; dead bot slots are reused by ShipBots). Its meshes (`node`)
## sit under DemoEnemy's `mesh`, which stays at the origin: whoever built it places `node` in
## world space each frame.
class_name ShipTarget
extends DemoEnemy

enum Kind { BOT, CORE, TURRET }

const HIDDEN := Vector3(0, -100, 0)

var kind := Kind.BOT
var hp := 3
var max_hp := 3
var alive := true
var exposed := false ## the core / a turret: open to damage now
var anchor := Vector3.ZERO ## where a turret / the core is held
var walk := Vector3.ZERO ## a bot: the ground velocity it wants (ShipBots)
var stun := 0.0 ## a bot: seconds it can't walk (just hit / thrown)
var hurt_flash := 0.0 ## for the hud: 1 on damage
var on_break := Callable() ## called once when it dies: (target)
var node: Node3D ## its own meshes (under `mesh`)
var glow: StandardMaterial3D ## the part that lights up on a hit (eye, core)
var shell: StandardMaterial3D ## the body's material
var shielded_hits := 0 ## a turret: shots that hit the shield instead (for the sparks)
var motion := Vector3.ZERO ## m/s it's moving now (measured: aim assist leads it)

var _seen_flash := 0.0
var _last_position := Vector3.INF


func _init(
		parent: Node3D,
		new_kind: Kind,
		where: Vector3,
		new_height: float,
		new_hp: int
) -> void:
	super(parent, Vector3(where.x, 0.0, where.z), new_height)
	kind = new_kind
	hp = new_hp
	max_hp = new_hp
	anchor = position
	for child in mesh.get_children(): # DemoEnemy's robot and capsule: never shown
		(child as Node3D).visible = false
	node = Node3D.new()
	mesh.add_child(node)


## Take `amount` damage. Returns true if that finished it.
func damage(amount: int) -> bool:
	if not alive or (kind != Kind.BOT and not exposed):
		return false
	hp = maxi(hp - amount, 0)
	hurt_flash = 1.0
	if kind == Kind.BOT:
		stun = maxf(stun, 0.35)
	if hp == 0:
		if kind != Kind.CORE: # the core's hit points run out per phase (Skyship.core_hit)
			alive = false
		if on_break.is_valid():
			on_break.call(self)
		return true
	return false


## Back to life with full hit points (a rebuilt turret, a reused bot slot).
func revive(at: Vector3) -> void:
	alive = true
	hp = max_hp
	anchor = Vector3(at.x, stand, at.z)
	position = anchor
	velocity = Vector3.ZERO
	yanked = false
	bound = 0.0
	stun = 0.0


## Whether it's something the blades should go for (the hud and ShipBots ask).
func targetable() -> bool:
	return alive and (kind == Kind.BOT or exposed)


func update(delta: float) -> void:
	if kind == Kind.TURRET and flash > _seen_flash + 0.01: # a fresh hit
		if not damage(1):
			shielded_hits += 1
	flash = maxf(flash - delta * 4.0, 0.0)
	_seen_flash = flash
	hurt_flash = maxf(hurt_flash - delta * 3.0, 0.0)
	cooldown = maxf(cooldown - delta, 0.0)
	stun = maxf(stun - delta, 0.0)
	squash = maxf(squash - delta * 3.0, 0.0)
	if delta > 0.0:
		var now := position
		motion = Vector3.ZERO if _last_position == Vector3.INF or now.distance_to(_last_position) > 5.0 else (now - _last_position) / delta
		_last_position = now
	match kind:
		Kind.CORE: _update_core()
		Kind.BOT: _update_bot(delta)
		Kind.TURRET: _update_turret()


func _update_core() -> void:
	velocity = Vector3.ZERO
	position = anchor if exposed else HIDDEN
	bound = 0.0


func _update_turret() -> void:
	velocity = Vector3.ZERO
	yanked = false
	bound = 0.0
	position = anchor if alive else HIDDEN


func _update_bot(delta: float) -> void:
	if not alive:
		position = HIDDEN
		velocity = Vector3.ZERO
		return
	if yanked:
		stun = 0.6
		return
	if bound > 0.0:
		bound -= delta
		velocity = Vector3.ZERO
		return
	velocity.y -= GRAVITY * delta
	var grounded := position.y <= stand + 0.02 and velocity.y <= 0.0
	if grounded and stun <= 0.0: # walk: ease the ground velocity toward what it wants
		var flat := Vector3(velocity.x, 0, velocity.z).lerp(walk, 1.0 - exp(-8.0 * delta))
		velocity = Vector3(flat.x, velocity.y, flat.z)
	elif grounded:
		var slow := exp(-FRICTION * delta)
		velocity = Vector3(velocity.x * slow, velocity.y, velocity.z * slow)
	position += velocity * delta
	if position.y <= stand:
		position.y = stand
		velocity.y = 0.0
		thrown = false


## Its look is drawn by whoever built it (ShipBots / ShipTurrets / Skyship): here only the hit glow.
func draw() -> void:
	if glow != null:
		glow.emission_energy_multiplier = 2.0 + flash * 6.0 + highlight * 2.0
	if shell != null:
		shell.emission_enabled = flash > 0.0 or highlight > 0.0
		shell.emission = Color(1, 0.3, 0.2) * flash + Color(1.0, 0.55, 0.15) * highlight * 0.35
