## Blades of Chaos style chains, no physics: every blade move is a path of cubic Beziers for the
## blade tip, played over a fixed time with an easing curve; the chains (BladeChain) lie on the
## blade's travelled path or pull taut — never wobble.
##
##   Throw  (left click)   ThrowMove     Pull   (E)      PullMove      Rush   (Space)  RushMove
##   Sweep  (right click)  SweepMove     Slice  (F)      SliceMove     Whirl  (X)      WhirlMove
##   Spin   (Q)            SpinMove      Bind   (G)      BindMove      Slam   (Z)      SlamMove
##   Lasso  (H, H again to throw)  LassoMove      Ground anchor (J)  AnchorMove
##   Rip    (K, K again to tug)    RipMove        — tears a heavy slab (RipProp) out of the ground
##   WASD move · C curves (BladeDebug) · T slow motion · V fire step (BladeFire) · R reset
##   Tab tuning panel (TuningPanel): the wind-up and every move's numbers as sliders, saved to
##       MoveTuning.TUNING_PATH; and a preview (MovePreview): play the picked move on a set target
##       and scrub its timeline. While it's open the camera is an editor camera (OrbitView):
##       right drag / two-finger swipe orbits, middle or shift + right drag pans, wheel / pinch zooms;
##       with the curves on (C) and a preview on, left-drag a curve point to bend it (CurveEditor)
## Combos: a move pressed while the blades are busy is queued (BUFFER seconds) and starts the moment
## they're free — and they're free as soon as they start reeling home (BACK), so the next move
## cuts in from wherever they are on the way back (attacks start from the blade's own position).
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
const PAIR_GAP := 0.25 ## the two blades land this far either side of the aim
const STAGGER := 0.05 ## the right blade leaves this much later
const PICK_RADIUS := 4.0 ## moves aimed at an enemy take the one within this of the mouse point
const ENEMY_POSITIONS := [Vector3(-3, 0, -5), Vector3(2, 0, -6), Vector3(5, 0, -3), Vector3(-5, 0, -2), Vector3(0, 0, -9)]
const CAMERA_OFFSET := Vector3(0, 7.5, 8.5)
const FIRE := Color(1.0, 0.45, 0.1)
const KEYS := {KEY_Q: "spin", KEY_E: "pull", KEY_F: "slice", KEY_G: "bind", KEY_SPACE: "rush",
		KEY_X: "whirl", KEY_Z: "slam", KEY_J: "anchor", KEY_K: "rip", KEY_H: "lasso"} ## key → move (press)
const SNAP_TIME := 0.3 ## seconds a chain takes to go from its thrown shape to taut (or back)
const SNAP_LAG := 0.6 ## … the straightening runs from the hand out to the blade (a whip)
const STEP_DRAW := 1.0 / 60.0 ## the draw step while a preview plays (MovePreview.STEP)
const BUFFER := 1.0 ## seconds a move pressed while the blades are busy waits for them (a throw is ~0.6 s out)

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

# Shared state, read and changed by the moves.
var player := Vector3.ZERO ## the player's feet
var facing := Vector3.FORWARD
var player_velocity := Vector3.ZERO ## only used in the air (after a grapple)
var blades: Array[Blade] = [] ## [left, right]
var enemies: Array[DemoEnemy] = []
var props: Array[RipProp] = [] ## heavy slabs stuck in the ground (RipMove tears them out)
var hitstop := 0.0
var shake := 0.0
var hit_count := 0 ## hits so far (the preview's timeline marks them)
var game_delta := 0.0 ## this frame's game time step (0 during hit-stop)
var mouse := Vector2.ZERO
var moves := {} ## key → BladeMove
var fire: BladeFire
var debug: BladeDebug
var body: PlayerBody ## the one showing (B switches)
var bodies: Array[PlayerBody] = [] ## [Sophia, capsule]

