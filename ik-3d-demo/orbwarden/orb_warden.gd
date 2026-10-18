## The Orb Warden boss: a giant gunmetal sphere on six long three-bone legs (OrbLayout), walking
## with the ordinary Spider gait — the legs, steps, body plane fit and body spring all come from
## spider/ — dressed here with its own look:
##   shell    the sphere, an equator belt and hip sockets, riding the spider's body (so it sinks
##            and sways with each tripod)
##   joints   OrbLimbs: glowing balls on every hip, knee and ankle — stagger points: enough hits
##            on one leg's joints make it buckle, and the warden reels for STAGGER_TIME (legs and
##            movement never break)
##   eye      OrbEye: the visor that slides over the sphere to watch the target
##   weak spots  OrbParts / OrbArmory: 4 power cores, 2 missile pods, the gun turret and the laser
##            emitter (the eye's lens, open only while the laser runs). Each broken weapon stops
##            that attack; every core lost makes it slower and slower to attack; with no cores
##            left it shuts down and collapses.
##   laser    OrbLaser: telegraph line → sweeping beam → scorch trail (+ sparks)
##   stomp    OrbStomp: rear up, slam the front feet down → `stomped` (the demo's shockwave)
##   missiles OrbMissiles: a volley from the pods, arcing onto marked spots → `missile_blast`
##   gun      OrbGun: bursts of bolts from the turret, on its own timer → `gun_hit`
## Brain (kept simple for now): turn to face the target, close to FIGHT_RANGE and hold there,
## attack every ATTACK_GAP seconds with a random pick from what it can still do (stomp only when
## close, missiles only when not), standing still while it attacks; the gun fires bursts in
## between. Menace rises while attacking, jumps when it's hit, and ebbs back between attacks.
class_name OrbWarden
extends Node3D

const SHELL := Color(0.15, 0.16, 0.18)
const BELT := Color(0.08, 0.08, 0.09)
const FIGHT_RANGE := 9.0 ## metres it likes to keep from the target
const ATTACK_GAP := Vector2(3.5, 6.0) ## seconds between auto attacks (random)
const ATTACK_RANGE := 18.0
const STOMP_RANGE := 12.0 ## the shockwave reaches ~15 m
const STOMP_CHANCE := 0.5
const STAND_LEGS := 3 ## fewer walking legs than this and it can't stand: it collapses
## It also collapses when its centre is outside the polygon its remaining feet make (seen from
## above) — e.g. every leg on one side gone: it tips over that way. The margin lets the centre sit
## this far past an edge (negative = outside) before it falls: the legs left can lean in and
## compensate a little, so losing a front pair or one side's front two still leaves it standing.
const BALANCE_MARGIN := -0.5
const DOWN_TILT := 0.4 ## radians it lies tilted toward the side it fell to
const FLOP := Vector2(2.4, 1.0) ## a collapsed leg lies (out, down) from its hip (body space)
const EYE_FADE := 3.0 ## seconds for the eye to flicker out once down
const TURN_RATE := 0.6 ## rad/s at most
const MENACE_EASE := 0.6 ## 1/s
const MISSILE_MIN := 6.0 ## no missile volleys at a target closer than this
const GUN_GAP := Vector2(2.0, 3.5) ## seconds between gun bursts
const GUN_RANGE := 26.0
const STAGGER_TIME := 2.2 ## seconds it reels after a leg buckles
const STAGGER_DIP := Vector3(-0.8, 0.25, 0.3) ## (height m, pitch, roll) toward the buckled leg
const BUCKLE_SPLAY := 1.2 ## metres the buckled leg's foot skids out

var target: Node3D
var auto_attack := true
var calm_menace := 0.0 ## menace between attacks ([ ] in the demo)
var collapsed := false ## down for good (too few legs to stand): no walking, no attacks — K repairs
var walk_speed := 1.0 ## × the spider's move_speed (and its gait cycle runs this much faster)
var paused := false ## stand still (turning and attacks still happen)
## Wave gait timing (see OrbLayout): each leg pair this share of a cycle behind the one in front…
var pair_delay := 1.0 / 3.0:
	set(value):
		pair_delay = value
		_apply_phases()
