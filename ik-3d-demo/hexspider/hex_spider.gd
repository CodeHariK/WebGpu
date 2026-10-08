## The hex spider: a body on N legs. Each leg has two hexagons:
##   foot hex   on the ground round the leg's rest spot (moves with the body). When the planted
##              foot ends up outside it (× step_trigger), the leg steps: the hex is placed where the
##              rest spot will be `lead` seconds ahead, each corner gets one ray down, bad corners are
##              rejected (HexPicker) and the foot swings to one of the front corners.
##   pole hex   in the air above and outside the hip: where the knee points. Each step picks one of
##              its corners (a slightly different knee every step), and the pole rises by pole_lift
##              while the foot is in the air and sinks back when it lands — the knee lifts with it.
## Gait: two alternating groups (a tripod for 6 legs, diagonal pairs for 4). When a group has
## landed and any leg of the other group wants to step, the whole other group steps together —
## a steady rhythm instead of each foot going on its own.
## Body: height = average foot height + ride_height; pitch / roll from the feet. Legs: two-bone solve.
## Controls (when `player` is true): W/S forward/back, A/D turn. Nothing pressed + auto_walk: circles.
class_name HexSpider
extends Node3D

@export_range(4, 8, 2) var leg_count := 6
@export var body_radius := 0.45
@export var leg_reach := 1.15 ## rest spot distance from the body centre
@export var ride_height := 0.55
@export var upper_length := 0.75
@export var lower_length := 0.85

@export_group("Foot hex")
@export var rings := HexPicker.Rings.HEX
@export var ring_radius := 0.3 ## foot hex size
@export var step_trigger := 1.0 ## step when the foot is this × ring_radius from its rest spot
@export var weighted := true ## only the front corners (off: any corner, pure random)
@export var forward_bias := 1.0
@export var randomness := 0.3
@export var max_slope := 40.0 ## degrees
@export var max_step := 0.8 ## metres above / below the body's ground
@export var min_gap := 0.3 ## metres between feet

@export_group("Pole hex")
@export var pole_up := 0.55 ## the pole hex sits this far above the hip …
@export var pole_out := 0.45 ## … and this far outward
@export var pole_radius := 0.14 ## pole hex size
@export var pole_lift := 0.3 ## the pole rises this much while the foot is in the air

@export_group("Step")
@export var step_time := 0.22
@export var step_height := 0.22
@export var lead := 0.3 ## seconds: the hex is placed where the rest spot will be this far ahead
@export var settle_after := 0.6 ## standing still this long: feet step back near their rest spots

@export_group("Move")
@export var move_speed := 1.6
@export var turn_speed := 1.6
@export var auto_walk := true
@export var player := true

var legs: Array[HexLeg] = []
var velocity := Vector3.ZERO
var ground_y := 0.0
var body: Node3D ## the visual body (tilts; this node stays upright)
var stuck := 0 ## legs that found nowhere to step on their last try

var _rng := RandomNumberGenerator.new()
var _still_for := 0.0
var _group := 0 ## the group that stepped last
var _bones: Array[MeshInstance3D] = []
var _feet: Array[MeshInstance3D] = []
var _ready_done := false


func _ready() -> void:
	_rng.seed = 7
	_build_legs()
	_build_visuals()
	await get_tree().physics_frame # colliders are in the physics world from now on
	var under := _ray_down(global_position)
	ground_y = under.y if under != Vector3.INF else 0.0
	for leg in legs:
		var hit := _ray_down(_rest_world(leg))
		leg.foot = hit if hit != Vector3.INF else _rest_world(leg)
	_ready_done = true


func _physics_process(delta: float) -> void:
	if not _ready_done:
		return
	_move(delta)
	_update_legs(delta)
	_pose_body(delta)
	_solve_legs()
	_place_visuals()


## The settings HexPicker needs (rebuilt each pick, so inspector edits apply at once).
func settings() -> Dictionary:
	return {
		"ring_radius": ring_radius, "rings": rings, "yaw": -rotation.y, "weighted": weighted,
		"forward_bias": forward_bias, "randomness": randomness, "max_slope": max_slope,
		"max_step": max_step, "min_gap": min_gap,
	}


# --- the body ----------------------------------------------------------------------------------

