## The gait spider: legs stepped by a clock (GaitPattern), not by "this foot is too far".
##   clock     one shared cycle. Its speed follows the walking speed so each foot covers about
##             `stride` metres while it's on the ground: faster walking = faster steps. Standing
##             still, it keeps ticking only until every foot is back near its rest spot.
##   when      each leg is in the air while its own phase (clock + offset) is in the swing part of
##             the cycle — the pattern decides the rhythm (tetrapod + wave, ripple, tripod…).
##   where     the foot aims for where its rest spot will be half-way through its next time on the
##             ground (so it lands ahead and is pushed back past the rest spot), turning included.
##             One ray down finds the floor there; with no floor (a pit) it takes the nearest spot
##             on two small rings round it that has one. It re-aims during the first part of the swing.
## Body: low, height = average foot height + ride_height, pitch / roll from the feet. Legs: two
## bones, the knee arched up and out (spider knees stand above the body). Pairs differ: the
## front and back pairs are longer and reach further forward / back; the front pair lifts higher.
## Controls (when `player`): W/S forward/back, A/D turn; nothing pressed + auto_walk: circles.
class_name GaitSpider
extends Node3D

const PAIR_ANGLES_8: Array[float] = [32.0, 72.0, 110.0, 148.0] ## degrees off forward
const PAIR_ANGLES_6: Array[float] = [40.0, 90.0, 140.0]
const PAIR_SIZE_8: Array[float] = [1.2, 1.0, 1.0, 1.15] ## leg length and reach scale
const PAIR_SIZE_6: Array[float] = [1.1, 1.0, 1.1]
const PAIR_LIFT_8: Array[float] = [1.5, 1.0, 1.0, 1.1] ## front legs lift higher (they feel ahead)
const PAIR_LIFT_6: Array[float] = [1.3, 1.0, 1.0]

@export var preset := GaitPattern.Preset.SPIDER
@export var reach := 1.3 ## rest spot distance from the hip (× pair size)
@export var femur := 0.8
@export var tibia := 1.05
@export var ride_height := 0.32
@export var lift := 0.22
@export var stride := 0.8 ## metres a foot travels backward (relative to the body) while down
@export var min_period := 0.35 ## seconds per cycle at most this fast …
@export var max_period := 1.6 ## … and at least this fast while walking
@export var settle_period := 0.9
@export var knee_up := 0.7 ## the knee aims up this much …
@export var knee_out := 0.6 ## … and outward this much (spider legs arch up and out)

@export_group("Move")
@export var move_speed := 1.4
@export var turn_speed := 1.4
@export var auto_walk := true
@export var player := true

var pattern := GaitPattern.new()
var legs: Array[GaitLeg] = []
var velocity := Vector3.ZERO
var angular_velocity := 0.0 ## rad/s about up
var clock := 0.0
var period := 1.0
var ground_y := 0.0
var body: Node3D ## the visual body (tilts; this node stays upright)

var _visuals: Array[Node3D] = []
var _ready_done := false


func _ready() -> void:
	var count := pattern.apply(preset)
	_build_legs(count)
	_build_visuals()
	await get_tree().physics_frame # colliders are in the physics world from now on
	var under := _ray_down(global_position)
	ground_y = under.y if under != Vector3.INF else 0.0
	for leg in legs:
		leg.foot = _floor_near(_rest_world(leg, global_position, rotation.y), leg)
	_ready_done = true


func _physics_process(delta: float) -> void:
	if not _ready_done:
		return
	_move(delta)
	_advance_clock(delta)
	_update_legs()
	_pose_body(delta)
	_solve_legs()
	_place_visuals()


# --- moving and the clock -------------------------------------------------------------------------

func _move(delta: float) -> void:
	var throttle := 0.0
	var turn := 0.0
	if player:
		throttle = float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN))
		turn = float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)) - float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT))
	if throttle == 0.0 and turn == 0.0 and auto_walk:
		throttle = 1.0
		turn = 0.35
	angular_velocity = turn * turn_speed
	rotation.y += angular_velocity * delta
	velocity = -global_basis.z * throttle * move_speed
	global_position += velocity * delta
	var sum := 0.0
	for leg in legs:
		sum += leg.foot.y
	ground_y = sum / legs.size()
	global_position.y = lerpf(global_position.y, ground_y + ride_height, 1.0 - exp(-8.0 * delta))


