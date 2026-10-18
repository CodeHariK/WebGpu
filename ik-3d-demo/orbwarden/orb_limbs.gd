## The Orb Warden's leg joints: a glowing ball on every hip, knee and ankle.
##
## Stagger points (the default, `breakable` off): shots don't break anything. Each hit on a leg's
## joints fills that leg's stagger meter (it drains over time); the joints heat from cyan to white
## as it fills, and when it's full the leg buckles — `leg_buckled` — and the warden reels for a
## moment (a window to hit its weak spots). Then the meter empties and that leg can't buckle again
## for a few seconds.
##
## Breakable legs (`breakable` on — the earlier design, kept switched off): each joint has its
## own health. Shooting one flashes it and throws sparks; the glow shifts cyan → orange → flickering
## red as it weakens. When a joint breaks, everything below it falls off as tumbling physics
## pieces (on DEBRIS_LAYER, so the spider's ground rays ignore them), the stump stops being drawn
## past the break and smoulders, and the leg leaves the gait: it no longer walks, its wave phase is
## dropped and it curls up at the body (Spider.refresh_gait). The other legs carry on.
##
## Joint index = leg × 3 + joint (0 hip, 1 knee, 2 ankle).
class_name OrbLimbs
extends Node3D

signal joint_hit(index: int)
signal leg_broken(leg: int, joint: int)
signal leg_buckled(leg: int)

const GLOW := Color(0.2, 0.95, 1.0)
const HURT := Color(1.0, 0.4, 0.05)
const DYING := Color(1.0, 0.05, 0.02)
const SIZES := [0.24, 0.22, 0.17] ## hip, knee, ankle ball radius
const HEALTH := [6.0, 4.0, 3.0] ## hits to break a hip, knee, ankle
const PICK_SCALE := 1.8 ## a shot this close (× ball radius) to a joint hits it
const FLASH_TIME := 0.12
const DEBRIS_LIFE := 14.0
const DEBRIS_LAYER := 2 ## physics layer bit (1-based) the fallen pieces live on
const SMOULDER_GAP := 0.12 ## seconds between drips from a broken stump
const STAGGER_HITS := 6.0 ## hits on one leg's joints (in quick succession) that make it buckle
const STAGGER_DRAIN := 0.8 ## meter drained per second
const STAGGER_COOLDOWN := 4.0 ## seconds after a buckle before that leg can buckle again
const HOT := Color(1.0, 0.95, 0.75) ## a full meter: white-hot joints

var warden: OrbWarden
var sparks: OrbSparks
var breakable := false ## true = shots break legs off (the earlier design); false = stagger points

var _stagger := PackedFloat32Array() ## per leg, 0 … STAGGER_HITS
var _cooldown := PackedFloat32Array() ## per leg, seconds

var _balls: Array[MeshInstance3D] = []
var _materials: Array[StandardMaterial3D] = []
var _health := PackedFloat32Array()
var _flash := PackedFloat32Array()
var _broken: Array[bool] = []
var _stumps: Array[int] = [] ## joint indices where a leg broke off (they smoulder)
var _time := 0.0
var _smoulder := 0.0


## New balls for the (re)built spider, every joint whole again.
func rebuild() -> void:
	for ball in _balls:
		ball.queue_free()
	_balls.clear()
	_materials.clear()
	_stumps.clear()
	var count := warden.spider.rig.layout.legs.size() * 3
	_health.resize(count)
	_flash.resize(count)
	_flash.fill(0.0)
	_broken.resize(count)
	_broken.fill(false)
	_stagger.resize(count / 3)
	_stagger.fill(0.0)
	_cooldown.resize(count / 3)
	_cooldown.fill(0.0)
	for i in count:
		_health[i] = HEALTH[i % 3]
		var mesh := SphereMesh.new()
		mesh.radius = SIZES[i % 3]
		mesh.height = SIZES[i % 3] * 2.0
		var material := StandardMaterial3D.new()
		material.albedo_color = GLOW
		material.emission_enabled = true
		material.emission = GLOW
		var ball := MeshInstance3D.new()
		ball.mesh = mesh
		ball.material_override = material
		ball.top_level = true
		add_child(ball)
		_balls.append(ball)
		_materials.append(material)


