## OLD: the single-file demo before the split (kept until the split version is checked). Not used by any scene.
## Blades of Chaos style chains, no physics: every blade move is a path of cubic Beziers for the blade
## tip, played over a fixed time with an easing curve. Both blades move together (mirrored, a few
## frames apart). The chains (BladeChain) lie on the blade's travelled path or pull taut — never wobble.
##
##   Throw  (left click)   both blades whip out wide on their own sides and converge on the mouse,
##                         stick for a beat, reel back along a new curve.
##   Sweep  (right click)  each blade starts on its own side and sweeps across the front: they cross.
##   Spin   (Q)            both blades orbit you, opposite each other, 3 turns (hits once per turn).
##   Pull   (E)            both blades hook the enemy nearest the mouse, hit-stop, then YANK it
##                         along a lifted arc (ease-in: slow, then violent) to a random spot: your
##                         feet, flung off to your left or right, or over your head to land behind.
##   Slice  (F)            a combo of slashes on the enemy nearest the mouse, blades taking turns.
##                         Each slash: a random plane through the line player → enemy (spun about
##                         that line), and a loop in that plane that cuts through the enemy.
##   Bind   (G)            chains burst out of the ground round the enemy nearest the mouse and
##                         coil round it, holding it still, then sink back (ChainBind). Each
##                         chain's path = a list of points (anchor, rise, a helix round the enemy)
##                         made smooth with Bezier.from_points — any point list works the same way.
##   Rush   (Space)        grapple / rush: both blades hook whatever is nearest the mouse, then YOU
##                         travel a lifted arc to it (ease-in), chains reeling in taut. An enemy:
##                         you land in front of it with a slam. A post: you fly up to its ring,
##                         keep your momentum and fall.
##   Whirl  (X)            enemy as a weapon: hook the enemy nearest the mouse, swing it round you
##                         on the chains (a lead-in curve, then quarter-circle Beziers, faster and
##                         faster) knocking down anyone it hits, and let go when the swing points
##                         at the mouse: it flies off as a projectile.
##   WASD move · C show curves · T slow motion · V fire trail step (1–4) · R reset
## Fire: each blade drags a FireTrail (fire_trail.gdshader, built up in 4 steps), sheds embers
## while it moves fast, and every hit throws a burst of sparks.
## Every attack path starts with a wind-up segment: the heavy blade first rises from the hand up
## behind the shoulder, slowing into the top (the hang = anticipation), then the strike segment
## whips it forward and down to the target. Both are cubic Beziers joined with matching tangents
## (the handles either side of the join lie on one line), so it is one smooth swing.
## In a slice combo each blade reels its chain fully home after every slash, then winds up again.
## Hits freeze the moves for a few frames (hit-stop) and shake the camera.
extends Node3D

const LINKS := 48
const HAND_HEIGHT := 1.25
const HAND_SPREAD := 0.35
const PLAYER_SPEED := 4.5
const HIT_RADIUS := 0.8
const HITSTOP := 0.07 ## seconds of freeze on a hit
const PULL_RANGE := 12.0
const PAIR_GAP := 0.25 ## the two blades land this far either side of the aim
const STAGGER := 0.05 ## the right blade leaves this much later
const SWEEP_RADIUS := 4.5
const SPIN_RADIUS := 3.8
const SPIN_TURNS := 3
const SPIN_TIME := 1.05
const CIRCLE_K := 0.5523 ## a quarter circle as a cubic Bezier: handles this × radius long
const SLICES := 6 ## slashes per slice combo (3 per blade)
const SLICE_TIME := 0.17
const SLICE_SIZE := 1.6 ## how far either side of the enemy a slash starts and ends
const SLICE_RANGE := 9.0
const SLICE_TILT_MIN := 15.0 ## a slash plane leans this many degrees (or more) toward its blade's side …
const SLICE_TILT_MAX := 65.0 ## … and at most this many
const BIND_CHAINS := 5
const BIND_RANGE := 12.0
const BIND_SPREAD := 2.4 ## the chains come out of the ground this far from the enemy
const COIL_RADIUS := 0.6 ## and coil round it this tight (the enemy is 0.4 wide)
const COIL_TURNS := 1.25
const BIND_LINKS := 60
const POSTS := [Vector3(-7, 4.5, -9), Vector3(7, 5.5, -11), Vector3(0, 6.5, -16)] ## grapple rings (tops)
const RUSH_PICK_PIXELS := 90.0 ## the rush target: the enemy or ring nearest the mouse on screen, this close
const RUSH_STOP := 1.3 ## an enemy rush ends this far in front of it
const GRAVITY := 25.0
const TRAIL_SPAN := 0.9 ## the trail spans this much of the blade, back from its tip
const EMBER_SPEED := 6.0 ## blades faster than this (m/s) shed embers
const WHIRL_RADIUS := 3.6
const WHIRL_TURNS := 3
const WHIRL_TIME := 1.5
const WHIRL_MIN_TURNS := 1.5 ## release no earlier than this many turns …
const WHIRL_AIM := 0.96 ## … when the swing direction · the aim direction is above this
const THROW_SPEED := 18.0
const BODY_HIT := 1.2 ## a swung / thrown enemy knocks over others this close
const FIRE_STEPS := ["fade", "colour by age", "flicker", "cartoon"]
const ENEMY_POSITIONS := [Vector3(-3, 0, -5), Vector3(2, 0, -6), Vector3(5, 0, -3), Vector3(-5, 0, -2), Vector3(0, 0, -9)]
const CAMERA_OFFSET := Vector3(0, 7.5, 8.5)
const FIRE := Color(1.0, 0.45, 0.1)

# A blade's phases. Each plays a path (one or more cubic Beziers, 4 points each) over a duration.
const IDLE := 0
const OUT := 1 ## hand → aim (throw / pull) or → sweep / spin start
const HOLD := 2 ## stuck in the target
const SWEEP := 3
const SPIN := 4
const YANK := 5 ## enemy (and blades) along a lifted arc to the player
const BACK := 6 ## → hand
const SLICE := 7 ## one slash of a slice combo
const WHIRL := 8 ## swinging a hooked enemy round the player
const PHASE_NAMES := ["idle", "out", "hold", "sweep", "spin", "yank", "back", "slice", "whirl"]


class Blade:
	var side := 1.0 ## +1 right hand, −1 left hand
	var phase := 0
	var move := "" ## "throw", "sweep", "spin", "pull"
	var delay := 0.0 ## waits this long before the phase starts playing
	var time := 0.0
	var duration := 0.0
	var segment_times := PackedFloat32Array() ## one time per path segment (or one for the whole path)
	var segment_easings := [] ## an easing name per segment
	var eased := 0.0 ## where along the path the tip is (0..1, over all segments)
	var path := PackedVector3Array() ## 4 control points per Bezier segment
	var tip := Vector3.ZERO
	var previous_tip := Vector3.ZERO
	var enemy := -1 ## pull: the hooked enemy
	var hit: Array[int] = [] ## enemies already hit (this move, or this spin turn)
	var turn := 0
	var spin_right := Vector3.RIGHT ## spin: the circle's frame, fixed when the spin starts
	var spin_forward := Vector3.FORWARD
	var slice_angles: Array[float] = [] ## slice: this blade's slash planes, still to do (radians about player → enemy)
	var chain: LimbChain
	var mesh: MeshInstance3D