## …and the right legs this share of a cycle after the left (0.5 = opposite).
var side_offset := 0.5:
	set(value):
		side_offset = value
		_apply_phases()
var spider: Spider
var eye: OrbEye
var laser: OrbLaser
var stomp: OrbStomp
var limbs: OrbLimbs
var sparks: OrbSparks
var parts: OrbParts
var missiles: OrbMissiles
var gun: OrbGun ## rebuilt with the body (OrbArmory)
var gun_auto := true ## the turret fires bursts on its own

signal stomped(at: Vector3)
signal missile_blast(at: Vector3, radius: float)
signal gun_hit(direction: Vector3)

var _menace := 0.0
var _down_time := 0.0
var _attack_wait := 3.0
var _gun_wait := 2.0
var _stagger_time := 0.0
var _buckled := -1 ## the leg that buckled (its foot is held splayed out), −1 = none
var _time := 0.0


func _ready() -> void:
	process_priority = SpiderChainRig.SOLVE_PRIORITY + 1 # after the legs are solved: joints sit on them
	spider = Spider.new()
	spider.name = "Spider"
	spider.layout_override = OrbLayout.make()
	spider.chain_legs = true
	spider.show_targets = false
	spider.auto_walk = false
	spider.move_speed = 1.1
	spider.step_distance = 1.1
	spider.step_height = 0.9
	spider.step_time = 0.45
	spider.fast_step_time = 0.25
	spider.lead_time = 0.4
	spider.max_lead = 1.2
	spider.lean = 0.02
	spider.steering = true
	spider.wave_gait = true # OrbLayout's phased pairs; off = alternating tripods
	add_child(spider)
	sparks = OrbSparks.new()
	sparks.name = "Sparks"
	add_child(sparks)
	laser = OrbLaser.new()
	laser.name = "Laser"
	laser.sparks = sparks
	add_child(laser)
	stomp = OrbStomp.new()
	stomp.name = "Stomp"
	stomp.warden = self
	stomp.impact.connect(func(at: Vector3) -> void: stomped.emit(at))
	add_child(stomp)
	limbs = OrbLimbs.new()
	limbs.name = "Limbs"
	limbs.warden = self
	limbs.sparks = sparks
	limbs.process_priority = process_priority # after the legs are solved: balls sit on them
	limbs.joint_hit.connect(_on_joint_hit)
	limbs.leg_broken.connect(_on_leg_broken)
	limbs.leg_buckled.connect(_on_leg_buckled)
	add_child(limbs)
	parts = OrbParts.new()
	parts.name = "Parts"
	parts.sparks = sparks
	parts.process_priority = process_priority
	parts.part_hit.connect(_on_part_hit)
	parts.part_broken.connect(_on_part_broken)
	add_child(parts)
	missiles = OrbMissiles.new()
	missiles.name = "Missiles"
	missiles.sparks = sparks
	missiles.exploded.connect(func(at: Vector3, radius: float) -> void: missile_blast.emit(at, radius))
	add_child(missiles)
	_dress()
	spider.rebuilt.connect(_dress)


## Start a laser sweep at the target now (if not already attacking and the emitter still works).
func attack() -> void:
	if target != null and not busy() and not eye.broken:
		laser.start(spider.global_position, target.global_position)


## Fire a missile volley from every pod still on (if not already attacking or a volley in the air).
func missiles_now() -> void:
	if target == null or busy() or missiles.busy():
		return
	var launchers: Array[Vector3] = []
	for part in parts.parts:
		if part.kind == "pod" and part.alive:
			launchers.append(OrbArmory.launch_point(part.node))
	if not launchers.is_empty():
		missiles.volley(launchers, target.global_position)


## A gun burst now (if the turret still works).
func gun_now() -> void:
	if not collapsed and _stagger_time <= 0.0 and gun != null:
		gun.burst()