## Walking: the cycle takes as long as it needs for a foot to cover `stride` while it's down.
## Standing: tick slowly while any foot is away from its rest spot (it steps back), then stop.
func _advance_clock(delta: float) -> void:
	var speed := velocity.length() + absf(angular_velocity) * reach
	if speed > 0.05:
		period = clampf(stride / speed / pattern.duty, min_period, max_period)
	elif _needs_settling() or _any_swinging():
		period = settle_period
	else:
		return
	clock = fposmod(clock + delta / period, 1.0)


func _needs_settling() -> bool:
	for leg in legs:
		if _flat_distance(leg.foot, _rest_world(leg, global_position, rotation.y)) > 0.1:
			return true
	return false


func _any_swinging() -> bool:
	for leg in legs:
		if leg.swinging:
			return true
	return false


func is_moving() -> bool:
	return velocity.length() > 0.05 or absf(angular_velocity) > 0.05


# --- the legs ----------------------------------------------------------------------------------

func _update_legs() -> void:
	var swing := pattern.swing()
	for leg in legs:
		var phase := pattern.leg_phase(clock, leg.pair, leg.side)
		var in_air := phase < swing
		if in_air and not leg.swinging:
			if not is_moving() and not _needs_settling():
				continue # standing and settled: don't lift
			leg.swinging = true
			leg.lift_off = leg.foot
			leg.progress = 0.0
		if not leg.swinging:
			continue
		if not in_air or phase / swing < leg.progress: # its swing time is over (or wrapped): land
			leg.foot = leg.target
			leg.swinging = false
			continue
		leg.progress = phase / swing
		if leg.progress < 0.6:
			leg.target = _landing(leg)
		leg.foot = leg.swing_position()


## Where the foot should land: its rest spot as it will be half-way through its next time on the
## ground (the body keeps moving and turning meanwhile), dropped onto the floor.
func _landing(leg: GaitLeg) -> Vector3:
	var ahead := 0.0
	if is_moving():
		ahead = pattern.swing() * period * (1.0 - leg.progress) + pattern.duty * period * 0.5
	var at := global_position + velocity * ahead
	var yaw := rotation.y + angular_velocity * ahead
	return _floor_near(_rest_world(leg, at, yaw), leg)


## The floor under `spot`; with none there, the nearest point on two small rings round it that has
## floor (leg.landed_ok = false); with none at all, the spot at ground height.
func _floor_near(spot: Vector3, leg: GaitLeg) -> Vector3:
	var hit := _ray_down(spot)
	leg.landed_ok = hit != Vector3.INF
	if leg.landed_ok:
		return hit
	for radius: float in [0.3, 0.6]:
		for i in 8:
			var around := _ray_down(spot + Vector3(cos(i * TAU / 8.0), 0.0, sin(i * TAU / 8.0)) * radius)
			if around != Vector3.INF:
				return around
	return Vector3(spot.x, ground_y, spot.z)


## A leg's rest spot for the body standing at `at` turned `yaw`.
func _rest_world(leg: GaitLeg, at: Vector3, yaw: float) -> Vector3:
	var p := at + Basis(Vector3.UP, yaw) * Vector3(leg.rest_local.x, 0.0, leg.rest_local.z)
	return Vector3(p.x, ground_y, p.z)


func rest_world(leg: GaitLeg) -> Vector3:
	return _rest_world(leg, global_position, rotation.y)


func _pose_body(delta: float) -> void:
	var front := 0.0
	var back := 0.0
	var right := 0.0
	var left := 0.0
	var n := legs.size() / 2.0
	for leg in legs:
		if leg.pair < legs.size() / 4.0:
			front += leg.foot.y
		else:
			back += leg.foot.y
		if leg.side > 0:
			right += leg.foot.y
		else:
			left += leg.foot.y
	var pitch := atan2((front - back) / n, reach * 1.6)
	var roll := atan2((right - left) / n, reach * 2.0)
	body.basis = body.basis.slerp(Basis.from_euler(Vector3(pitch, 0.0, roll)), 1.0 - exp(-6.0 * delta))


# Two bones per leg; the knee aims high above the hip and a little outward.
func _solve_legs() -> void:
	for leg in legs:
		var hip := body.global_transform * leg.hip_local
		var to_foot := leg.foot - hip
		var d := clampf(to_foot.length(), 0.05, leg.upper + leg.lower - 0.001)
		var dir := to_foot.normalized()
		var out := Vector3(hip.x - global_position.x, 0.0, hip.z - global_position.z).normalized()
		var pole := Vector3.UP * knee_up + out * knee_out
		var bend := (pole - dir * pole.dot(dir)).normalized()
		var a := acos(clampf((leg.upper * leg.upper + d * d - leg.lower * leg.lower) / (2.0 * leg.upper * d), -1.0, 1.0))
		leg.knee = hip + dir * leg.upper * cos(a) + bend * leg.upper * sin(a)