class Enemy:
	var position := Vector3.ZERO
	var velocity := Vector3.ZERO
	var flash := 0.0
	var yanked := false
	var bound := 0.0 ## seconds left held by Chain Bind (can't move)
	var thrown := false ## flying after a Whirl: knocks over others until it lands
	var cooldown := 0.0 ## can't be body-hit again for this long
	var mesh: MeshInstance3D
	var material: StandardMaterial3D


@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var _player := Vector3.ZERO
var _facing := Vector3.FORWARD
var _player_mesh: MeshInstance3D
var _blades: Array[Blade] = [] ## [left, right]
var _enemies: Array[Enemy] = []
var _renderer: LimbRenderer
var _hitstop := 0.0
var _shake := 0.0
var _show_curves := false
var _slow := false
var _debug: MeshInstance3D
var _debug_mesh := ImmediateMesh.new()
var _mouse := Vector2.ZERO
# Tuned live with the sliders (bottom left).
var _windup_time := 0.16 ## the wind-up segment: hand → top
var _top_height := 2.7 ## the wind-up's top: this high …
var _top_behind := 0.9 ## … this far behind the hand
var _swing_tilt := 40.0 ## degrees each blade's swing plane leans out to its own side
var _labels: Array[Label3D] = [] ## debug: control point ids
var _binds: Array[ChainBind] = [] ## Chain Bind: the chains out right now
var _bind_renderer: LimbRenderer
var _bind_enemy := -1
var _bind_gripped := false
var _player_velocity := Vector3.ZERO ## only used in the air (after a grapple) and for landing
var _rushing := false
var _rush_path := PackedVector3Array() ## the player's feet: 4 control points
var _rush_time := 0.0
var _rush_duration := 0.0
var _rush_enemy := -1 ## -1: a post
var _rush_target := Vector3.ZERO ## what the blades hooked
var _trails: Array[FireTrail] = [] ## one per blade
var _embers: Array[CPUParticles3D] = [] ## one per blade
var _sparks: Array[CPUParticles3D] = [] ## hit bursts, reused round robin
var _next_spark := 0
var _fire_material: ShaderMaterial
var _whirl_enemy := -1
var _whirl_angle := 0.0 ## where on the circle the swing starts
var _whirl_from := Vector3.ZERO ## where the enemy was when hooked (the lead-in's start)
var _whirl_right := Vector3.RIGHT ## the circle's frame, fixed when the swing starts
var _whirl_forward := Vector3.FORWARD
var _fling := {} ## pull: where the yanked enemy goes {name, to, lift, duration, velocity}, picked once per pull
var _slice_enemy := -1


func _ready() -> void:
	_add_ground()
	_renderer = LimbRenderer.new()
	add_child(_renderer)
	_bind_renderer = LimbRenderer.new() # its own, so bind chains come and go without the blades
	add_child(_bind_renderer)
	_add_posts()
	_add_fire()
	_player_mesh = _mesh_instance(_capsule(0.3, 1.6), Color(0.85, 0.85, 0.8))
	_debug = MeshInstance3D.new()
	_debug.mesh = _debug_mesh
	var line_material := StandardMaterial3D.new()
	line_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	line_material.vertex_color_use_as_albedo = true
	line_material.no_depth_test = true
	_debug.material_override = line_material
	add_child(_debug)
	_add_sliders()
	_reset()


func _reset() -> void:
	_player = Vector3.ZERO
	_player_velocity = Vector3.ZERO
	_rushing = false
	for blade in _blades:
		blade.mesh.queue_free()
	for enemy in _enemies:
		enemy.mesh.queue_free()
	_blades.clear()
	_enemies.clear()
	_renderer.clear()
	_binds.clear()
	_bind_renderer.clear()
	_bind_enemy = -1
	for trail in _trails:
		trail.clear_trail()
	for side in [-1.0, 1.0]:
		var blade := Blade.new()
		blade.side = side
		blade.tip = _hand(side)
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
		blade.mesh = _mesh_instance(_blade_mesh(), FIRE, true)
		_blades.append(blade)
	for p in ENEMY_POSITIONS:
		var enemy := Enemy.new()
		enemy.position = p + Vector3(0, 1.0, 0)
		enemy.material = StandardMaterial3D.new()
		enemy.material.albedo_color = Color(0.45, 0.55, 0.75)
		enemy.mesh = MeshInstance3D.new()
		enemy.mesh.mesh = _capsule(0.4, 2.0)
		enemy.mesh.material_override = enemy.material
		add_child(enemy.mesh)
		_enemies.append(enemy)


func _process(delta: float) -> void:
	_move_player(delta)
	var game_delta := delta
	if _hitstop > 0.0: # hit-stop: the moves and the enemies freeze, the camera still shakes
		_hitstop -= delta
		game_delta = 0.0
	_update_rush(game_delta)
	for blade in _blades:
		_update_blade(blade, game_delta)
	_update_binds(game_delta)
	_update_enemies(game_delta)
	_draw(delta)


func _right() -> Vector3:
	return _facing.cross(Vector3.UP).normalized()


func _hand(side: float) -> Vector3:
	return _player + _right() * side * HAND_SPREAD + Vector3(0, HAND_HEIGHT, 0)


func _move_player(delta: float) -> void:
	if _rushing: # the rush path moves the player (see _update_rush)
		return
	var input := Vector3(float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)), 0,
			float(Input.is_key_pressed(KEY_S)) - float(Input.is_key_pressed(KEY_W)))
	_player += input.limit_length(1.0) * PLAYER_SPEED * delta
	if _player.y > 0.0 or _player_velocity.y > 0.0: # in the air: keep the launch, fall
		_player_velocity.y -= GRAVITY * delta
		_player += _player_velocity * delta
		if _player.y <= 0.0:
			_player.y = 0.0
			_player_velocity = Vector3.ZERO
			_shake = maxf(_shake, 0.12) # landing thud
	var aim := _mouse_point()
	var flat := Vector3(aim.x - _player.x, 0, aim.z - _player.z)
	if flat.length() > 0.3:
		_facing = flat.normalized()


func _mouse_point() -> Vector3:
	var hit = Plane(Vector3.UP, 1.0).intersects_ray(camera.project_ray_origin(_mouse), camera.project_ray_normal(_mouse))
	return hit if hit != null else _player + _facing * 5.0 + Vector3(0, 1, 0)


# --- paths: a list of cubic Beziers, played as one ----------------------------------------------

static func _ease(
		t: float,
		easing: String
) -> float:
	match easing:
		"out": return 1.0 - pow(1.0 - t, 3.0) # fast start, soft landing: the throw
		"in": return t * t * t # slow start, violent end: the yank
		"accelerate": return t * t # winds up faster and faster: the whirl
		"smooth": return t * t * (3.0 - 2.0 * t)
	return t


# Play `path` over `duration` with one easing for the whole of it.
func _start(
		blade: Blade,
		phase: int,
		path: PackedVector3Array,
		duration: float,
		easing: String
) -> void:
	_start_segments(blade, phase, path, PackedFloat32Array([duration]), [easing])