## Remaining power: share of the cores still whole (1 … 0).
func power() -> float:
	var cores := parts.total("core")
	return float(parts.alive("core")) / cores if cores > 0 else 1.0


## Rear up and slam the front feet down now (if not already attacking).
func stomp_now() -> void:
	if not busy():
		stomp.start()


func busy() -> bool:
	if collapsed or _stagger_time > 0.0:
		return true # nothing more to start
	return laser.busy() or stomp.busy()


## Can the walking legs hold the body up? At least STAND_LEGS of them, and the body's centre
## inside the convex hull of their relaxed foot spots (body space, from above) with a margin.
func can_stand() -> bool:
	var feet := PackedVector2Array()
	for leg in spider.rig.layout.legs:
		if leg.walks:
			feet.append(Vector2(leg.rest_foot().x, leg.rest_foot().z))
	if feet.size() < STAND_LEGS:
		return false
	return _inside_hull(Geometry2D.convex_hull(feet), Vector2.ZERO, BALANCE_MARGIN)


# True if `point` is inside the closed convex polygon `hull` and at least `margin` from every edge.
static func _inside_hull(hull: PackedVector2Array, point: Vector2, margin: float) -> bool:
	if hull.size() < 4: # convex_hull repeats the first point at the end: a triangle has 4
		return false
	var winding := 0.0
	for i in hull.size() - 1:
		winding += hull[i].cross(hull[i + 1])
	for i in hull.size() - 1:
		var edge := hull[i + 1] - hull[i]
		var side := edge.cross(point - hull[i]) / edge.length() # signed distance to the edge line
		if side * signf(winding) < margin:
			return false
	return true


## Where the body falls (body space, flat): away from the feet that are left, toward the lost side.
func _fall_direction() -> Vector2:
	var center := Vector2.ZERO
	var count := 0
	for leg in spider.rig.layout.legs:
		if leg.walks:
			center += Vector2(leg.rest_foot().x, leg.rest_foot().z)
			count += 1
	if count == 0 or center.length() < 0.01:
		return Vector2.from_angle(randf() * TAU)
	return -center.normalized()


## Can't stand: everything stops, every leg goes limp and splays out, the body drops onto the
## ground (the sphere resting on its belly, tilted toward the side it fell to) and the eye flickers out.
func collapse() -> void:
	if collapsed:
		return
	var fall := _fall_direction() # (x right, y = z back), before the legs go limp
	collapsed = true
	_down_time = 0.0
	stomp.cancel()
	laser.stop()
	_release_buckle()
	_stagger_time = 0.0
	if gun != null:
		gun.enabled = false
	var layout := spider.rig.layout
	for leg in layout.legs.size():
		var definition := layout.legs[leg]
		definition.walks = false
		definition.hold = definition.hip + definition.out * FLOP.x + Vector3.DOWN * FLOP.y
		if leg < layout.wave_phases.size():
			layout.wave_phases[leg] = -1.0
	spider.refresh_gait()
	spider.follow_feet = false # nothing to stand on: the body height is set below
	spider.body_pitch_offset = DOWN_TILT * fall.y # falling forward (−z) = nose down
	spider.body_roll_offset = -DOWN_TILT * fall.x # falling right = right side down
	spider.kick_body(Vector3(-4.0, fall.y * 2.0, -fall.x * 2.0)) # it topples, not just sinks
	sparks.burst(spider.rig.skeleton.global_position, Vector3.UP, 50, 7.0, 1.0, 0.8, 0.05, spider.global_position.y)


## Back to six whole legs.
func repair() -> void:
	stomp.cancel()
	laser.stop()
	missiles.clear()
	_release_buckle()
	_stagger_time = 0.0
	collapsed = false
	spider.follow_feet = true
	spider.body_height_offset = 0.0
	spider.body_pitch_offset = 0.0
	spider.body_roll_offset = 0.0
	eye.power = 1.0
	spider.layout_override = OrbLayout.make()
	spider.rebuild() # → rebuilt → _dress (new eye, joints, weak spots)
	_apply_phases()