# --- building ----------------------------------------------------------------------------------

## Rebuild with another gait preset (the leg count may change).
func set_preset(value: GaitPattern.Preset) -> void:
	preset = value
	var count := pattern.apply(preset)
	if count != legs.size():
		var feet := legs.size()
		_build_legs(count)
		_build_visuals()
		if feet > 0:
			for leg in legs:
				leg.foot = _floor_near(rest_world(leg), leg)


func _build_legs(count: int) -> void:
	legs.clear()
	var pairs := count / 2
	var angles: Array[float] = PAIR_ANGLES_8 if pairs == 4 else PAIR_ANGLES_6
	var sizes: Array[float] = PAIR_SIZE_8 if pairs == 4 else PAIR_SIZE_6
	var lifts: Array[float] = PAIR_LIFT_8 if pairs == 4 else PAIR_LIFT_6
	for side: int in [-1, 1]:
		for k in pairs:
			var a := deg_to_rad(angles[k])
			var dir := Vector3(sin(a) * side, 0.0, -cos(a))
			var leg := GaitLeg.new()
			leg.pair = k
			leg.side = side
			leg.name = "%s%d" % ["L" if side < 0 else "R", k + 1]
			leg.hip_local = Vector3(0.0, 0.0, -0.15) + dir * 0.18
			leg.rest_local = leg.hip_local + dir * reach * sizes[k]
			leg.rest_local.y = 0.0
			leg.upper = femur * sizes[k]
			leg.lower = tibia * sizes[k]
			leg.lift = lift * lifts[k]
			legs.append(leg)


func _build_visuals() -> void:
	for node in _visuals:
		node.queue_free()
	_visuals.clear()
	if body == null:
		body = Node3D.new()
		body.name = "Body"
		add_child(body)
		_add_blob(body, Vector3(0, 0, -0.15), Vector3(0.24, 0.17, 0.27), Color(0.08, 0.08, 0.1)) # head part
		_add_blob(body, Vector3(0, 0.06, 0.38), Vector3(0.3, 0.25, 0.42), Color(0.95, 0.4, 0.12)) # abdomen
		for x: float in [-0.06, 0.06]:
			_add_blob(body, Vector3(x, 0.07, -0.38), Vector3(0.03, 0.03, 0.03), Color(1.0, 0.25, 0.2), true) # eyes
	for leg in legs:
		_visuals.append(_add_bone(0.045, Color(0.95, 0.42, 0.12))) # femur
		_visuals.append(_add_bone(0.03, Color(0.08, 0.08, 0.1))) # tibia


func _place_visuals() -> void:
	for i in legs.size():
		var leg := legs[i]
		var hip := body.global_transform * leg.hip_local
		_place_bone(_visuals[i * 2], hip, leg.knee)
		_place_bone(_visuals[i * 2 + 1], leg.knee, leg.foot)


func _add_blob(parent: Node3D, at: Vector3, radii: Vector3, color: Color, glow := false) -> void:
	var mesh := SphereMesh.new()
	mesh.radius = 1.0
	mesh.height = 2.0
	mesh.radial_segments = 12
	mesh.rings = 6
	var blob := MeshInstance3D.new()
	blob.mesh = mesh
	blob.position = at
	blob.scale = radii
	blob.material_override = _material(color, glow)
	parent.add_child(blob)


func _add_bone(radius: float, color: Color) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius * 0.6
	mesh.height = 1.0
	mesh.radial_segments = 6
	var bone := MeshInstance3D.new()
	bone.mesh = mesh
	bone.material_override = _material(color)
	bone.top_level = true
	add_child(bone)
	return bone


# A unit-height cylinder stretched from a to b (its top end at a).
func _place_bone(bone: Node3D, a: Vector3, b: Vector3) -> void:
	var span := a - b
	var length := maxf(span.length(), 0.001)
	bone.global_transform = Transform3D(Basis(Quaternion(Vector3.UP, span / length)).scaled(Vector3(1.0, length, 1.0)), (a + b) * 0.5)


func _ray_down(at: Vector3) -> Vector3:
	var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(at + Vector3.UP * 1.5, at + Vector3.DOWN * 2.0))
	return hit.position if not hit.is_empty() else Vector3.INF


static func _flat_distance(a: Vector3, b: Vector3) -> float:
	return Vector2(a.x - b.x, a.z - b.z).length()


static func _material(color: Color, glow := false) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.55
	if glow:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = 2.0
	return material