# Tuned live on the tuning panel's "wind-up" page (Tab).
var windup_time := 0.16 ## extra time an attack's OUT curve gets for its wind-up
var top_height := 2.2 ## the wind point (BladeMove.wind_point): this high …
var top_behind := 0.9 ## … this far behind you …
var wind_side := 0.6 ## … and this far out to the blade's own side
var swing_tilt := 40.0 ## degrees each blade's swing plane leans out to its own side
var strike_handle := 0.4 ## the attack curve's handles at the wind point, as a share of each piece's length (rounder ↔ sharper)
var windup_bow := 1.4 ## how far the hand → wind-point piece bows out (× its straight length)
var drop := 0.25 ## a throw comes down onto its target from this high (× the throw's length)
var throw_time := 0.15 ## a throw's strike takes this long … (every move that throws both blades out)
var throw_per_m := 0.008 ## … plus this per metre to the target
var hitstop_time := 0.07 ## seconds the moves freeze on a hit (× the move's hitstop_scale)
var hit_shake := 0.25 ## camera shake on a hit
# The chains' weight (ChainSway): how far the middle sags (× the chain's length) and how stiff its
# springs are, when slack (blade home), under load (taut: holding, yanking, swinging round you) and
# while it lies on a flying blade's path; damping (low: it bounces); how much of the lag shows on a path.
var chain_sag := 0.12
var chain_sag_load := 0.03
var chain_hz := 2.5
var chain_hz_load := 5.0
var chain_hz_path := 9.0
var chain_damping := 0.3
var chain_path_sway := 0.3

var tuning: MoveTuning
var preview: MovePreview
var curves := CurveEditor.new(self) ## drag curve points (panel open, preview on, curves shown)
var orbit := OrbitView.new() ## the editor camera, used while the tuning panel is open
var aim_override := Vector3.INF ## when set (a preview), mouse_point() returns this instead of the mouse
var _renderer: LimbRenderer
var _panel: TuningPanel
var _queued := Callable() ## the move pressed while the blades were busy (empty: none)
var _queued_name := ""
var _queued_for := 0.0 ## seconds it still waits
var _slow := false
var _game_view := OrbitView.new() ## the game's camera (never changed)
var _eye := Vector3.ZERO ## where the camera is easing to / looking at
var _focus := Vector3.ZERO
var _jolt := RandomNumberGenerator.new() ## camera shake: its own random numbers, so a preview's moves replay the same


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
			SliceMove.new(self), BindMove.new(self), RushMove.new(self), WhirlMove.new(self), SlamMove.new(self), LassoMove.new(self), AnchorMove.new(self), RipMove.new(self)]:
		moves[move.key] = move
	fire = BladeFire.new()
	add_child(fire)
	fire.setup($WorldEnvironment.environment)
	debug = BladeDebug.new()
	add_child(debug)
	tuning = MoveTuning.new(self) # the code's values become the defaults …
	tuning.load_saved() # … then the saved ones (if any) replace them
	Blade.shaper = shape_path
	Blade.timing = tuning.timing
	preview = MovePreview.new(self)
	_panel = TuningPanel.new()
	$Hud.add_child(_panel)
	_panel.setup(tuning, preview, orbit.reset)
	_eye = _game_view.eye(player)
	_focus = _game_view.focus(player)
	_reset()


## The shared settings: the tuning panel's "wind-up" page (MoveTuning section "shared").
func tunables() -> Array:
	return [
		["windup_time", 0.04, 0.6, 0.01],
		["top_height", 1.4, 4.5, 0.05],
		["top_behind", 0.0, 3.0, 0.05],
		["wind_side", 0.0, 2.0, 0.05],
		["swing_tilt", 0.0, 80.0, 1.0],
		["strike_handle", 0.05, 1.0, 0.01],
		["windup_bow", 1.0, 3.0, 0.05],
		["drop", 0.0, 1.0, 0.01],
		["throw_time", 0.05, 0.6, 0.01],
		["throw_per_m", 0.0, 0.05, 0.001],
		["hitstop_time", 0.0, 0.3, 0.01],
		["hit_shake", 0.0, 1.0, 0.01],
		["chain_sag", 0.0, 0.4, 0.01],
		["chain_sag_load", 0.0, 0.2, 0.005],
		["chain_hz", 0.5, 8.0, 0.1],
		["chain_hz_load", 1.0, 15.0, 0.1],
		["chain_hz_path", 2.0, 25.0, 0.5],
		["chain_damping", 0.05, 1.0, 0.01],
		["chain_path_sway", 0.0, 1.0, 0.01],
	]


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
	for slab in props:
		slab.mesh.queue_free()
	blades.clear()
	enemies.clear()
	props.clear()
	_renderer.clear()
	for side in [-1.0, 1.0]:
		blades.append(_new_blade(side))
	for p: Vector3 in ENEMY_POSITIONS:
		enemies.append(DemoEnemy.new(self, p))
	for p: Vector3 in RipMove.PROPS:
		props.append(RipProp.new(self, p))
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


