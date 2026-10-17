## Blades of Chaos style chains, no physics: every blade move is a path of cubic Beziers for the
## blade tip, played over a fixed time with an easing curve; the chains (BladeChain) lie on the
## blade's travelled path or pull taut — never wobble.
##
##   Throw  (left click)   ThrowMove     Pull   (E)      PullMove      Rush   (Space)  RushMove
##   Sweep  (right click)  SweepMove     Slice  (F)      SliceMove     Whirl  (X)      WhirlMove
##   Spin   (Q)            SpinMove      Bind   (G)      BindMove
##   WASD move · C curves (BladeDebug) · T slow motion · V fire step (BladeFire) · R reset
## The player is a PlayerBody (B switches): SophiaBody — her animation clips underneath, custom IK
## modifiers for the arms and the spine lean on top — or the capsule BladeBody. Both pose from the
## same BodyPose; the chains start at the body's hands.
##
## This node owns the shared state (player, blades, enemies, hit-stop, shake, the slider settings)
## and runs the frame: each blade plays its path and its move (moves/) decides what happens at each
## phase's end. Hits freeze the moves for a few frames (hit-stop) and shake the camera.
class_name BladesDemo
extends Node3D

const LINKS := 48 ## links per blade chain
const HAND_HEIGHT := 1.25
const HAND_SPREAD := 0.35
const PLAYER_SPEED := 4.5
const GRAVITY := 25.0
const HIT_RADIUS := 0.8
const HITSTOP := 0.07 ## seconds of freeze on a hit
const PAIR_GAP := 0.25 ## the two blades land this far either side of the aim
const STAGGER := 0.05 ## the right blade leaves this much later
const PICK_RADIUS := 4.0 ## moves aimed at an enemy take the one within this of the mouse point
const ENEMY_POSITIONS := [Vector3(-3, 0, -5), Vector3(2, 0, -6), Vector3(5, 0, -3), Vector3(-5, 0, -2), Vector3(0, 0, -9)]
const CAMERA_OFFSET := Vector3(0, 7.5, 8.5)
const FIRE := Color(1.0, 0.45, 0.1)

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

# Shared state, read and changed by the moves.
var player := Vector3.ZERO ## the player's feet
var facing := Vector3.FORWARD
var player_velocity := Vector3.ZERO ## only used in the air (after a grapple)
var blades: Array[Blade] = [] ## [left, right]
var enemies: Array[DemoEnemy] = []
var hitstop := 0.0
var shake := 0.0
var game_delta := 0.0 ## this frame's game time step (0 during hit-stop)
var mouse := Vector2.ZERO
var moves := {} ## key → BladeMove
var fire: BladeFire
var debug: BladeDebug
var body: PlayerBody ## the one showing (B switches)
var bodies: Array[PlayerBody] = [] ## [Sophia, capsule]

# Tuned live with the sliders (BladeSliders).
var windup_time := 0.16 ## extra time an attack's OUT curve gets for its wind-up
var top_height := 2.2 ## the wind point (BladeMove.wind_point): this high …
var top_behind := 0.9 ## … this far behind you …
var wind_side := 0.6 ## … and this far out to the blade's own side
var swing_tilt := 40.0 ## degrees each blade's swing plane leans out to its own side

var _renderer: LimbRenderer
var _slow := false


func _ready() -> void:
	DemoProps.add_ground(self)
	DemoProps.add_posts(self, RushMove.POSTS)
	_renderer = LimbRenderer.new()
	add_child(_renderer)
	for each: PlayerBody in [SophiaBody.new(), BladeBody.new()]:
		add_child(each)
		each.setup(self)
		bodies.append(each)
	_use_body(0)
	for move: BladeMove in [ThrowMove.new(self), SweepMove.new(self), SpinMove.new(self), PullMove.new(self),
			SliceMove.new(self), BindMove.new(self), RushMove.new(self), WhirlMove.new(self)]:
		moves[move.key] = move
	fire = BladeFire.new()
	add_child(fire)
	fire.setup($WorldEnvironment.environment)
	debug = BladeDebug.new()
	add_child(debug)
	BladeSliders.build(self, $Hud)
	_reset()


func _reset() -> void:
	player = Vector3.ZERO
	player_velocity = Vector3.ZERO
	for move: BladeMove in moves.values():
		move.reset()
	fire.clear()
	for blade in blades:
		blade.mesh.queue_free()
	for enemy in enemies:
		enemy.mesh.queue_free()
	blades.clear()
	enemies.clear()
	_renderer.clear()
	for side in [-1.0, 1.0]:
		blades.append(_new_blade(side))
	for p: Vector3 in ENEMY_POSITIONS:
		enemies.append(DemoEnemy.new(self, p))
	for each in bodies:
		each.reset()