func joint_count() -> int:
	return _balls.size()


func is_broken(index: int) -> bool:
	return _broken[index]


func joint_position(index: int) -> Vector3:
	return warden.spider.rig.joint_position(index / 3, index % 3)


## Legs still attached and walking.
func legs_left() -> int:
	var count := 0
	for leg in warden.spider.rig.layout.legs:
		if leg.walks:
			count += 1
	return count


## The unbroken joint a shot along `direction` from `from` hits first; −1 = none.
func pick(from: Vector3, direction: Vector3) -> int:
	var best := -1
	var nearest := INF
	var dir := direction.normalized()
	for i in _balls.size():
		if _broken[i]:
			continue
		var offset := joint_position(i) - from
		var along := offset.dot(dir)
		if along <= 0.0 or along >= nearest:
			continue
		if (offset - dir * along).length() < SIZES[i % 3] * PICK_SCALE:
			best = i
			nearest = along
	return best


## Damage a joint (a shot from `direction`). Returns true if it broke.
func hit(index: int, damage: float, direction: Vector3) -> bool:
	if index < 0 or _broken[index]:
		return false
	_flash[index] = FLASH_TIME
	var at := joint_position(index)
	if sparks != null:
		sparks.burst(at, -direction, 12, 7.0, 0.8, 0.35, 0.03, 0.0)
	joint_hit.emit(index)
	if not breakable:
		_add_stagger(index / 3, damage)
		return false
	_health[index] -= damage
	if _health[index] <= 0.0:
		break_leg(index / 3, index % 3)
		return true
	return false


## How full leg `leg`'s stagger meter is (0 … 1).
func stagger(leg: int) -> float:
	return _stagger[leg] / STAGGER_HITS


func _add_stagger(leg: int, amount: float) -> void:
	if _cooldown[leg] > 0.0:
		return
	_stagger[leg] += amount
	if _stagger[leg] >= STAGGER_HITS:
		_stagger[leg] = 0.0
		_cooldown[leg] = STAGGER_COOLDOWN
		if sparks != null:
			sparks.burst(joint_position(leg * 3 + 1), Vector3.UP, 24, 6.0, 1.0, 0.5, 0.035, 0.0)
		leg_buckled.emit(leg)


## Break leg `leg` at `joint`: that joint and everything below it go.
func break_leg(leg: int, joint: int) -> void:
	var spider := warden.spider
	var chain: LimbChain = (spider.rig as SpiderChainRig).chains[leg]
	for j in range(joint, 3):
		_broken[leg * 3 + j] = true
	_stumps.append(leg * 3 + joint)
	for s in range(joint, chain.segment_count()):
		_drop_segment(chain, s, spider.rig.skeleton.global_position)
	chain.shown_segments = joint
	var definition := spider.rig.layout.legs[leg]
	definition.walks = false
	definition.hold = stump_hold(definition, joint)
	if leg < spider.rig.layout.wave_phases.size():
		spider.rig.layout.wave_phases[leg] = -1.0
	spider.refresh_gait()
	if sparks != null:
		var at := chain.points[joint]
		sparks.burst(at, Vector3.UP, 40, 9.0, 1.0, 0.7, 0.04, 0.0)
		sparks.embers(at, 10, 3.0, 0.0)
	leg_broken.emit(leg, joint)


## Where a broken leg's stump hangs (body space): a short stump droops out and down from the
## hip; one still holding its shin dangles further out.
static func stump_hold(definition: SpiderLayout.LegDef, joint: int) -> Vector3:
	var out := 0.9 if joint <= 1 else 1.8
	return definition.hip + definition.out * out + Vector3.DOWN * 1.3