# Play `path` with its own time and easing per segment (wind-up slow, strike fast …).
func _start_segments(
		blade: Blade,
		phase: int,
		path: PackedVector3Array,
		times: PackedFloat32Array,
		easings: Array
) -> void:
	blade.phase = phase
	blade.path = path
	blade.segment_times = times
	blade.segment_easings = easings
	blade.duration = 0.0
	for time in times:
		blade.duration += time
	blade.time = 0.0
	blade.eased = 0.0
	blade.turn = 0


# Where along the path (0..1) the blade is after `time`: one easing over the whole path, or per
# segment — each segment then gets its own share of the time and its own speed curve.
func _eased_position(blade: Blade) -> float:
	var times := blade.segment_times
	if times.size() == 1:
		return _ease(clampf(blade.time / blade.duration, 0.0, 1.0), blade.segment_easings[0])
	var start := 0.0
	for i in times.size():
		if blade.time < start + times[i] or i == times.size() - 1:
			var local := clampf((blade.time - start) / times[i], 0.0, 1.0)
			return (i + _ease(local, blade.segment_easings[i])) / times.size()
		start += times[i]
	return 1.0


func _hold(blade: Blade, duration: float) -> void:
	_start(blade, HOLD, PackedVector3Array([blade.tip, blade.tip, blade.tip, blade.tip]), duration, "linear")


func _both_idle() -> bool:
	return _blades[0].phase == IDLE and _blades[1].phase == IDLE


func _begin(blade: Blade, move: String, enemy := -1) -> void:
	blade.move = move
	blade.enemy = enemy
	blade.hit.clear()
	blade.delay = STAGGER if blade.side > 0.0 else 0.0


# Each blade swings in its own plane: it holds your forward direction and an "up" that leans out
# to the blade's side by _swing_tilt — so the left blade's arcs bulge up-left and never cross over
# to the right, and the other way round.
func _swing_up(blade: Blade) -> Vector3:
	var tilt := deg_to_rad(_swing_tilt)
	return (Vector3.UP * cos(tilt) + _right() * blade.side * sin(tilt)).normalized()


# Every attack's OUT path = wind-up segment + strike segment, one smooth curve, in the blade's plane:
#   wind-up  hand → top (up the tilted plane, _top_behind back); its last handle sits behind the
#            top, so the blade arrives at the top moving forward
#   strike   top → target; its first handle sits ahead of the top on the same line (the tangents
#            match: no kink), its last two points come from the attack's own `path` (p2, p3)
# The wind-up eases into the top (slows: the hang), the strike uses the attack's easing.
# Control points 0–3 = wind-up, 4–7 = strike (3 and 4 are the same point: the join).
func _launch(
		blade: Blade,
		path: PackedVector3Array,
		duration: float,
		easing: String
) -> void:
	var hand := _hand(blade.side)
	var up := _swing_up(blade)
	var top := _wind_top(blade)
	var reach := top.distance_to(path[3])
	var windup := [hand, hand + up * 0.6 - _facing * _top_behind * 0.5, top - _facing * 0.8, top]
	var strike := [top, top + _facing * reach * 0.35, path[2], path[3]]
	_start_segments(blade, OUT, PackedVector3Array(windup + strike),
			PackedFloat32Array([_windup_time, duration]), ["smooth", easing])


# The top of the wind-up: up the blade's tilted plane to _top_height, _top_behind back.
func _wind_top(blade: Blade) -> Vector3:
	var hand := _hand(blade.side)
	var up := _swing_up(blade)
	return hand + up * (_top_height - HAND_HEIGHT) / maxf(up.y, 0.2) - _facing * _top_behind


# --- moves --------------------------------------------------------------------------------------

func _throw(aim: Vector3, move: String, enemy := -1) -> void:
	if not _both_idle():
		return
	for blade in _blades:
		_begin(blade, move, enemy)
		var hand := _hand(blade.side)
		var right := _right() * blade.side
		var target := aim + right * PAIR_GAP
		var distance := hand.distance_to(target)
		# Comes down onto the target from above and out on its own side (its tilted plane).
		var p2 := target + _swing_up(blade) * distance * 0.25
		_launch(blade, PackedVector3Array([hand, hand, p2, target]), 0.15 + distance * 0.008, "out")


func _sweep() -> void:
	if not _both_idle():
		return
	for blade in _blades:
		_begin(blade, "sweep")
		var hand := _hand(blade.side)
		var start := _sweep_path(blade)[0]
		_launch(blade, PackedVector3Array([hand, hand.lerp(start, 0.33) + Vector3.UP * 0.5, hand.lerp(start, 0.66) + Vector3.UP * 0.5, start]), 0.1, "linear")


# Each blade sweeps its own side only: a quarter circle from its side round to the front, where
# the two meet (just short of crossing). A quarter circle is one cubic Bezier with handles
# CIRCLE_K × radius long, along the circle's tangents.
func _sweep_path(blade: Blade) -> PackedVector3Array:
	var side := _right() * blade.side
	var center := _player + Vector3(0, 1.1, 0)
	var a := center + side * SWEEP_RADIUS
	var b := center + _facing * SWEEP_RADIUS + side * 0.3
	var k := CIRCLE_K * SWEEP_RADIUS
	return PackedVector3Array([a, a + _facing * k, b + side * k, b])


func _spin() -> void:
	if not _both_idle():
		return
	for blade in _blades:
		_begin(blade, "spin")
		blade.delay = 0.0 # opposite each other from the start
		blade.spin_right = _right()
		blade.spin_forward = _facing
		var hand := _hand(blade.side)
		var start := _spin_path(blade)[0]
		_launch(blade, PackedVector3Array([hand, hand.lerp(start, 0.33), hand.lerp(start, 0.66), start]), 0.1, "linear")


# SPIN_TURNS full circles round the player, 4 quarter-circle Beziers per turn. The right blade starts
# on your right (angle 0), the left one opposite (angle π); both go the same way round.
func _spin_path(blade: Blade) -> PackedVector3Array:
	var center := _player + Vector3(0, 1.1, 0)
	var start := 0.0 if blade.side > 0.0 else PI
	var path := PackedVector3Array()
	for q in SPIN_TURNS * 4:
		var a0 := start + q * PI / 2.0
		var a1 := a0 + PI / 2.0
		var p0 := _circle(blade, center, a0)
		var p3 := _circle(blade, center, a1)
		path.append(p0)
		path.append(p0 + _circle_tangent(blade, a0) * CIRCLE_K * SPIN_RADIUS)
		path.append(p3 - _circle_tangent(blade, a1) * CIRCLE_K * SPIN_RADIUS)
		path.append(p3)
	return path


func _circle(blade: Blade, center: Vector3, angle: float) -> Vector3:
	return center + (blade.spin_right * cos(angle) + blade.spin_forward * sin(angle)) * SPIN_RADIUS


func _circle_tangent(blade: Blade, angle: float) -> Vector3:
	return -blade.spin_right * sin(angle) + blade.spin_forward * cos(angle)


