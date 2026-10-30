## The skyship boss: a saucer carrier hovering over the closed arena. It owns its parts and runs
## the fight; each part is its own file:
##
##   ShipTimeline  which attack plays when (data per phase) → the attacks in attacks/
##   ShipShield    the bubble, on and off in a cycle
##   ShipTurrets   the laser turrets under its arms: shoot them while the shield is down
##   ShipBots      the repair bots it drops
##   ShipMissiles  flies and lands every missile
##   core          a ShipTarget under the belly (only reachable while staggered)
##
## The fight, as states:
##   FIGHT    hovering, attacks playing, the shield cycling. Destroy every turret → STAGGER.
##   STAGGER  the attack is cut off, the ship sinks until its belly core is
##            just above the floor, the armour round the core opens: hit it. After
##            `exposed_time` s — or as soon as the core's hit points for this phase are gone —
##            it climbs back up (RISE), the
##            turrets are rebuilt, and the fight goes on (a phase further on if the core took
##            its hits: lower and faster).
##   CRASH    the last phase's core is gone: it tips over, smokes and falls into the arena.
##
## Motion: it hovers on the far side of the arena from you, eased by a spring; attacks may hold it
## still (hold()) or steer it (drift()). While its shield is down it hangs still (so a chain throw
## or a shot at a turret lands where you aimed). It banks into its motion but never spins.
class_name Skyship
extends Node3D

enum State { FIGHT, STAGGER, RISE, CRASH }
const STATE_NAMES := ["fight", "stagger", "rise", "crash"]

const HEIGHTS := [17.0, 14.0, 11.0] ## hover height per phase (it comes lower as it weakens)
const SIZE := 1.8 ## the whole ship is built at 1 and scaled by this
const HULL := 3.6 ## m (before SIZE): the hull's radius, which you can't walk into when it's down
const CORE_DROP := 0.75 ## the core sits this far under the ship's middle (before SIZE)
const LOW := 1.35 ## the core's height while staggered (in blade reach)
const ARMS := 4
const ARM_LENGTH := 4.6
const SPRING := 1.4 ## Hz: how it follows its goal
const CRUISE := 9.0 ## m/s: the fastest its goal moves while it flies (rising included)
const HOVER_RING := 26.0 ## m from the arena's middle it hovers (on the far side from you)
const KEEP_AWAY := 18.0 ## m: never closer than this to you across the floor while it flies
const SHIELD_TIMES := [[4.0, 8.0], [5.0, 7.0], [6.0, 6.0]] ## per phase: shield up, down (s)
const KEEL := 0.45 ## radians it tips over while staggered

var state := State.FIGHT
var phase := 0
var core_hp_per_phase := 8
var exposed_time := 7.0
var hatch := 0.0 ## 0 … 1: the belly hatch's glow (BotDrop)
var timeline := ShipTimeline.new()
var shield := ShipShield.new()
var turrets := ShipTurrets.new()
var bots := ShipBots.new()
var missiles := ShipMissiles.new()
var sparks: OrbSparks
var core: ShipTarget
var defeated := false

var _demo: SkyshipDemo
var _time := 0.0
var _state_time := 0.0
var _velocity := Vector3.ZERO
var _hold := false
var _held_at := Vector3.ZERO
var _drift_goal := Vector3.INF
var _drift_speed := 0.0
var _drift_point := Vector3.ZERO
var _armour: Array[Node3D] = []
var _engines: Array[MeshInstance3D] = []
var _halo: Node3D
var _core_mesh: MeshInstance3D
var _core_material := ShipKit.glow(Color(1.0, 0.3, 0.1), 2.0)
var _hatch_material := ShipKit.glow(ShipKit.RED, 0.0)
var _engine_material := ShipKit.glow(Color(1.0, 0.55, 0.2), 3.0)
var _belly_light: OmniLight3D
var _search: SpotLight3D ## a red searchlight that always finds you
var _smoke := 0.0
var _keel := 0.0
var _hover_angle := PI
var _goal_point := Vector3.ZERO
var _keel_axis := Vector3.RIGHT


func setup(owner: SkyshipDemo, spark_node: OrbSparks) -> void:
	_demo = owner
	sparks = spark_node
	_build()
	add_child(missiles)
	missiles.sparks = sparks
	missiles.on_land = owner.blast
	shield.setup(self)
	turrets.setup(owner, self)
	bots.setup(owner, self)
	timeline.setup(owner, self)