func _move(delta: float) -> void:
	var throttle := 0.0
	var turn := 0.0
	if player:
		throttle = float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP)) - float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN))
		turn = float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)) - float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT))
	if throttle == 0.0 and turn == 0.0 and auto_walk:
		throttle = 1.0
		turn = 0.35
	rotation.y += turn * turn_speed * delta
	velocity = -global_basis.z * throttle * move_speed
	global_position += velocity * delta
	# Height: ride above the average foot.
	var sum := 0.0
	for leg in legs:
		sum += leg.foot.y
	ground_y = sum / legs.size()
	global_position.y = lerpf(global_position.y, ground_y + ride_height, 1.0 - exp(-8.0 * delta))


func _pose_body(delta: float) -> void:
	var front := _average_height(func(leg: HexLeg) -> bool: return leg.rest_local.z < 0.0)
	var back := _average_height(func(leg: HexLeg) -> bool: return leg.rest_local.z >= 0.0)
	var right := _average_height(func(leg: HexLeg) -> bool: return leg.side > 0)
	var left := _average_height(func(leg: HexLeg) -> bool: return leg.side < 0)
	var pitch := atan2(front - back, leg_reach * 1.2)
	var roll := atan2(right - left, leg_reach * 2.0)
	var target := Basis.from_euler(Vector3(pitch, 0.0, roll))
	body.basis = body.basis.slerp(target, 1.0 - exp(-6.0 * delta))


func _average_height(which: Callable) -> float:
	var sum := 0.0
	var count := 0
	for leg in legs:
		if which.call(leg):
			sum += leg.foot.y
			count += 1
	return sum / maxf(count, 1)


# --- the legs ----------------------------------------------------------------------------------

func _update_legs(delta: float) -> void:
	var moving := velocity.length() > 0.05
	_still_for = 0.0 if moving else _still_for + delta
	var in_air := 0
	for leg in legs:
		if leg.stepping:
			leg.swing(delta, step_time, step_height)
		else:
			leg.planted_for += delta
		if leg.stepping:
			in_air += 1
	if in_air > 0: # a group is in the air; only a leg left far behind (a sharp turn) may join
		for leg in legs:
			if not leg.stepping and _off(leg) > ring_radius * step_trigger * 2.5:
				_try_step(leg, moving)
		return
	# All down: the other group goes next if any of its legs wants to (else this group again).
	var next := 1 - _group
	if not _group_wants(next):
		if not _group_wants(_group):
			return
		next = _group
	var stepped := false
	for leg in legs:
		if leg.group == next and (moving or _wants(leg)) and _try_step(leg, moving):
			stepped = true
	if stepped:
		_group = next


func _wants(leg: HexLeg) -> bool:
	var off := _off(leg)
	return off > ring_radius * step_trigger or (_still_for > settle_after and off > ring_radius * 0.4)


func _group_wants(group: int) -> bool:
	for leg in legs:
		if leg.group == group and _wants(leg):
			return true
	return false


# How far the planted foot is from its rest spot (flat).
func _off(leg: HexLeg) -> float:
	return _flat_distance(leg.foot, _rest_world(leg))


func _try_step(leg: HexLeg, moving: bool) -> bool:
	var ahead := velocity * lead if moving else Vector3.ZERO
	var centre := _rest_world(leg) + Vector3(ahead.x, 0.0, ahead.z)
	centre.y = ground_y
	var others: Array[Vector3] = []
	for other in legs:
		if other != leg:
			others.append(other.step_to if other.stepping else other.foot)
	var move_dir := Vector3(velocity.x, 0.0, velocity.z).normalized() if moving else Vector3.ZERO
	var index := HexPicker.pick(get_world_3d().direct_space_state, leg, centre, ground_y, move_dir, others, settings(), _rng)
	if index < 0:
		return false
	var corner := HexPicker._flat(_rng.randi_range(0, 5) * PI / 3.0) * pole_radius # body space
	leg.start_step(leg.candidates[index].hit, corner)
	return true


## The centre of the leg's pole hex, body space: above and outside the hip.
func pole_centre_local(leg: HexLeg) -> Vector3:
	var out := Vector3(leg.hip_local.x, 0.0, leg.hip_local.z).normalized()
	return leg.hip_local + out * pole_out + Vector3.UP * pole_up


## How many legs found nowhere to step on their last try.
func stuck_count() -> int:
	var count := 0
	for leg in legs:
		if not leg.candidates.is_empty() and leg.chosen < 0:
			count += 1
	return count