## A frame: the game steps once by `delta` — or, while a preview plays, by as many fixed steps as
## it asks for (none while it's paused or scrubbed) — then everything is drawn.
func _process(delta: float) -> void:
	if not preview.active:
		simulate(delta)
		_draw(delta, delta)
		return
	var steps := preview.steps(delta)
	_draw(STEP_DRAW if steps > 0 else 0.0, delta) # the camera keeps moving even while paused


## One game step: the player, hit-stop, queued moves, the moves, the blades, the body, the enemies.
func simulate(delta: float) -> void:
	_move_player(delta)
	game_delta = delta
	if hitstop > 0.0: # hit-stop: the moves and the enemies freeze, the camera still shakes
		hitstop -= delta
		game_delta = 0.0
	_run_queued(delta)
	moves.rush.tick(game_delta)
	moves.anchor.tick(game_delta)
	moves.rip.tick(game_delta)
	for blade in blades:
		_update_blade(blade, game_delta)
	body.update(game_delta) # torso lean + arms, from what the blades just did
	moves.bind.tick(game_delta)
	for enemy in enemies:
		enemy.update(game_delta)
	moves.whirl.tick(game_delta)
	moves.lasso.tick(game_delta)


# --- shared helpers (used by the moves) ---------------------------------------------------------

func right() -> Vector3:
	return facing.cross(Vector3.UP).normalized()


## Where blade paths start and end for this side (a fixed point by the hip: paths stay stable).
func hand(side: float) -> Vector3:
	return player + right() * side * HAND_SPREAD + Vector3(0, HAND_HEIGHT, 0)


## Where the chain actually leaves the body: the shown body's hand (PlayerBody.grip).
func grip(side: float) -> Vector3:
	return body.grip(side)


## Where an idle blade hangs: just under the grip. Attacks start from here and reels end here, so
## a blade never jumps between hanging and a path.
func rest(side: float) -> Vector3:
	if preview.active: # the fixed anchor: the body's animated grip would make a replay differ
		return hand(side) + Vector3(0, -0.15, 0)
	return grip(side) + Vector3(0, -0.15, 0)


## Where the mouse points on the plane 1 m above the ground (enemy height).
func mouse_point() -> Vector3:
	if aim_override != Vector3.INF:
		return aim_override
	var hit_point = Plane(Vector3.UP, 1.0).intersects_ray(camera.project_ray_origin(mouse), camera.project_ray_normal(mouse))
	return hit_point if hit_point != null else player + facing * 5.0 + Vector3(0, 1, 0)


## Whether a new move may start: every blade is idle or on its last reel home (BACK, and its move
## says that reel ends it), and no rush is carrying the player.
func blades_free() -> bool:
	if moves.rush.rushing:
		return false
	for blade in blades:
		if blade.phase == Blade.IDLE:
			continue
		if blade.phase != Blade.BACK or not (moves[blade.move] as BladeMove).done_after_back(blade):
			return false
	return true


## Start `action` (a move's start) now if the blades are free, else queue it: it starts the moment
## they are, unless BUFFER seconds pass first. A newer press replaces the queued one.
func request(
		move_name: String,
		action: Callable
) -> void:
	if blades_free():
		action.call()
		return
	_queued = action
	_queued_name = move_name
	_queued_for = BUFFER


## What a move's key (or mouse button) does: start it — queued if the blades are busy — or, for a
## move that's waiting for its second press (lasso: throw, rip: tug), do that right now. The
## preview presses moves through this too.
func press(action: String) -> void:
	match action:
		"throw": request("throw", moves.throw.start.bind(mouse_point())) # aimed where you clicked
		"bind": moves.bind.start() # its own chains: never waits for the blades
		"rip":
			if moves.rip.hooked: # hooked: K tugs, right now
				moves.rip.start()
			else:
				request("rip", moves.rip.start)
		"lasso":
			if moves.lasso.held >= 0: # swinging: H throws, right now
				moves.lasso.start()
			else:
				request("lasso", moves.lasso.start)
		_: request(action, moves[action].start)


# The queued move: start it once the blades are free, or drop it when its time runs out.
func _run_queued(delta: float) -> void:
	if not _queued.is_valid():
		return
	_queued_for -= delta
	if blades_free():
		var action := _queued
		_queued = Callable()
		_queued_name = ""
		action.call()
	elif _queued_for <= 0.0:
		_queued = Callable()
		_queued_name = ""


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
	hit_count += 1
	hitstop = hitstop_time * move.hitstop_scale()
	shake = hit_shake
	move.after_hit(blade)