func _new_blade(side: float) -> Blade:
	var blade := Blade.new()
	blade.side = side
	blade.tip = hand(side)
	blade.previous_tip = blade.tip
	var lengths := PackedFloat32Array()
	lengths.resize(LINKS - 1)
	lengths.fill(0.1)
	blade.chain = LimbChain.new(lengths, blade.tip, LimbChain.Solver.FK)
	blade.chain.style = LimbChain.Style.BLOCK # iron links, not beads
	blade.chain.radius = 0.03
	blade.chain.gap = 0.012
	blade.chain.color = Color(0.22, 0.2, 0.19)
	_renderer.add(blade.chain)
	blade.mesh = DemoProps.mesh_instance(self, DemoProps.blade_mesh(), FIRE, true)
	return blade


func _process(delta: float) -> void:
	_move_player(delta)
	game_delta = delta
	if hitstop > 0.0: # hit-stop: the moves and the enemies freeze, the camera still shakes
		hitstop -= delta
		game_delta = 0.0
	moves.rush.tick(game_delta)
	for blade in blades:
		_update_blade(blade, game_delta)
	body.update(game_delta) # torso lean + arms, from what the blades just did
	moves.bind.tick(game_delta)
	for enemy in enemies:
		enemy.update(game_delta)
	moves.whirl.tick(game_delta)
	_draw(delta)


# --- shared helpers (used by the moves) ---------------------------------------------------------

func right() -> Vector3:
	return facing.cross(Vector3.UP).normalized()


## Where blade paths start and end for this side (a fixed point by the hip: paths stay stable).
func hand(side: float) -> Vector3:
	return player + right() * side * HAND_SPREAD + Vector3(0, HAND_HEIGHT, 0)


## Where the chain actually leaves the body: the shown body's hand (PlayerBody.grip).
func grip(side: float) -> Vector3:
	return body.grip(side)


## Where the mouse points on the plane 1 m above the ground (enemy height).
func mouse_point() -> Vector3:
	var hit_point = Plane(Vector3.UP, 1.0).intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	return hit_point if hit_point != null else player + facing * 5.0 + Vector3(0, 1, 0)


func both_idle() -> bool:
	return blades[0].phase == Blade.IDLE and blades[1].phase == Blade.IDLE


## The enemy within PICK_RADIUS of the mouse point (and `reach` of the player), closest first; -1: none.
func enemy_near_mouse(
		reach: float,
		skip_bound: bool
) -> int:
	var aim := mouse_point()
	var best := -1
	var best_distance := PICK_RADIUS
	for i in enemies.size():
		var enemy := enemies[i]
		var d := enemy.position.distance_to(aim)
		if d < best_distance and enemy.position.distance_to(player) < reach and not (skip_bound and enemy.bound > 0.0):
			best = i
			best_distance = d
	return best


## A blade hits enemy `index`: flash, sparks, maybe knock it away, hit-stop, shake; then the
## blade's move gets its say (a throw sticks, …).
func hit(
		blade: Blade,
		index: int,
		knock: bool
) -> void:
	var move: BladeMove = moves[blade.move]
	var enemy := enemies[index]
	enemy.flash = 1.0
	fire.spark(blade.tip if blade.tip.distance_to(enemy.position) < 2.0 else enemy.position)
	if knock and move.knocks_back() and not enemy.yanked:
		var away := enemy.position - player
		away.y = 0.0
		enemy.velocity = away.normalized() * 7.0 + Vector3.UP * 2.0
	hitstop = HITSTOP * move.hitstop_scale()
	shake = 0.25
	move.after_hit(blade)


# --- the frame ------------------------------------------------------------------------------------

func _move_player(delta: float) -> void:
	if moves.rush.rushing: # the rush path moves the player
		return
	var input := Vector3(float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)), 0,
			float(Input.is_key_pressed(KEY_S)) - float(Input.is_key_pressed(KEY_W)))
	player += input.limit_length(1.0) * PLAYER_SPEED * delta
	if player.y > 0.0 or player_velocity.y > 0.0: # in the air: keep the launch, fall
		player_velocity.y -= GRAVITY * delta
		player += player_velocity * delta
		if player.y <= 0.0:
			player.y = 0.0
			player_velocity = Vector3.ZERO
			shake = maxf(shake, 0.12) # landing thud
	var aim := mouse_point()
	var flat := Vector3(aim.x - player.x, 0, aim.z - player.z)
	if flat.length() > 0.3:
		facing = flat.normalized()


## One blade's frame: play its path (some paths follow the player or the enemy and are rebuilt),
## put the tip on it, let its move react, check hits, and at the phase's end ask the move what next.
func _update_blade(
		blade: Blade,
		delta: float
) -> void:
	blade.previous_tip = blade.tip
	if blade.phase == Blade.IDLE:
		blade.tip = grip(blade.side) + Vector3(0, -0.15, 0)
		return
	if blade.delay > 0.0:
		blade.delay -= delta
		blade.tip = grip(blade.side)
		blade.path[0] = blade.tip
		return
	var move: BladeMove = moves[blade.move]
	blade.time += delta
	if blade.phase == Blade.BACK:
		blade.path[3] = hand(blade.side) # the hand moves while the blade comes home
	else:
		var live := move.live_path(blade)
		if not live.is_empty():
			blade.path = live
	blade.eased = blade.eased_position()
	blade.tip = Bezier.path_point(blade.path, blade.eased)
	if move.after_tip(blade):
		return
	if move.hits_during(blade.phase):
		var phase := blade.phase
		_check_hits(blade)
		if blade.phase != phase: # a hit changed the phase (a throw stuck): don't end it now
			return
	if blade.progress() >= 1.0:
		move.next_phase(blade)