func status() -> String:
	if collapsed:
		return "DOWN — collapsed (K repairs)"
	var state := "REELING" if _stagger_time > 0.0 else "laser %s   stomp %s" % [laser.status() if not eye.broken else "BROKEN", stomp.status()]
	return "%s   cores %d/%d   pods %d/%d   gun %s   menace %.2f" % [state, parts.alive("core"), parts.total("core"),
		parts.alive("pod"), parts.total("pod"), "ok" if parts.alive("gun") > 0 else "BROKEN", _menace]


func _process(delta: float) -> void:
	_time += delta
	if collapsed:
		_lie_down(delta)
		return
	parts.floor_y = spider.global_position.y
	if _stagger_time > 0.0:
		_reel(delta)
		_apply_speed()
		return
	stomp.update(delta)
	var attacking := busy()
	var goal := 1.0 if attacking else calm_menace
	_menace = move_toward(_menace, goal, MENACE_EASE * delta * (3.0 if attacking else 1.0))
	eye.menace = _menace
	if target != null:
		if not laser.busy(): # the laser steers the eye itself
			eye.look_target = target.global_position + Vector3.UP * 0.5
		_steer(attacking)
		if auto_attack and not attacking:
			_attack_wait -= delta
			if _attack_wait <= 0.0 and _distance() < ATTACK_RANGE:
				_choose_attack()
				_attack_wait = randf_range(ATTACK_GAP.x, ATTACK_GAP.y) / _power_scale()
		if gun_auto and gun != null and gun.enabled:
			_gun_wait -= delta
			if _gun_wait <= 0.0 and _distance() < GUN_RANGE and not stomp.busy():
				gun_now()
				_gun_wait = randf_range(GUN_GAP.x, GUN_GAP.y) / _power_scale()
	_apply_speed()


# A random pick from the attacks it can still make at this range.
func _choose_attack() -> void:
	var options: Array[String] = []
	if not eye.broken:
		options.append("laser")
	if _distance() < STOMP_RANGE:
		options.append("stomp")
	if parts.alive("pod") > 0 and _distance() > MISSILE_MIN and not missiles.busy():
		options.append("missiles")
	if options.is_empty():
		return
	match options.pick_random():
		"laser": attack()
		"stomp": stomp_now()
		"missiles": missiles_now()


# Slower with less power: 1 with every core, 0.5 with one left.
func _power_scale() -> float:
	return 0.5 + 0.5 * power()


# Reeling after a leg buckled: no walking or attacking; the body dips toward that leg, whose foot
# skids out, then everything eases back as the time runs out.
func _reel(delta: float) -> void:
	_stagger_time = maxf(_stagger_time - delta, 0.0)
	var t := _stagger_time / STAGGER_TIME # 1 → 0
	var depth := smoothstep(0.0, 0.3, t) # holds low, then rises over the last third
	spider.steer = Vector2.ZERO
	spider.steer_turn = 0.0
	if _buckled >= 0:
		var hip := spider.rig.layout.legs[_buckled].hip
		spider.body_height_offset = STAGGER_DIP.x * depth
		spider.body_pitch_offset = STAGGER_DIP.y * depth * signf(hip.z) # front leg: nose down
		spider.body_roll_offset = -STAGGER_DIP.z * depth * signf(hip.x) # right leg: right side down
	eye.look_target = spider.rig.skeleton.global_position + Vector3(sin(_time * 9.0), -0.3, cos(_time * 7.0)) * 5.0 # dazed
	if _stagger_time <= 0.0:
		_release_buckle()


func _release_buckle() -> void:
	if _buckled >= 0 and _buckled < spider.legs.size():
		spider.legs[_buckled].release_override()
	_buckled = -1
	if not collapsed:
		spider.body_height_offset = 0.0
		spider.body_pitch_offset = 0.0
		spider.body_roll_offset = 0.0