# Two-bone solve per leg: hip on the tilted body, knee bent toward the leg's pole.
func _solve_legs() -> void:
	for leg in legs:
		var hip := body.global_transform * leg.hip_local
		leg.pole = body.global_transform * (pole_centre_local(leg) + leg.pole_corner_now()) + Vector3.UP * leg.pole_lift_now(pole_lift)
		var to_foot := leg.foot - hip
		var reach := upper_length + lower_length - 0.001
		var d := clampf(to_foot.length(), 0.05, reach)
		var dir := to_foot.normalized()
		var bend := leg.pole - hip
		bend = (bend - dir * bend.dot(dir)).normalized()
		var cos_a := clampf((upper_length * upper_length + d * d - lower_length * lower_length) / (2.0 * upper_length * d), -1.0, 1.0)
		var a := acos(cos_a)
		leg.knee = hip + dir * upper_length * cos(a) + bend * upper_length * sin(a)


# --- building ----------------------------------------------------------------------------------

# Legs fan out on both sides from 35° to 145° off forward. Groups alternate along each side and
# across, so a 6-legged spider walks in tripods and a 4-legged one in diagonal pairs.
func _build_legs() -> void:
	legs.clear()
	var per_side := leg_count / 2
	for side: int in [1, -1]:
		for j in per_side:
			var angle := deg_to_rad(lerpf(35.0, 145.0, (j + 0.5) / per_side))
			var dir := Vector3(sin(angle) * side, 0.0, -cos(angle))
			var leg := HexLeg.new()
			leg.side = side
			leg.hip_local = dir * body_radius * 0.9
			leg.rest_local = dir * leg_reach
			leg.group = (j + (0 if side > 0 else 1)) % 2
			legs.append(leg)


func _build_visuals() -> void:
	body = Node3D.new()
	body.name = "Body"
	add_child(body)
	var shell := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = body_radius
	sphere.height = body_radius * 1.4
	shell.mesh = sphere
	shell.material_override = _material(Color(0.25, 0.22, 0.3))
	body.add_child(shell)
	var eye := MeshInstance3D.new()
	var eye_mesh := SphereMesh.new()
	eye_mesh.radius = body_radius * 0.25
	eye_mesh.height = body_radius * 0.5
	eye.mesh = eye_mesh
	eye.position = Vector3(0, body_radius * 0.15, -body_radius * 0.85)
	eye.material_override = _material(Color(1.0, 0.35, 0.3), true)
	body.add_child(eye)
	var bone_mesh := CylinderMesh.new()
	bone_mesh.top_radius = 0.045
	bone_mesh.bottom_radius = 0.035
	bone_mesh.height = 1.0
	var foot_mesh := SphereMesh.new()
	foot_mesh.radius = 0.06
	foot_mesh.height = 0.12
	var leg_material := _material(Color(0.35, 0.32, 0.4))
	for i in legs.size() * 2:
		var bone := MeshInstance3D.new()
		bone.mesh = bone_mesh
		bone.material_override = leg_material
		bone.top_level = true
		add_child(bone)
		_bones.append(bone)
	for i in legs.size():
		var foot := MeshInstance3D.new()
		foot.mesh = foot_mesh
		foot.material_override = _material(Color(0.9, 0.85, 0.6))
		foot.top_level = true
		add_child(foot)
		_feet.append(foot)


func _place_visuals() -> void:
	for i in legs.size():
		var leg := legs[i]
		var hip := body.global_transform * leg.hip_local
		_place_bone(_bones[i * 2], hip, leg.knee)
		_place_bone(_bones[i * 2 + 1], leg.knee, leg.foot)
		_feet[i].global_position = leg.foot


func _place_bone(bone: MeshInstance3D, a: Vector3, b: Vector3) -> void:
	var span := b - a
	var length := maxf(span.length(), 0.001)
	var rotation_to := Quaternion(Vector3.UP, span / length) if length > 0.001 else Quaternion()
	bone.global_transform = Transform3D(Basis(rotation_to).scaled(Vector3(1.0, length, 1.0)), (a + b) * 0.5)


# --- helpers -----------------------------------------------------------------------------------

## The leg's rest spot in the world, on the body's ground height.
func _rest_world(leg: HexLeg) -> Vector3:
	var p := global_position + global_basis * Vector3(leg.rest_local.x, 0.0, leg.rest_local.z)
	return Vector3(p.x, ground_y, p.z)


func _ray_down(at: Vector3) -> Vector3:
	var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(at + Vector3.UP * 2.0, at + Vector3.DOWN * 3.0))
	return hit.position if not hit.is_empty() else Vector3.INF


static func _flat_distance(a: Vector3, b: Vector3) -> float:
	return Vector2(a.x - b.x, a.z - b.z).length()


static func _material(color: Color, glow := false) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.7
	if glow:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = 2.0
	return material