func _process(delta: float) -> void:
	_time += delta
	_smoulder -= delta
	var drip := _smoulder <= 0.0
	if drip:
		_smoulder = SMOULDER_GAP
	for leg in _stagger.size():
		_stagger[leg] = maxf(_stagger[leg] - STAGGER_DRAIN * delta, 0.0)
		_cooldown[leg] = maxf(_cooldown[leg] - delta, 0.0)
	for i in _balls.size():
		var ball := _balls[i]
		_flash[i] = maxf(_flash[i] - delta, 0.0)
		if _broken[i]:
			ball.visible = _stumps.has(i) # the break point: a dull red-hot stump end
			if ball.visible:
				ball.global_position = joint_position(i)
				ball.scale = Vector3.ONE * 0.7
				_set_glow(i, DYING.darkened(0.3), 1.5 + sin(_time * 9.0 + i))
				if drip and sparks != null and randf() < 0.5:
					sparks.burst(ball.global_position, Vector3.DOWN, 1, 1.0, 0.5, 0.9, 0.04, 0.0, Color(1.0, 0.6, 0.15))
			continue
		ball.visible = true
		ball.global_position = joint_position(i)
		var color := GLOW
		var energy := 2.5 + 1.2 * sin(_time * 3.0)
		if breakable:
			var health: float = _health[i] / HEALTH[i % 3]
			color = HURT.lerp(GLOW, health) if health > 0.4 else DYING.lerp(HURT, health / 0.4)
			if health <= 0.4 and fmod(_time * 13.0 + i, 1.0) < 0.35: # about to go: flickers
				energy *= 0.25
		else:
			var heat := stagger(i / 3)
			color = GLOW.lerp(HOT, heat)
			energy += 4.0 * heat
			if heat > 0.7 and fmod(_time * 15.0, 1.0) < 0.4: # nearly buckling: flickers
				energy *= 0.4
			if _cooldown[i / 3] > 0.0: # just buckled: dimmed while it recovers
				color = GLOW.darkened(0.5)
				energy *= 0.4
		color = color.lerp(Color.WHITE, _flash[i] / FLASH_TIME)
		_set_glow(i, color, energy + 6.0 * _flash[i] / FLASH_TIME)


func _set_glow(index: int, color: Color, energy: float) -> void:
	var material := _materials[index]
	material.albedo_color = color
	material.emission = color
	material.emission_energy_multiplier = energy


# Segment `s` of `chain` as a free-falling physics piece, flung away from the body.
func _drop_segment(chain: LimbChain, s: int, body_center: Vector3) -> void:
	var a := chain.points[s]
	var b := chain.points[s + 1]
	var length := a.distance_to(b)
	if length < 0.05:
		return
	var radius := chain.segment_radius(s)
	var piece := RigidBody3D.new()
	piece.collision_layer = 1 << (DEBRIS_LAYER - 1)
	piece.collision_mask = 1 # lands on the ground
	piece.mass = 4.0 * length
	var shape := CylinderShape3D.new()
	shape.radius = radius
	shape.height = length
	var collision := CollisionShape3D.new()
	collision.shape = shape
	piece.add_child(collision)
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = length
	var material := StandardMaterial3D.new()
	material.albedo_color = chain.color
	material.metallic = 0.6
	material.roughness = 0.5
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	piece.add_child(visual)
	var end_ball := SphereMesh.new() # the dead joint at its lower end, gone dark
	end_ball.radius = radius * 1.3
	end_ball.height = radius * 2.6
	var dead := StandardMaterial3D.new()
	dead.albedo_color = Color(0.1, 0.12, 0.13)
	var end := MeshInstance3D.new()
	end.mesh = end_ball
	end.material_override = dead
	end.position = Vector3.UP * length * 0.5 # segment_basis' +Y runs a → b: this is the b end
	piece.add_child(end)
	add_child(piece)
	piece.global_transform = Transform3D(chain.segment_basis(s).orthonormalized(), (a + b) * 0.5)
	var away := ((a + b) * 0.5 - body_center) * Vector3(1, 0, 1)
	away = away.normalized() if away.length() > 0.01 else Vector3.RIGHT
	piece.linear_velocity = away * randf_range(3.0, 5.0) + Vector3.UP * randf_range(2.0, 4.0)
	piece.angular_velocity = Vector3(randf_range(-5, 5), randf_range(-3, 3), randf_range(-5, 5))
	get_tree().create_timer(DEBRIS_LIFE).timeout.connect(piece.queue_free)