func _pull() -> void:
	var aim := _mouse_point()
	var best := -1
	var best_distance := 4.0 # enemies within 4 m of the mouse point
	for i in _enemies.size():
		var d := _enemies[i].position.distance_to(aim)
		if d < best_distance and _enemies[i].position.distance_to(_player) < PULL_RANGE and _enemies[i].bound <= 0.0:
			best = i
			best_distance = d
	if best >= 0 and _both_idle():
		_fling = _pick_fling()
		_throw(_enemies[best].position, "pull", best)


# Where a pull throws the enemy. Over the head (behind) needs a higher arc and a bit more time;
# the side and back throws keep some speed so the enemy tumbles on after landing.
func _pick_fling() -> Dictionary:
	var right := _right()
	var choices := [
		{"name": "at your feet", "to": _facing * 1.6, "lift": 2.0, "duration": 0.32, "carry": 0.0},
		{"name": "flung left", "to": -right * 4.5 + _facing * 0.5, "lift": 2.5, "duration": 0.36, "carry": 5.0},
		{"name": "flung right", "to": right * 4.5 + _facing * 0.5, "lift": 2.5, "duration": 0.36, "carry": 5.0},
		{"name": "over your head", "to": -_facing * 5.0, "lift": 5.0, "duration": 0.48, "carry": 4.0},
	]
	return choices[randi() % choices.size()]


func _slice() -> void:
	if not _both_idle():
		return
	var aim := _mouse_point()
	var best := -1
	var best_distance := 4.0
	for i in _enemies.size():
		var d := _enemies[i].position.distance_to(aim)
		if d < best_distance and _enemies[i].position.distance_to(_player) < SLICE_RANGE:
			best = i
			best_distance = d
	if best < 0:
		return
	_slice_enemy = best
	for blade in _blades:
		_begin(blade, "slice")
		blade.slice_angles.clear()
	for k in SLICES: # alternate blades; each slash in its own random plane
		_blades[(k + 1) % 2].slice_angles.append(deg_to_rad(randf_range(SLICE_TILT_MIN, SLICE_TILT_MAX)))
	# The right blade starts half a cycle later: one slashes while the other reels in and winds up.
	_blades[1].delay = (_windup_time + 0.08 + SLICE_TIME + 0.2) * 0.5
	for blade in _blades:
		_slice_out(blade)


# One slash from the hand: wind up, then out (over the shoulder) to the slash's start.
func _slice_out(blade: Blade) -> void:
	var start := _slice_path(blade)[0]
	var hand := _hand(blade.side)
	var lift := Vector3.UP * 0.8
	_launch(blade, PackedVector3Array([hand, hand.lerp(start, 0.33) + lift, hand.lerp(start, 0.66) + lift, start]), 0.08, "linear")


# One slash: down through the enemy from above and out on the blade's own side, in a plane that
# holds the line hand → enemy (x) and leans toward the blade's side by its random angle.
#   along x: starts and ends short of the enemy (near), handles past it (far), so at the middle
#            (Bezier at t = ½: (p0 + 3·p1 + 3·p2 + p3) / 8) it is exactly at the enemy's distance
#   across:  starts high-outside (o0), ends straight below (o3); the handles o1, o2 are chosen so
#            o0 + 3·o1 + 3·o2 + o3 = 0, i.e. the middle is exactly ON the enemy. A symmetric loop
#            would end as far over the other side as it started; this one only dips a little past.
func _slice_path(blade: Blade) -> PackedVector3Array:
	var enemy := _enemies[_slice_enemy].position
	var hand := _hand(blade.side)
	var x := enemy - hand
	var distance := x.length()
	x = x / distance if distance > 1e-3 else _facing
	var up := (Vector3.UP - x * x.dot(Vector3.UP)).normalized()
	var side := _right() * blade.side
	side = (side - x * x.dot(side)).normalized()
	var angle := blade.slice_angles[0]
	var o0 := (up * cos(angle) + side * sin(angle)) * SLICE_SIZE # high, out on its own side
	var o3 := -up * SLICE_SIZE * 0.8 # straight below
	var o1 := o0 * 0.5
	var o2 := -(o0 + o1 * 3.0 + o3) / 3.0
	var near := hand + x * distance * 0.55
	var far := hand + x * distance * (1.0 - 0.55 / 4.0) / 0.75
	return PackedVector3Array([near + o0, far + o1, far + o2, near + o3])


# Chain Bind: BIND_CHAINS chains out of the ground, spread round the enemy, each coiling a helix
# round it at its own height, alternating direction, a few frames apart.
func _bind() -> void:
	if not _binds.is_empty():
		return
	var aim := _mouse_point()
	var best := -1
	var best_distance := 4.0
	for i in _enemies.size():
		var d := _enemies[i].position.distance_to(aim)
		if d < best_distance and _enemies[i].position.distance_to(_player) < BIND_RANGE:
			best = i
			best_distance = d
	if best < 0:
		return
	var enemy := _enemies[best]
	var foot := Vector3(enemy.position.x, 0.0, enemy.position.z)
	var longest := 0.0
	for i in BIND_CHAINS:
		var angle := TAU * i / BIND_CHAINS + randf_range(-0.3, 0.3)
		var anchor := foot + Vector3(cos(angle), 0.0, sin(angle)) * BIND_SPREAD
		var h0 := 0.35 + 0.25 * i
		var direction := 1.0 if i % 2 == 0 else -1.0
		var route := ChainBind.coil_points(anchor, foot, COIL_RADIUS, angle, COIL_TURNS, h0, h0 + 0.5, direction)
		var bind := ChainBind.new(route, BIND_LINKS, Color(0.32, 0.2, 0.15))
		bind.delay = i * 0.06
		longest = maxf(longest, bind.delay + bind.grow_time + bind.hold_time)
		_binds.append(bind)
		_bind_renderer.add(bind.chain)
	_bind_enemy = best
	_bind_gripped = false
	enemy.bound = longest
	enemy.velocity = Vector3.ZERO


func _update_binds(delta: float) -> void:
	if _binds.is_empty():
		return
	var done := true
	for bind in _binds:
		bind.update(delta)
		if bind.phase >= ChainBind.HOLD and not _bind_gripped: # the first chain closes: grip
			_bind_gripped = true
			_enemies[_bind_enemy].flash = 1.0
			_hitstop = HITSTOP
			_shake = 0.2
		done = done and bind.phase == ChainBind.DONE
	if done:
		_binds.clear()
		_bind_renderer.clear()
		_bind_enemy = -1


# Rush / grapple: hook the enemy or post ring nearest the mouse on screen with both blades.
func _rush() -> void:
	if not _both_idle() or _rushing:
		return
	var best_pixels := RUSH_PICK_PIXELS
	var target := Vector3.ZERO
	var enemy := -2
	for i in _enemies.size():
		var pixels := camera.unproject_position(_enemies[i].position).distance_to(_mouse)
		if pixels < best_pixels and not camera.is_position_behind(_enemies[i].position):
			best_pixels = pixels
			target = _enemies[i].position
			enemy = i
	for top in POSTS:
		var pixels := camera.unproject_position(top).distance_to(_mouse)
		if pixels < best_pixels and not camera.is_position_behind(top):
			best_pixels = pixels
			target = top
			enemy = -1
	if enemy == -2:
		return
	_rush_enemy = enemy
	_rush_target = target
	_throw(target, "rush", enemy)