## Start the fight over (the demo's reset has just cleared demo.enemies).
func reset() -> void:
	state = State.FIGHT
	phase = 0
	defeated = false
	_state_time = 0.0
	_velocity = Vector3.ZERO
	_hold = false
	_drift_goal = Vector3.INF
	hatch = 0.0
	missiles.clear()
	bots.clear()
	timeline.restart(0, 2.0)
	core = ShipTarget.new(_demo, ShipTarget.Kind.CORE, Vector3.ZERO, 1.4, core_hp_per_phase)
	_demo.enemies.append(core)
	_apply_phase()
	shield.rebuild()
	turrets.reset()
	_hover_angle = PI
	position = _hover_point()
	_drift_point = position
	_goal_point = position
	rotation = Vector3.ZERO


# --- what the attacks ask -------------------------------------------------------------------------

## Under the ship, on the floor.
func ground_point() -> Vector3:
	return ShipKit.flat(global_position)


## The belly's emitter / hatch (lasers start here, bots drop from here).
func belly() -> Vector3:
	return global_transform * Vector3(0, -CORE_DROP - 0.2, 0)


## The missile pods (on top of each arm's engine).
func pods() -> Array[Vector3]:
	var points: Array[Vector3] = []
	for i in ARMS:
		var angle := TAU * i / ARMS + PI / ARMS
		points.append(global_transform * Vector3(sin(angle) * ARM_LENGTH, 0.5, cos(angle) * ARM_LENGTH))
	return points


## Stay where you are (true) until released (false).
func hold(on: bool) -> void:
	_hold = on
	_held_at = position


## Steer toward `goal` at `speed` m/s (Vector3.INF: back to the hover path).
func drift(goal: Vector3, speed: float) -> void:
	if _drift_goal == Vector3.INF and goal != Vector3.INF:
		_drift_point = position
	_drift_goal = goal
	_drift_speed = speed


## A living laser turret (a random one), null if none are left.
func laser_turret() -> ShipTarget:
	var alive := turrets.living()
	return alive.pick_random() if not alive.is_empty() else null


# The shield's cycle for the phase: [up, down] seconds — shorter windows as it gets angry.
func _apply_phase() -> void:
	shield.up_time = SHIELD_TIMES[phase][0]
	shield.down_time = SHIELD_TIMES[phase][1]


## The last turret was destroyed (ShipTurrets).
func turrets_destroyed() -> void:
	if state != State.FIGHT:
		return
	timeline.stop()
	_hold = false
	_drift_goal = Vector3.INF
	_enter(State.STAGGER)
	_demo.shake = maxf(_demo.shake, 0.5)
	if sparks != null:
		sparks.burst(global_position, Vector3.UP, 60, 9.0, 1.0, 0.8, 0.05, 0.0, Color(0.6, 0.95, 1.0))


## The core took a hit (SkyshipDemo.hit): out of hit points → next phase (or the crash).
func core_hit() -> void:
	if core.hp > 0 or state != State.STAGGER:
		return
	if sparks != null:
		sparks.burst(core.position, Vector3.UP, 50, 8.0, 1.0, 0.7, 0.05, 0.0)
	if phase >= HEIGHTS.size() - 1:
		defeated = true
		_enter(State.CRASH)
		return
	phase += 1
	_enter(State.RISE)


# --- the frame -------------------------------------------------------------------------------------

func tick(delta: float) -> void:
	_time += delta
	_state_time += delta
	match state:
		State.FIGHT:
			timeline.tick(delta)
		State.STAGGER:
			var done := _state_time > exposed_time
			if done:
				_enter(State.RISE)
		State.RISE:
			if _state_time > 2.0:
				_apply_phase()
				shield.rebuild()
				turrets.rebuild()
				timeline.restart(phase, 1.2)
				_enter(State.FIGHT)
	core.exposed = state == State.STAGGER and _state_time > 1.2
	core.anchor = global_transform * Vector3(0, -CORE_DROP, 0)
	_move(delta)
	turrets.tick()
	shield.tick(delta)
	bots.tick(delta)
	missiles.tick(delta)


func _enter(next: State) -> void:
	state = next
	_state_time = 0.0
	if next == State.RISE:
		core.hp = core_hp_per_phase
		core.max_hp = core_hp_per_phase
	if next == State.CRASH:
		timeline.stop()