# --- the frame ------------------------------------------------------------------------------------

func _move_player(delta: float) -> void:
	if moves.rush.rushing or moves.anchor.hauling: # a rush / the anchor's haul moves the player
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
		blade.tip = rest(blade.side)
		return
	if blade.delay > 0.0: # waiting to leave: drift to the rest point (a blade cut off mid-reel eases in)
		blade.delay -= delta
		blade.tip = blade.tip.lerp(rest(blade.side), 1.0 - exp(-20.0 * delta))
		blade.path[0] = blade.tip
		return
	var move: BladeMove = moves[blade.move]
	blade.time += delta
	if blade.phase == Blade.BACK:
		blade.path[3] = rest(blade.side) # the hand moves while the blade comes home
	else:
		var live := move.live_path(blade)
		if not live.is_empty():
			blade.base_path = live
			blade.path = shape_path(blade, live)
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

## Draw the frame: `delta` is the game time that passed (0 while a preview is paused), `view_delta`
## the real time (the camera follows on real time).
func _draw(
		delta: float,
		view_delta: float
) -> void:
	var taut: Array[PackedVector3Array] = [] # taut chains' control points, for the debug view
	for blade in blades:
		blade.chain.points = _chain_points(blade, delta, taut)
		_aim_blade(blade)
	_renderer.draw()
	moves.bind.draw()
	fire.draw(blades, delta)
	for enemy in enemies:
		enemy.draw()
	for slab in props:
		slab.draw()
	shake = maxf(shake - delta, 0.0)
	var jolt := Vector3(_jolt.randf_range(-1, 1), _jolt.randf_range(-1, 1), 0) * shake * 0.4
	if preview.active: # a steady view while previewing
		jolt = Vector3.ZERO
	var editing := _panel.visible
	var view := orbit if editing else _game_view
	var follow := 1.0 - exp(-(12.0 if editing else 5.0) * view_delta) # the editor camera answers faster
	_eye = _eye.lerp(view.eye(player), follow)
	_focus = _focus.lerp(view.focus(player), follow)
	camera.position = _eye + jolt
	camera.h_offset = lerpf(camera.h_offset, _panel_shift(), 1.0 - exp(-6.0 * view_delta))
	camera.look_at(_focus)
	debug.draw(self, taut)
	_draw_hud()


## The chain: on the blade's path while it flies out (wind-up and strike) or reels back, else taut.
## When it switches (the blade bites: thrown shape → taut; the reel starts: taut → on the path) it
## doesn't jump: for SNAP_TIME it blends from the shape it had, link by link, starting at the hand
## and running out to the blade, with a little overshoot — the crack of a heavy chain going tight.
func _chain_points(
		blade: Blade,
		delta: float,
		taut_controls: Array[PackedVector3Array]
) -> PackedVector3Array:
	var on_path := _lies_on_path(blade)
	var points := _chain_shape(blade, delta, taut_controls, on_path)
	if on_path != blade.chain_on_path and blade.chain.points.size() == points.size():
		blade.snap_from = blade.chain.points.duplicate()
		blade.snap_time = 0.0
	blade.chain_on_path = on_path
	if blade.phase == Blade.IDLE:
		blade.snap_time = SNAP_TIME # home: no blend (one cut short would leave the chain hanging out)
	if blade.snap_time < SNAP_TIME and blade.snap_from.size() == points.size():
		blade.snap_time += delta
		var from := blade.snap_from
		if on_path and blade.phase == Blade.BACK: # reeling in: blend from the taut chain to the blade
			var unused: Array[PackedVector3Array] = [] # now, not the one it had out at the target
			from = _chain_shape(blade, delta, unused, false)
		points = _snap(from, points, blade.snap_time / SNAP_TIME)
	return _weigh(blade, points, on_path, delta)


# The chain's weight (ChainSway) for its state: slack at home, under load when taut, light on a path.
func _weigh(
		blade: Blade,
		points: PackedVector3Array,
		on_path: bool,
		delta: float
) -> PackedVector3Array:
	var length := ChainSway.length_of(points)
	if on_path:
		return blade.sway.apply(points, 0.0, chain_hz_path, chain_damping, chain_path_sway, delta)
	if blade.phase == Blade.IDLE or blade.delay > 0.0:
		return blade.sway.apply(points, chain_sag * length, chain_hz, chain_damping, 1.0, delta)
	return blade.sway.apply(points, chain_sag_load * length, chain_hz_load, chain_damping, 1.0, delta)