# The player's arc (feet): up and over, down onto the spot — in front of the enemy, or just under
# the ring. Ease-in like the yank: a beat of tension, then violent.
func _start_rush(enemy: int) -> void:
	var from := _player
	var to: Vector3
	if enemy >= 0:
		var foot := _enemies[enemy].position
		foot.y = 0.0
		var flat := foot - Vector3(from.x, 0.0, from.z)
		to = foot - flat.normalized() * RUSH_STOP if flat.length() > RUSH_STOP else from
	else:
		to = _rush_target + Vector3(0, -HAND_HEIGHT - 0.4, 0) # hands just under the ring
	var distance := from.distance_to(to)
	var lift := 1.5 + distance * 0.12
	_rush_path = PackedVector3Array([from, from + Vector3.UP * lift, to + Vector3.UP * lift * 0.6, to])
	_rush_duration = 0.25 + distance * 0.025
	_rush_time = 0.0
	_rushing = true
	_player_velocity = Vector3.ZERO


func _update_rush(delta: float) -> void:
	if not _rushing:
		return
	_rush_time += delta
	var t := clampf(_rush_time / _rush_duration, 0.0, 1.0)
	var p := _rush_path
	var previous := _player
	_player = Bezier.point(p[0], p[1], p[2], p[3], _ease(t, "in"))
	var flat := Vector3(p[3].x - _player.x, 0.0, p[3].z - _player.z)
	if flat.length() > 0.3:
		_facing = flat.normalized()
	if t < 1.0:
		return
	_rushing = false
	if _rush_enemy >= 0: # land on it: slam
		_hit(_blades[0], _rush_enemy, true)
		_hitstop = HITSTOP * 2.0
		_shake = 0.4
	else: # the ring: keep the speed of the arrival and fly on, then fall
		_player_velocity = (_player - previous) / maxf(delta, 1e-4) * 0.5
	for blade in _blades:
		if blade.phase == HOLD or blade.phase == OUT:
			_back(blade)


# Whirl: hook the enemy nearest the mouse with both blades (an ordinary throw, move "whirl").
func _whirl() -> void:
	if not _both_idle():
		return
	var aim := _mouse_point()
	var best := -1
	var best_distance := 4.0
	for i in _enemies.size():
		var d := _enemies[i].position.distance_to(aim)
		if d < best_distance and _enemies[i].position.distance_to(_player) < PULL_RANGE and _enemies[i].bound <= 0.0:
			best = i
			best_distance = d
	if best >= 0:
		_throw(_enemies[best].position, "whirl", best)


# Hooked: the enemy is now swung by the chains. The circle starts where the enemy already is
# (its angle round the player), with a fixed frame so turning to aim doesn't spin the circle.
func _start_whirl(enemy: int) -> void:
	_whirl_enemy = enemy
	_whirl_right = _right()
	_whirl_forward = _facing
	var offset := _enemies[enemy].position - _player
	_whirl_angle = atan2(offset.dot(_whirl_forward), offset.dot(_whirl_right))
	_whirl_from = _enemies[enemy].position
	_enemies[enemy].yanked = true # the path moves it now
	for blade in _blades:
		_start(blade, WHIRL, _whirl_path(), WHIRL_TIME, "accelerate")


# Lead-in (from the enemy to the circle, arriving along its tangent) + WHIRL_TURNS turns of
# quarter-circle Beziers round the player.
func _whirl_path() -> PackedVector3Array:
	var center := _player + Vector3(0, 1.4, 0)
	var path := PackedVector3Array()
	var start := _circle_point(center, _whirl_angle)
	var from := _whirl_from
	var k := CIRCLE_K * WHIRL_RADIUS
	path.append_array([from, from.lerp(start, 0.4) + Vector3.UP * 0.8, start - _circle_tangent_at(_whirl_angle) * k, start])
	for q in WHIRL_TURNS * 4:
		var a0 := _whirl_angle + q * PI / 2.0
		var a1 := a0 + PI / 2.0
		var p0 := _circle_point(center, a0)
		var p3 := _circle_point(center, a1)
		path.append_array([p0, p0 + _circle_tangent_at(a0) * k, p3 - _circle_tangent_at(a1) * k, p3])
	return path


func _circle_point(center: Vector3, angle: float) -> Vector3:
	return center + (_whirl_right * cos(angle) + _whirl_forward * sin(angle)) * WHIRL_RADIUS


func _circle_tangent_at(angle: float) -> Vector3:
	return -_whirl_right * sin(angle) + _whirl_forward * cos(angle)


# Let go along the swing's direction right now: the enemy becomes a projectile.
func _release_whirl(direction: Vector3) -> void:
	var enemy := _enemies[_whirl_enemy]
	enemy.yanked = false
	enemy.thrown = true
	var flat := Vector3(direction.x, 0.0, direction.z).normalized()
	enemy.velocity = flat * THROW_SPEED + Vector3.UP * 5.0
	_whirl_enemy = -1
	_shake = 0.25
	for blade in _blades:
		blade.enemy = -1
		_back(blade)


func _back(blade: Blade) -> void:
	var hand := _hand(blade.side)
	var right := _right() * blade.side
	var distance := hand.distance_to(blade.tip)
	var p1 := blade.tip + Vector3.UP * distance * 0.2 + right * distance * 0.25
	var p2 := hand + _facing * distance * 0.3 + right * distance * 0.35
	_start(blade, BACK, PackedVector3Array([blade.tip, p1, p2, hand]), 0.12 + distance * 0.01, "smooth")


func _update_blade(blade: Blade, delta: float) -> void:
	blade.previous_tip = blade.tip
	if blade.phase == IDLE:
		blade.tip = _hand(blade.side) + Vector3(0, -0.15, 0)
		return
	if blade.delay > 0.0:
		blade.delay -= delta
		blade.tip = _hand(blade.side)
		blade.path[0] = blade.tip
		return
	blade.time += delta
	match blade.phase: # paths that follow the player (or the enemy) while they play
		BACK: blade.path[3] = _hand(blade.side)
		SWEEP: blade.path = _sweep_path(blade)
		SPIN: blade.path = _spin_path(blade)
		SLICE: blade.path = _slice_path(blade)
		WHIRL: blade.path = _whirl_path()
	var t := clampf(blade.time / blade.duration, 0.0, 1.0)
	blade.eased = _eased_position(blade)
	var base := Bezier.path_point(blade.path, blade.eased)
	blade.tip = base
	if (blade.phase == YANK or blade.phase == WHIRL) and blade.enemy >= 0:
		_enemies[blade.enemy].position = base
		blade.tip = base + _right() * blade.side * PAIR_GAP
	if blade.phase == WHIRL and blade.side > 0.0 and _whirl_enemy >= 0: # one blade decides the release
		var turns := blade.eased * (WHIRL_TURNS * 4 + 1) / 4.0 - 0.25 # the lead-in is a quarter
		var swing := Bezier.path_tangent(blade.path, blade.eased).normalized()
		var aim := _mouse_point() - _player
		aim.y = 0.0
		if (turns >= WHIRL_MIN_TURNS and swing.dot(aim.normalized()) > WHIRL_AIM) or blade.time >= blade.duration:
			_release_whirl(swing)
			return
	if blade.phase == SPIN:
		var turn := int(blade.eased * SPIN_TURNS)
		if turn != blade.turn: # a new turn: every enemy can be hit again
			blade.turn = turn
			blade.hit.clear()
	if blade.phase == OUT or blade.phase == SWEEP or blade.phase == SPIN or blade.phase == SLICE:
		var phase := blade.phase
		_check_hits(blade)
		if blade.phase != phase: # a hit stuck the blade (HOLD started): don't end that phase now
			return
	if t >= 1.0:
		_next_phase(blade)