func _move(delta: float) -> void:
	_steer_hover(delta)
	var goal := _hover_point()
	match state:
		State.STAGGER:
			goal = Vector3(position.x, LOW + CORE_DROP * SIZE, position.z)
			var flat := ShipKit.flat(goal)
			if flat.length() > SkyshipArena.RADIUS - 20.0: # sink well inside the wall
				goal = flat.normalized() * (SkyshipArena.RADIUS - 20.0) + Vector3.UP * goal.y
		State.CRASH:
			goal = Vector3(position.x, 0.6, position.z)
		_:
			if _hold:
				goal = _held_at
			elif _drift_goal != Vector3.INF:
				var target := Vector3(_drift_goal.x, HEIGHTS[phase], _drift_goal.z)
				_drift_point = _drift_point.move_toward(target, _drift_speed * delta)
				goal = _drift_point
			elif state == State.FIGHT and not shield.on: # shield down: it hangs still (your window to hit it)
				goal = _goal_point
	if state == State.FIGHT or state == State.RISE: # the goal itself moves no faster than CRUISE (no lurch when a hold lets go)
		_goal_point = _goal_point.move_toward(goal, CRUISE * delta)
	else:
		_goal_point = goal
	var omega := TAU * (SPRING * (0.5 if state == State.CRASH else 1.0))
	_velocity += ((_goal_point - position) * omega * omega - _velocity * 2.0 * omega) * delta
	position += _velocity * delta
	if state != State.CRASH:
		_drift_point = _drift_point if _drift_goal != Vector3.INF else position
	var bank := (Vector3(_velocity.z, 0, -_velocity.x) * 0.06).limit_length(0.25)
	var turn := Basis.IDENTITY # it doesn't spin: its turrets stay where you aimed
	if state == State.CRASH: # tipping over as it falls
		bank = Vector3(minf(_state_time * 0.25, 0.5), 0, minf(_state_time * 0.15, 0.3))
	transform.basis = Basis.from_euler(bank) * turn
	_keel = lerpf(_keel, KEEL if state == State.STAGGER else 0.0, 1.0 - exp(-3.0 * delta))
	if _keel > 0.001: # staggered: it keels over, its belly turned toward you (so you see the core)
		var toward := ShipKit.flat(_demo.player) - ground_point()
		if toward.length() > 0.1:
			_keel_axis = toward.normalized().cross(Vector3.UP)
		transform.basis = Basis(_keel_axis, _keel) * transform.basis
	transform.basis = transform.basis * Basis.from_scale(Vector3.ONE * SIZE)


## Where it hovers: on a wide ring round the arena's middle, on the far side from you (swaying a
## little round it), never closer than KEEP_AWAY across the floor — it looms over the horizon,
## not over your head.
func _hover_point() -> Vector3:
	var radius := HOVER_RING + sin(_time * 0.2) * 3.0
	var point := Vector3(sin(_hover_angle) * radius, 0.0, cos(_hover_angle) * radius)
	var from_player := point - ShipKit.flat(_demo.player)
	if from_player.length() < KEEP_AWAY:
		point = ShipKit.flat(_demo.player) + (from_player.normalized() if from_player.length() > 0.1 else Vector3.FORWARD) * KEEP_AWAY
		point = SkyshipArena.inside(point, 6.0)
	return point + Vector3.UP * (HEIGHTS[phase] + sin(_time * 1.3) * 0.4)


# The hover ring's angle swings slowly to the side of the arena opposite you.
func _steer_hover(delta: float) -> void:
	var player := ShipKit.flat(_demo.player)
	var want := _hover_angle
	if player.length() > 1.0:
		want = atan2(-player.x, -player.z)
	want += sin(_time * 0.15) * 0.6
	_hover_angle = lerp_angle(_hover_angle, want, 1.0 - exp(-0.35 * delta))


func draw(delta: float) -> void:
	var open := 0.0
	if state == State.STAGGER:
		open = clampf((_state_time - 0.8) / 0.4, 0.0, 1.0)
	for i in _armour.size():
		var angle := TAU * i / _armour.size()
		_armour[i].rotation = Vector3(0, angle, 0)
		(_armour[i].get_child(0) as Node3D).rotation.x = -open * 1.1
	var hot := core.flash
	_core_material.emission_energy_multiplier = 1.5 + open * 3.0 + hot * 8.0 + sin(_time * 6.0) * 0.4 * open
	_core_mesh.scale = Vector3.ONE * (1.0 + hot * 0.15)
	_hatch_material.emission_energy_multiplier = hatch * 5.0
	_hatch_material.albedo_color = ShipKit.RED * (0.2 + hatch)
	_belly_light.light_energy = 1.0 + open * 3.0 + hatch * 2.0
	_belly_light.light_color = Color(1.0, 0.35, 0.15) if open > 0.0 or hatch > 0.0 else Color(1.0, 0.7, 0.4)
	_engine_material.emission_energy_multiplier = 3.0 + sin(_time * 30.0) * 0.4 - (2.5 if state == State.CRASH else 0.0)
	_halo.rotation.y += delta * 0.8
	_search.global_position = belly()
	var aim := _demo.player + Vector3.UP * 0.5
	if aim.distance_to(_search.global_position) > 0.5 and absf((aim - _search.global_position).normalized().y) < 0.999:
		_search.look_at(aim, Vector3.UP)
	_search.light_energy = 0.0 if state == State.CRASH else (6.0 + 3.0 * hatch)
	if state == State.CRASH or (state == State.STAGGER and sparks != null):
		_smoke += delta
		if _smoke > (0.05 if state == State.CRASH else 0.25) and sparks != null:
			_smoke = 0.0
			var at := global_transform * Vector3(randf_range(-2.5, 2.5), 0.2, randf_range(-2.5, 2.5))
			sparks.burst(at, Vector3.UP, 4, 3.0, 0.9, 0.6, 0.05)
	shield.draw()
	turrets.draw()
	bots.draw()
	core.draw()