# Down: no walking or turning, the body resting on the ground (its centre a sphere radius above
# the root, which follows the ground), the eye flickering out over a few seconds.
func _lie_down(delta: float) -> void:
	spider.speed_scale = 0.0
	spider.steer = Vector2.ZERO
	spider.steer_turn = 0.0
	spider.body_height_offset = OrbLayout.RADIUS * 0.97 - spider.ride_height - spider.rig.layout.relaxed_height()
	_drape_legs()
	eye.charge = 0.0
	_down_time += delta
	var fade := clampf(1.0 - _down_time / EYE_FADE, 0.08, 1.0)
	var flicker := 1.0 if fade <= 0.08 or fmod(_time * 7.0, 1.0) < 0.6 else 0.25
	eye.power = fade * flicker
	if target != null:
		eye.look_target = target.global_position


# Limp legs reach for the ground around the fallen body: each leg's hold spot is FLOP.x out from
# its hip, on the ground (the root's height), turned back into the tilted body's space — so the
# legs on the high side hang down to the ground instead of sticking up with the tilt.
func _drape_legs() -> void:
	var to_body := spider.rig.skeleton.global_transform.affine_inverse()
	var root := spider.global_transform
	for definition in spider.rig.layout.legs:
		var spot := definition.hip + definition.out * FLOP.x
		definition.hold = to_body * (root * Vector3(spot.x, 0.05, spot.z))


# Walking speed: the spider's speed_scale, with the wave gait's cycle limits scaled to match (a
# faster walk steps faster rather than taking strides longer than the legs reach).
func _apply_speed() -> void:
	spider.speed_scale = 0.0 if paused or _stagger_time > 0.0 else walk_speed * _power_scale()
	spider.rig.layout.wave_period = OrbLayout.WAVE_PERIOD / maxf(walk_speed, 0.1)


# Rewrite the layout's per-leg phases from pair_delay / side_offset and hand them to the gait.
func _apply_phases() -> void:
	if spider == null or spider.rig == null:
		return
	var layout := spider.rig.layout
	for leg in layout.legs.size():
		if not layout.legs[leg].walks: # broken off
			layout.wave_phases[leg] = -1.0
			continue
		layout.wave_phases[leg] = OrbLayout.wave_phase(OrbLayout.leg_pair(leg), OrbLayout.leg_side(leg), pair_delay, side_offset)
	spider.wave_gait = spider.wave_gait # its setter re-reads the phases


func _distance() -> float:
	return ((target.global_position - spider.global_position) * Vector3(1, 0, 1)).length()


# Face the target and keep FIGHT_RANGE from it; plant and only turn slowly while attacking.
func _steer(attacking: bool) -> void:
	var to := (target.global_position - spider.global_position) * Vector3(1, 0, 1)
	var facing := -spider.global_basis.z
	var angle := facing.signed_angle_to(to, Vector3.UP) if to.length() > 0.1 else 0.0
	var turn := clampf(angle * 1.5, -TURN_RATE, TURN_RATE)
	var forward := clampf((to.length() - FIGHT_RANGE) / 3.0, -0.6, 1.0)
	forward *= clampf(1.0 - absf(angle) / 1.2, 0.0, 1.0) # turn first, then walk
	if attacking:
		forward = 0.0
		turn *= 0.3
	spider.steer = Vector2(0.0, forward)
	spider.steer_turn = turn


# A joint was shot: the eye flinches and it gets angrier.
func _on_joint_hit(index: int) -> void:
	_menace = minf(_menace + 0.25, 1.0)
	var away := (limbs.joint_position(index) - spider.rig.skeleton.global_position).normalized()
	eye.jolt(-away * 3.0)


# A leg's joints took enough hits: it buckles and the warden reels (stops attacking, dips toward
# that leg, its foot skids out, the eye goes dazed).
func _on_leg_buckled(leg: int) -> void:
	if collapsed:
		return
	stomp.cancel()
	laser.stop()
	if gun != null:
		gun.stop()
	_release_buckle()
	_stagger_time = STAGGER_TIME
	_buckled = leg
	var gait_leg := spider.legs[leg]
	gait_leg.begin_override()
	var out := spider.rig.skeleton.global_basis * spider.rig.layout.legs[leg].out
	gait_leg.override_position = gait_leg.planted + (out * Vector3(1, 0, 1)).normalized() * BUCKLE_SPLAY
	var hip := spider.rig.layout.legs[leg].hip
	spider.kick_body(Vector3(-4.0, -signf(hip.z) * 1.2, -signf(hip.x) * 1.5))
	_menace = 1.0
	eye.jolt(Vector3(randf_range(-4, 4), 3.0, randf_range(-4, 4)))