func _next_phase(blade: Blade) -> void:
	match blade.phase:
		OUT:
			match blade.move:
				"sweep": _start(blade, SWEEP, _sweep_path(blade), 0.22, "smooth")
				"spin": _start(blade, SPIN, _spin_path(blade), SPIN_TIME, "linear")
				"slice":
					blade.hit.clear()
					_start(blade, SLICE, _slice_path(blade), SLICE_TIME, "smooth")
				"pull":
					if blade.enemy >= 0:
						_hit(blade, blade.enemy, false)
					_hold(blade, 0.12)
				"whirl":
					if blade.enemy >= 0:
						_hit(blade, blade.enemy, false)
						if _whirl_enemy < 0: # the first blade to hook starts the swing
							_start_whirl(blade.enemy)
						elif blade.phase == OUT: # the second joins it
							_start(blade, WHIRL, _whirl_path(), WHIRL_TIME, "accelerate")
							blade.time = _blades[0].time if blade.side > 0.0 else _blades[1].time
				"rush": # hooked: stay in the target while the player travels to it
					if blade.enemy >= 0:
						_hit(blade, blade.enemy, false)
					_hold(blade, 99.0)
					if not _rushing:
						_start_rush(blade.enemy)
				_: _hold(blade, 0.06)
		HOLD:
			if blade.move == "pull" and blade.enemy >= 0:
				var from := _enemies[blade.enemy].position
				var to: Vector3 = _player + _fling.to + Vector3(0, 1.0, 0)
				var lift: float = _fling.lift
				_enemies[blade.enemy].yanked = true
				_start(blade, YANK, PackedVector3Array([from, from + Vector3.UP * lift, to + Vector3.UP * lift * 0.8, to]), _fling.duration, "in")
			else:
				_back(blade)
		YANK:
			var enemy := _enemies[blade.enemy]
			enemy.yanked = false
			var p := blade.path
			var arrival := p[3] - p[0] # keep going the way the throw went
			arrival.y = 0.0
			enemy.velocity = arrival.normalized() * float(_fling.carry) if arrival.length() > 1e-3 else Vector3.ZERO
			_hit(blade, blade.enemy, false) # lands: hit-stop + flash, no knockback
			blade.enemy = -1
			_back(blade)
		SWEEP, SPIN, WHIRL:
			_back(blade)
		SLICE: # reel the chain fully home before the next slash
			blade.slice_angles.pop_front()
			_back(blade)
		BACK:
			if blade.move == "slice" and not blade.slice_angles.is_empty():
				_slice_out(blade)
			else:
				blade.phase = IDLE


func _check_hits(blade: Blade) -> void:
	for i in _enemies.size():
		if i in blade.hit or i == blade.enemy:
			continue
		# the whole stretch the blade covered this frame, so a fast blade can't skip through
		var closest := Geometry3D.get_closest_point_to_segment(_enemies[i].position, blade.previous_tip, blade.tip)
		if closest.distance_to(_enemies[i].position) < HIT_RADIUS:
			blade.hit.append(i)
			_hit(blade, i, true)


func _hit(
		blade: Blade,
		index: int,
		knock: bool
) -> void:
	var enemy := _enemies[index]
	enemy.flash = 1.0
	_spark(blade.tip if blade.tip.distance_to(enemy.position) < 2.0 else enemy.position)
	if blade.move == "slice":
		knock = false
	if knock and not enemy.yanked:
		var away := enemy.position - _player
		away.y = 0.0
		enemy.velocity = away.normalized() * 7.0 + Vector3.UP * 2.0
	_hitstop = HITSTOP * (0.5 if blade.move == "slice" else 1.0)
	_shake = 0.25
	if blade.move == "throw" and blade.phase == OUT: # a throw that hits stops there and sticks
		_hold(blade, 0.08)


func _update_enemies(delta: float) -> void:
	for enemy in _enemies:
		enemy.flash = maxf(enemy.flash - delta * 4.0, 0.0)
		if enemy.yanked:
			continue
		if enemy.bound > 0.0: # held by Chain Bind
			enemy.bound -= delta
			enemy.velocity = Vector3.ZERO
			continue
		enemy.velocity.y -= 20.0 * delta
		enemy.position += enemy.velocity * delta
		if enemy.position.y <= 1.0:
			enemy.position.y = 1.0
			enemy.thrown = false
			enemy.velocity = enemy.velocity * Vector3(exp(-8.0 * delta), 0, exp(-8.0 * delta))
	_body_hits(delta)


# A swung (whirl) or thrown enemy bowls over any other enemy it touches.
func _body_hits(delta: float) -> void:
	for enemy in _enemies:
		enemy.cooldown = maxf(enemy.cooldown - delta, 0.0)
	for i in _enemies.size():
		var weapon := _enemies[i]
		if not (weapon.thrown or i == _whirl_enemy):
			continue
		for j in _enemies.size():
			var other := _enemies[j]
			if j == i or other.cooldown > 0.0 or other.bound > 0.0 or other.yanked:
				continue
			if weapon.position.distance_to(other.position) > BODY_HIT:
				continue
			var away := other.position - weapon.position
			away.y = 0.0
			other.velocity = away.normalized() * 9.0 + Vector3.UP * 4.0
			other.flash = 1.0
			other.cooldown = 0.4
			_spark((weapon.position + other.position) * 0.5)
			_hitstop = HITSTOP * 0.7
			_shake = maxf(_shake, 0.2)


# --- drawing ------------------------------------------------------------------------------------

# The chain: on the blade's path while it flies out (wind-up and strike) or reels back, else taut.
# Returns the link positions; `taut_controls` gets the taut curve's control points (for debug).
func _chain_points(
		blade: Blade,
		delta: float,
		taut_controls: Array[PackedVector3Array]
) -> PackedVector3Array:
	var hand := _hand(blade.side)
	if blade.delay <= 0.0 and blade.phase == OUT and blade.move != "sweep" and blade.move != "spin":
		return BladeChain.on_path(hand, blade.path, 0.0, blade.eased, true, LINKS)
	if blade.delay <= 0.0 and blade.phase == BACK:
		return BladeChain.on_path(hand, blade.path, blade.eased, 1.0, false, LINKS)
	var motion := blade.tip - blade.previous_tip
	var speed := motion.length() / maxf(delta, 1e-4)
	var trail := -motion.normalized() * clampf(speed / 25.0, 0.0, 1.0) if speed > 0.01 else Vector3.ZERO
	var c := BladeChain.taut(hand, blade.tip, trail)
	taut_controls.append(c)
	return Bezier.even_points(c[0], c[1], c[2], c[3], LINKS)