# The ship, made of primitives: an eight-sided saucer hull with a deck and a glowing bridge ring on
# top, four arms with engines (and missile pods) at their ends, a turning halo, and under the
# belly the core in a ring of armour plates that swing open, a hatch, and a light on the floor.
func _build() -> void:
	var hull_material := ShipKit.metal(Color(0.24, 0.25, 0.28), 0.5)
	var trim := ShipKit.metal(ShipKit.DARK, 0.6)
	ShipKit.add(self, ShipKit.cylinder(2.7, 3.5, 0.9, 8), hull_material)
	ShipKit.add(self, ShipKit.cylinder(3.5, 2.4, 0.4, 8), trim, Vector3.DOWN * 0.65)
	ShipKit.add(self, ShipKit.cylinder(1.5, 2.2, 0.7, 8), hull_material, Vector3.UP * 0.8)
	ShipKit.add(self, ShipKit.sphere(1.05, 20), ShipKit.metal(Color(0.15, 0.17, 0.2), 0.2), Vector3.UP * 1.2).scale = Vector3(1, 0.55, 1)
	var windows := ShipKit.add(self, ShipKit.ring(1.45, 1.6), ShipKit.glow(ShipKit.CYAN, 2.0), Vector3.UP * 1.15)
	windows.scale = Vector3(1, 0.3, 1)
	for i in ARMS:
		var angle := TAU * i / ARMS + PI / ARMS
		var direction := Vector3(sin(angle), 0, cos(angle))
		var arm := ShipKit.add(self, ShipKit.box(Vector3(0.7, 0.35, ARM_LENGTH)), trim, direction * ARM_LENGTH * 0.5)
		arm.rotation.y = angle
		var pod := direction * ARM_LENGTH
		ShipKit.add(self, ShipKit.cylinder(0.55, 0.45, 1.3, 12), hull_material, pod)
		ShipKit.add(self, ShipKit.box(Vector3(0.6, 0.35, 0.6)), trim, pod + Vector3.UP * 0.8)
		_engines.append(ShipKit.add(self, ShipKit.cylinder(0.4, 0.2, 0.25, 12), _engine_material, pod + Vector3.DOWN * 0.75))
		var flame := OmniLight3D.new()
		flame.light_color = Color(1.0, 0.55, 0.2)
		flame.light_energy = 1.5
		flame.omni_range = 4.0
		flame.position = pod + Vector3.DOWN * 1.1
		add_child(flame)
	_halo = Node3D.new()
	add_child(_halo)
	var halo_ring := ShipKit.add(_halo, ShipKit.ring(3.9, 4.05), ShipKit.glow(Color(1.0, 0.5, 0.15), 1.5), Vector3.DOWN * 0.1)
	halo_ring.scale = Vector3(1, 0.4, 1)
	for i in 6:
		var angle := TAU * i / 6.0
		ShipKit.add(_halo, ShipKit.box(Vector3(0.25, 0.25, 0.6)), trim, Vector3(sin(angle), 0, cos(angle)) * 4.0 + Vector3.DOWN * 0.1).rotation.y = angle
	_core_mesh = ShipKit.add(self, ShipKit.sphere(0.6, 20), _core_material, Vector3.DOWN * CORE_DROP)
	for i in 6:
		var hinge := Node3D.new()
		hinge.position = Vector3.DOWN * 0.45
		add_child(hinge)
		var plate := Node3D.new()
		plate.position = Vector3(0, 0, 0.62)
		hinge.add_child(plate)
		ShipKit.add(plate, ShipKit.box(Vector3(0.62, 0.7, 0.12)), trim, Vector3(0, -0.35, 0))
		_armour.append(hinge)
	var hatch_ring := ShipKit.add(self, ShipKit.ring(1.0, 1.25), _hatch_material, Vector3.DOWN * 0.86)
	hatch_ring.scale = Vector3(1, 0.3, 1)
	_search = SpotLight3D.new()
	_search.top_level = true
	_search.light_color = Color(1.0, 0.2, 0.12)
	_search.spot_range = 45.0
	_search.spot_angle = 7.0
	_search.spot_attenuation = 0.5
	_search.shadow_enabled = true
	add_child(_search)
	_belly_light = OmniLight3D.new()
	_belly_light.position = Vector3.DOWN * 1.4
	_belly_light.omni_range = 9.0
	_belly_light.light_energy = 1.0
	add_child(_belly_light)