# Whether the chain lies on the blade's path now (flying out, or reeling back), else it's taut.
func _lies_on_path(blade: Blade) -> bool:
	if blade.delay > 0.0:
		return false
	if blade.phase == Blade.BACK:
		return true
	return blade.phase == Blade.OUT and (moves[blade.move] as BladeMove).chain_on_path(blade)


# `from` → `to` link by link at time t (0..1): the hand's end first, the blade's end last, each link
# easing in and out (smoothstep) with a small overshoot that settles, so no link jumps in one frame.
static func _snap(
		from: PackedVector3Array,
		to: PackedVector3Array,
		t: float
) -> PackedVector3Array:
	var out := to.duplicate()
	var last := maxf(to.size() - 1, 1)
	for k in to.size():
		var x := clampf(t * (1.0 + SNAP_LAG) - k / last * SNAP_LAG, 0.0, 1.0)
		var w := x * x * (3.0 - 2.0 * x) + 0.12 * sin(PI * x) * x # smooth, a touch past its place near the end
		out[k] = from[k].lerp(to[k], w)
	return out


# The chain's shape for its mode: on the path, or taut from the hand to the tip.
## `taut_controls` gets each taut chain's control points (for the debug view).
func _chain_shape(
		blade: Blade,
		delta: float,
		taut_controls: Array[PackedVector3Array],
		on_path: bool
) -> PackedVector3Array:
	var from := grip(blade.side)
	if on_path and blade.phase == Blade.OUT:
		return BladeChain.on_path(from, blade.path, 0.0, blade.eased, true, LINKS)
	if on_path:
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
	var fling: String = moves.slam.fling_name() if moves.slam.left > 0 else moves.pull.fling_name()
	hud.text = "left %s · right %s%s%s%s%s\nLeft click throw   Right click sweep   Q spin   E pull   F slice   G bind   Space rush   X whirl   Z slam   H lasso (H: throw)   J ground anchor   K rip (K: tug)   Tab tuning   WASD move   C curves   T slow motion   V fire (%s)   B body   R reset" % [
		Blade.PHASE_NAMES[blades[0].phase], Blade.PHASE_NAMES[blades[1].phase],
		"   next: %s" % _queued_name if _queued_name != "" else "",
		"   fling: %s" % fling if fling != "" else "", "   [curves]" if debug.enabled else "", "   [slow]" if _slow else "",
		fire.step_name()]


## Whether the tuning panel (the editor) is open.
func is_editing() -> bool:
	return _panel != null and _panel.visible


## A path as a blade will play it: its move's path plus the curve editor's offsets.
func shape_path(
		blade: Blade,
		path: PackedVector3Array
) -> PackedVector3Array:
	return curves.shape(blade, path)


# With the tuning panel open (it covers the right side), slide the view right by half the panel's
# width so the player sits in the middle of what's still visible.
func _panel_shift() -> float:
	if _panel == null or not _panel.visible:
		return 0.0
	var screen := get_viewport().get_visible_rect().size
	var tall := 2.0 * orbit.distance * tan(deg_to_rad(camera.fov) * 0.5) # metres seen top to bottom at the player
	return (_panel.size.x + 16.0) * 0.5 / screen.x * tall * screen.x / screen.y


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
	if curves.handle(event): # dragging a curve point
		return
	if _panel.visible and orbit.handle(event, camera): # the editor camera's mouse / trackpad
		return
	if preview.active and (event is InputEventMouseButton or (event is InputEventKey and KEYS.has(event.keycode))):
		return # the preview owns the scene: no moves of your own until ■ Stop
	if event is InputEventMouseButton and event.pressed:
		mouse = event.position
		if event.button_index == MOUSE_BUTTON_LEFT:
			press("throw")
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			press("sweep")
	elif event is InputEventKey and event.pressed and not event.echo:
		var action: String = KEYS.get(event.keycode, "")
		if action != "":
			press(action)
			return
		match event.keycode:
			KEY_V: fire.next_step()
			KEY_C: debug.enabled = not debug.enabled
			KEY_TAB: _panel.visible = not _panel.visible
			KEY_T:
				_slow = not _slow
				Engine.time_scale = 0.2 if _slow else 1.0
			KEY_B: _use_body((bodies.find(body) + 1) % bodies.size())
			KEY_R: _reset()