func _draw(delta: float) -> void:
	_player_mesh.position = _player + Vector3(0, 0.8, 0)
	_player_mesh.rotation.y = atan2(-_facing.x, -_facing.z)
	var curves: Array[PackedVector3Array] = [] # taut chains' control points, for the debug view
	for blade in _blades:
		blade.chain.points = _chain_points(blade, delta, curves)
		blade.mesh.position = blade.tip
		# The blade continues the chain: it points along the last link, away from the hand.
		var links := blade.chain.points
		var heading := links[-1] - links[-2]
		if heading.length() < 1e-3:
			heading = blade.tip - _hand(blade.side)
		if heading.length() > 1e-3 and absf(heading.normalized().dot(Vector3.UP)) < 0.99:
			blade.mesh.look_at(blade.tip + heading, Vector3.UP)
	_renderer.draw()
	_bind_renderer.draw()
	_draw_fire(delta)
	for enemy in _enemies:
		enemy.mesh.position = enemy.position
		enemy.material.emission_enabled = enemy.flash > 0.0
		enemy.material.emission = Color(1, 0.3, 0.2) * enemy.flash
	_shake = maxf(_shake - delta, 0.0)
	var jolt := Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * _shake * 0.4
	camera.position = camera.position.lerp(_player + CAMERA_OFFSET, 1.0 - exp(-5.0 * delta)) + jolt
	camera.look_at(camera.position - CAMERA_OFFSET - jolt + Vector3(0, 0.5, 0))
	_draw_curves(curves)
	var pull := "   pull: %s" % _fling.name if not _fling.is_empty() else ""
	hud.text = "left %s · right %s%s%s%s\nLeft click throw   Right click sweep   Q spin   E pull   F slice   G bind   Space rush   X whirl   WASD move   C curves   T slow motion   V fire (%s)   R reset" % [
		PHASE_NAMES[_blades[0].phase], PHASE_NAMES[_blades[1].phase], pull, "   [curves]" if _show_curves else "", "   [slow]" if _slow else "",
		"%d %s" % [_fire_step(), FIRE_STEPS[_fire_step() - 1]]]


# Debug view (C). For every Bezier segment of each moving blade's path (orange) and each taut
# chain (cyan):
#   control polygon  P0 → P1 → P2 → P3, dim: the straight lines the curve is pulled toward
#   control points   white crosses (P0 / P3 on the curve, P1 / P2 the handles off it)
#   smooth curve     the Bezier itself
#   tangents         green ticks along the curve: its direction (and, by length, its speed in t)
# plus the blade's own tangent right now (yellow) and the join between segments (magenta).
# Each moving blade adds a sample (its base and tip) to its trail; fast blades shed embers.
func _draw_fire(delta: float) -> void:
	for b in _blades.size():
		var blade := _blades[b]
		var moving := blade.phase != IDLE and blade.phase != HOLD and blade.delay <= 0.0
		if moving:
			var heading := -blade.mesh.global_basis.z # the blade points along its −Z (see _draw)
			_trails[b].add_sample(blade.tip - heading * TRAIL_SPAN, blade.tip)
		_trails[b].rebuild()
		var speed := blade.tip.distance_to(blade.previous_tip) / maxf(delta, 1e-4)
		_embers[b].global_position = blade.tip
		_embers[b].emitting = moving and speed > EMBER_SPEED


func _spark(at: Vector3) -> void:
	var burst := _sparks[_next_spark]
	_next_spark = (_next_spark + 1) % _sparks.size()
	burst.global_position = at
	burst.restart()


func _draw_curves(chains: Array[PackedVector3Array]) -> void:
	_debug_mesh.clear_surfaces()
	_label_points()
	if not _show_curves:
		return
	_debug_mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for blade in _blades:
		if blade.phase == IDLE or blade.path.size() < 4:
			continue
		for k in range(0, blade.path.size() - 3, 4):
			_curve_lines(blade.path.slice(k, k + 4), Color(1.0, 0.55, 0.1))
			if k > 0:
				_cross(blade.path[k], Color(1.0, 0.2, 0.9), 0.18)
		var tangent := Bezier.path_tangent(blade.path, blade.eased)
		if tangent.length() > 1e-4:
			_line(blade.tip, blade.tip + tangent.normalized() * 0.9, Color(1.0, 1.0, 0.2))
	for c in chains:
		_curve_lines(c, Color(0.3, 0.9, 1.0))
	if _rushing: # the player's rush arc
		_curve_lines(_rush_path, Color(0.4, 1.0, 0.6))
	for bind in _binds: # each bind chain: its points (crosses + dim polyline) and the smooth path
		var violet := Color(0.75, 0.45, 1.0)
		for i in bind.points.size():
			_cross(bind.points[i], Color.WHITE, 0.06)
			if i > 0:
				_line(bind.points[i - 1], bind.points[i], Color(violet, 0.4))
		var last := bind.path[0]
		for i in range(1, 121):
			var p := Bezier.path_point(bind.path, i / 120.0)
			_line(last, p, violet)
			last = p
	_debug_mesh.surface_end()


# Control point ids on each moving blade's path: L0… left, R0… right; the join reads "L3/4".
func _label_points() -> void:
	var used := 0
	if _show_curves:
		for blade in _blades:
			if blade.phase == IDLE or blade.path.size() < 4:
				continue
			var prefix := "L" if blade.side < 0.0 else "R"
			for i in blade.path.size():
				if i > 0 and i % 4 == 0 and blade.path[i].is_equal_approx(blade.path[i - 1]):
					_labels[used - 1].text += "/%d" % i # same point as the last one: the join
					continue
				if used == _labels.size():
					var label := Label3D.new()
					label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
					label.no_depth_test = true
					label.pixel_size = 0.005
					label.font_size = 40
					label.outline_size = 8
					add_child(label)
					_labels.append(label)
				var label := _labels[used]
				label.text = "%s%d" % [prefix, i]
				label.modulate = Color(1.0, 0.75, 0.5) if blade.side < 0.0 else Color(1.0, 0.95, 0.5)
				label.position = blade.path[i] + Vector3(0, 0.18, 0)
				label.visible = true
				used += 1
	for k in range(used, _labels.size()):
		_labels[k].visible = false


func _curve_lines(
		c: PackedVector3Array,
		color: Color
) -> void:
	var dim := Color(color, 0.45)
	_line(c[0], c[1], dim) # control polygon
	_line(c[1], c[2], dim)
	_line(c[2], c[3], dim)
	var last := c[0]
	for i in range(1, 33): # smooth curve
		var p := Bezier.point(c[0], c[1], c[2], c[3], i / 32.0)
		_line(last, p, color)
		last = p
	for i in 9: # tangent ticks: direction, length ∝ speed along t
		var t := i / 8.0
		var p := Bezier.point(c[0], c[1], c[2], c[3], t)
		var d := Bezier.tangent(c[0], c[1], c[2], c[3], t)
		_line(p, p + d * 0.06, Color(0.3, 1.0, 0.4))
	for p in c: # control points
		_cross(p, Color.WHITE, 0.08)