func _check_hits(blade: Blade) -> void:
	for i in enemies.size():
		if i in blade.hit or i == blade.enemy:
			continue
		# the whole stretch the blade covered this frame, so a fast blade can't skip through
		var closest := Geometry3D.get_closest_point_to_segment(enemies[i].position, blade.previous_tip, blade.tip)
		if closest.distance_to(enemies[i].position) < HIT_RADIUS:
			blade.hit.append(i)
			hit(blade, i, true)


# --- drawing ------------------------------------------------------------------------------------

func _draw(delta: float) -> void:
	var taut: Array[PackedVector3Array] = [] # taut chains' control points, for the debug view
	for blade in blades:
		blade.chain.points = _chain_points(blade, delta, taut)
		_aim_blade(blade)
	_renderer.draw()
	moves.bind.draw()
	fire.draw(blades, delta)
	for enemy in enemies:
		enemy.draw()
	shake = maxf(shake - delta, 0.0)
	var jolt := Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * shake * 0.4
	camera.position = camera.position.lerp(player + CAMERA_OFFSET, 1.0 - exp(-5.0 * delta)) + jolt
	camera.look_at(camera.position - CAMERA_OFFSET - jolt + Vector3(0, 0.5, 0))
	debug.draw(self, taut)
	_draw_hud()


## The chain: on the blade's path while it flies out (wind-up and strike) or reels back, else taut.
## `taut_controls` gets each taut chain's control points (for the debug view).
func _chain_points(
		blade: Blade,
		delta: float,
		taut_controls: Array[PackedVector3Array]
) -> PackedVector3Array:
	var from := grip(blade.side)
	if blade.delay <= 0.0 and blade.phase == Blade.OUT and (moves[blade.move] as BladeMove).chain_on_path(blade):
		return BladeChain.on_path(from, blade.path, 0.0, blade.eased, true, LINKS)
	if blade.delay <= 0.0 and blade.phase == Blade.BACK:
		return BladeChain.on_path(from, blade.path, blade.eased, 1.0, false, LINKS)
	var motion := blade.tip - blade.previous_tip
	var speed := motion.length() / maxf(delta, 1e-4)
	var trail := -motion.normalized() * clampf(speed / 25.0, 0.0, 1.0) if speed > 0.01 else Vector3.ZERO
	var c := BladeChain.taut(from, blade.tip, trail)
	taut_controls.append(c)
	return Bezier.even_points(c[0], c[1], c[2], c[3], LINKS)


## The blade continues the chain: it points along the last link, away from the hand.
func _aim_blade(blade: Blade) -> void:
	blade.mesh.position = blade.tip
	var links := blade.chain.points
	var heading := links[-1] - links[-2]
	if heading.length() < 1e-3:
		heading = blade.tip - grip(blade.side)
	if heading.length() > 1e-3 and absf(heading.normalized().dot(Vector3.UP)) < 0.99:
		blade.mesh.look_at(blade.tip + heading, Vector3.UP)


func _draw_hud() -> void:
	var fling: String = moves.pull.fling_name()
	hud.text = "left %s · right %s%s%s%s\nLeft click throw   Right click sweep   Q spin   E pull   F slice   G bind   Space rush   X whirl   WASD move   C curves   T slow motion   V fire (%s)   B body   R reset" % [
		Blade.PHASE_NAMES[blades[0].phase], Blade.PHASE_NAMES[blades[1].phase],
		"   pull: %s" % fling if fling != "" else "", "   [curves]" if debug.enabled else "", "   [slow]" if _slow else "",
		fire.step_name()]


## Show one body and hide the others (only the shown one updates).
func _use_body(index: int) -> void:
	body = bodies[index]
	for each in bodies:
		each.visible = each == body
	body.reset()


# --- input ----------------------------------------------------------------------------------------

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		mouse = event.position
	elif event is InputEventMouseButton and event.pressed:
		mouse = event.position
		if event.button_index == MOUSE_BUTTON_LEFT:
			moves.throw.start(mouse_point())
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			moves.sweep.start()
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_Q: moves.spin.start()
			KEY_E: moves.pull.start()
			KEY_F: moves.slice.start()
			KEY_G: moves.bind.start()
			KEY_SPACE: moves.rush.start()
			KEY_X: moves.whirl.start()
			KEY_V: fire.next_step()
			KEY_C: debug.enabled = not debug.enabled
			KEY_T:
				_slow = not _slow
				Engine.time_scale = 0.2 if _slow else 1.0
			KEY_B: _use_body((bodies.find(body) + 1) % bodies.size())
			KEY_R: _reset()