# A weak spot was hit: flinch, get angrier; the eye flashes when its lens is the one hit.
func _on_part_hit(part: OrbParts.Part) -> void:
	_menace = minf(_menace + 0.2, 1.0)
	if part.kind == "laser":
		eye.hurt = 0.12
	var away := (part.position() - spider.rig.skeleton.global_position).normalized()
	eye.jolt(-away * 2.0)


# A weak spot broke: that weapon stops; a core lost slows it (power); no cores left: shutdown.
func _on_part_broken(part: OrbParts.Part) -> void:
	_menace = 1.0
	var away := (part.position() - spider.rig.skeleton.global_position)
	var local := spider.rig.skeleton.global_basis.inverse() * away.normalized()
	spider.kick_body(Vector3(-2.0, local.z * 1.0, -local.x * 1.2))
	match part.kind:
		"laser":
			eye.broken = true
			laser.stop()
		"gun":
			gun.enabled = false
		"core":
			if parts.alive("core") == 0:
				collapse()


# A leg came off (only with OrbLimbs.breakable): the body lurches toward the lost side and any
# stomp is called off.
func _on_leg_broken(leg: int, _joint: int) -> void:
	_menace = 1.0
	if stomp.busy():
		stomp.cancel()
	var hip := spider.rig.layout.legs[leg].hip
	spider.kick_body(Vector3(-2.5, -signf(hip.z) * 0.8, signf(hip.x) * -1.2))
	if not can_stand():
		collapse()


# The shell, eye and joint balls on the (re)built spider body.
func _dress() -> void:
	var body := spider.rig.skeleton
	var shell := SphereMesh.new()
	shell.radius = OrbLayout.RADIUS
	shell.height = OrbLayout.RADIUS * 2.0
	shell.radial_segments = 48
	shell.rings = 24
	body.add_child(OrbEye._mesh(shell, Transform3D.IDENTITY, OrbEye._material(SHELL, 0.38, 0.85)))
	var belt := TorusMesh.new()
	belt.inner_radius = OrbLayout.RADIUS * 0.97
	belt.outer_radius = OrbLayout.RADIUS * 1.06
	belt.rings = 48
	var dark := OrbEye._material(BELT, 0.5, 0.7)
	body.add_child(OrbEye._mesh(belt, Transform3D(Basis.IDENTITY, Vector3(0, -OrbLayout.HIP_DROP, 0)), dark))
	var crown := TorusMesh.new() # a smaller ring on top
	crown.inner_radius = 0.55
	crown.outer_radius = 0.68
	body.add_child(OrbEye._mesh(crown, Transform3D(Basis.IDENTITY, Vector3(0, OrbLayout.RADIUS * 0.92, 0)), dark))
	for leg in spider.rig.layout.legs: # a socket where each leg leaves the sphere
		var socket := CylinderMesh.new()
		socket.top_radius = 0.3
		socket.bottom_radius = 0.36
		socket.height = 0.3
		var y := leg.out # the cylinder's axis points out of the sphere
		var x := y.cross(Vector3.UP).normalized()
		body.add_child(OrbEye._mesh(socket, Transform3D(Basis(x, y, x.cross(y)), leg.hip), dark))
	eye = OrbEye.new()
	eye.name = "Eye"
	eye.radius = OrbLayout.RADIUS
	body.add_child(eye)
	laser.eye = eye
	limbs.rebuild()
	OrbArmory.build(self, body) # weak spots (sets gun)
	gun.bolt_hit.connect(func(direction: Vector3) -> void: gun_hit.emit(direction))