func _line(
		a: Vector3,
		b: Vector3,
		color: Color
) -> void:
	_debug_mesh.surface_set_color(color)
	_debug_mesh.surface_add_vertex(a)
	_debug_mesh.surface_add_vertex(b)


func _cross(
		p: Vector3,
		color: Color,
		size: float
) -> void:
	for axis in [Vector3.RIGHT, Vector3.UP, Vector3.BACK]:
		_line(p - axis * size, p + axis * size, color)


# Sliders (bottom left) for the wind-up and the swing planes; they apply to the next attack.
func _add_sliders() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(16, get_viewport().get_visible_rect().size.y - 170)
	var box := VBoxContainer.new()
	panel.add_child(box)
	$Hud.add_child(panel)
	_slider(box, "Wind-up behind", 0.0, 3.0, 0.05, _top_behind, "%.2f m", func(v: float) -> void: _top_behind = v)
	_slider(box, "Wind-up height", 1.4, 4.5, 0.05, _top_height, "%.2f m", func(v: float) -> void: _top_height = v)
	_slider(box, "Wind-up time", 0.04, 0.6, 0.01, _windup_time, "%.2f s", func(v: float) -> void: _windup_time = v)
	_slider(box, "Swing tilt (out)", 0.0, 80.0, 1.0, _swing_tilt, "%.0f°", func(v: float) -> void: _swing_tilt = v)


func _slider(
		box: VBoxContainer,
		title: String,
		low: float,
		high: float,
		step: float,
		value: float,
		format: String,
		apply: Callable
) -> void:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.custom_minimum_size.x = 200
	label.text = "%s  %s" % [title, format % value]
	var slider := HSlider.new()
	slider.min_value = low
	slider.max_value = high
	slider.step = step
	slider.value = value
	slider.custom_minimum_size.x = 220
	slider.focus_mode = Control.FOCUS_NONE # keep the keys for the game
	slider.value_changed.connect(func(v: float) -> void:
		apply.call(v)
		label.text = "%s  %s" % [title, format % v])
	row.add_child(label)
	row.add_child(slider)
	box.add_child(row)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		_mouse = event.position
	elif event is InputEventMouseButton and event.pressed:
		_mouse = event.position
		if event.button_index == MOUSE_BUTTON_LEFT:
			_throw(_mouse_point(), "throw")
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			_sweep()
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_Q:
				_spin()
			KEY_E:
				_pull()
			KEY_F:
				_slice()
			KEY_G:
				_bind()
			KEY_SPACE:
				_rush()
			KEY_X:
				_whirl()
			KEY_V:
				_fire_material.set_shader_parameter("fire_step", _fire_step() % 4 + 1)
			KEY_C:
				_show_curves = not _show_curves
			KEY_T:
				_slow = not _slow
				Engine.time_scale = 0.2 if _slow else 1.0
			KEY_R:
				_reset()


# --- looks --------------------------------------------------------------------------------------

func _mesh_instance(
		mesh: Mesh,
		color: Color,
		glow := false
) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	if glow:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = 2.5
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	add_child(instance)
	return instance


func _capsule(
		radius: float,
		height: float
) -> Mesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = height
	return mesh


func _blade_mesh() -> Mesh: # a flat blade pointing along −Z (look_at's forward)
	var mesh := PrismMesh.new()
	mesh.size = Vector3(0.18, 0.7, 0.05)
	var turned := ArrayMesh.new()
	var arrays := mesh.get_mesh_arrays()
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var turn := Basis(Vector3.RIGHT, -PI / 2.0) # prism tip from +Y to −Z
	for i in vertices.size():
		vertices[i] = turn * vertices[i]
		normals[i] = turn * normals[i]
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TANGENT] = null
	turned.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return turned


func _fire_step() -> int:
	return int(_fire_material.get_shader_parameter("fire_step"))


# The trails (one shared fire material + noise), embers per blade, a few reusable spark bursts,
# and glow on the environment so colours above 1.0 bloom.
func _add_fire() -> void:
	var noise := FastNoiseLite.new()
	noise.frequency = 0.035
	noise.fractal_octaves = 3
	var texture := NoiseTexture2D.new()
	texture.width = 256
	texture.height = 64
	texture.seamless = true
	texture.noise = noise
	_fire_material = ShaderMaterial.new()
	_fire_material.shader = load("res://blades/fire_trail.gdshader")
	_fire_material.set_shader_parameter("noise", texture)
	_fire_material.set_shader_parameter("fire_step", 4)
	for i in 2:
		var trail := FireTrail.new(_fire_material)
		add_child(trail)
		_trails.append(trail)
		var embers := _particles(14, 0.45, 1.5, 0.06)
		embers.one_shot = false
		embers.explosiveness = 0.0
		embers.emitting = false
		_embers.append(embers)
	for i in 4:
		var burst := _particles(26, 0.4, 7.0, 0.08)
		burst.one_shot = true
		burst.explosiveness = 1.0
		burst.emitting = false
		_sparks.append(burst)
	var environment: Environment = $WorldEnvironment.environment
	environment.glow_enabled = true
	environment.glow_hdr_threshold = 1.0
	environment.glow_intensity = 0.5


# Little glowing squares that fly out, fall and shrink: sparks and embers.
func _particles(
		amount: int,
		lifetime: float,
		speed: float,
		size: float
) -> CPUParticles3D:
	var particles := CPUParticles3D.new()
	particles.amount = amount
	particles.lifetime = lifetime
	particles.local_coords = false
	particles.direction = Vector3.UP
	particles.spread = 180.0
	particles.initial_velocity_min = speed * 0.4
	particles.initial_velocity_max = speed
	particles.gravity = Vector3(0, -9.8, 0)
	particles.damping_min = 2.0
	particles.damping_max = 4.0
	var shrink := Curve.new()
	shrink.add_point(Vector2(0.0, 1.0))
	shrink.add_point(Vector2(1.0, 0.0))
	particles.scale_amount_curve = shrink
	var quad := QuadMesh.new()
	quad.size = Vector2.ONE * size
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	material.billboard_keep_scale = true # let the particles shrink
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.albedo_color = Color(2.5, 1.2, 0.3) # above 1: glows
	quad.material = material
	particles.mesh = quad
	add_child(particles)
	return particles


# Grapple posts: a pole from the ground up to a glowing ring (its POSTS point).
func _add_posts() -> void:
	for top in POSTS:
		var pole := CylinderMesh.new()
		pole.top_radius = 0.12
		pole.bottom_radius = 0.18
		pole.height = top.y
		var pole_instance := _mesh_instance(pole, Color(0.4, 0.36, 0.33))
		pole_instance.position = Vector3(top.x, top.y * 0.5, top.z)
		var ring := TorusMesh.new()
		ring.inner_radius = 0.28
		ring.outer_radius = 0.4
		var ring_instance := _mesh_instance(ring, Color(0.3, 0.9, 1.0), true)
		ring_instance.position = top + Vector3(0, 0.3, 0)
		ring_instance.rotation.x = PI / 2.0


func _add_ground() -> void:
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(60, 60)
	var ground := MeshInstance3D.new()
	ground.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.3, 0.28, 0.27)
	ground.material_override = material
	add_child(ground)
