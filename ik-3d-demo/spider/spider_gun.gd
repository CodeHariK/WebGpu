## A machine gun on the head (layout.head.gun, built by SpiderRig). Hold `trigger` (set it every
## frame you want to fire) and the barrels take turns firing at `fire_rate`. Every shot:
##   recoil   the barrel slides back on its own spring and snaps forward; the head is knocked
##            back and nose-up, with a random yaw/roll jolt, through SpiderSprings.kick_head —
##            so sustained fire shudders and walks off target while the springs fight back
##   shot     a glowing tracer flies out of the muzzle (a little random spread) and sparks where
##            it hits the ground or reaches the target
##   flash    a muzzle flash + light for a frame or two
##   casing   a brass casing is flung out to the side and tumbles on the ground (physics debris)
## The gun only aims where the head points: the head's springs do the aiming (look_target).
class_name SpiderGun
extends Node

signal target_hit(point: Vector3) ## a tracer reached the target (hook damage up here)

const GROUND_MASK := 1 ## tracers hit the ground on this physics layer
const CASING_LAYER := 2 ## casings sit here (off the spider's ground raycasts) and land on layer 1
const CASING_LIFETIME := 2.5
const TRACER_COLOR := Color(1.0, 0.75, 0.3)

@export var spider: Spider
@export var springs: SpiderSprings ## for the head recoil
@export var target: Node3D ## tracers that pass close to it spark on it (the "player")
@export var fire_rate := 14.0 ## shots per second (all barrels together)
@export var spread := 0.025 ## radians of random scatter per shot
@export var bullet_speed := 45.0 ## m/s
@export var max_range := 30.0 ## metres before a tracer fades out
@export_group("Recoil")
@export var recoil_push := 1.0 ## m/s the head is knocked back per shot
@export var recoil_nod := 3.0 ## rad/s the head's nose is kicked up per shot
@export var recoil_jolt := 1.2 ## rad/s random yaw/roll per shot
@export var barrel_kick := 3.0 ## m/s a barrel slides back per shot
@export var barrel_spring := Vector3(9.0, 0.45, 1.0) ## (f, ζ, r) the barrel's return spring

var trigger := false ## fire while true; cleared every frame, so hold it by setting it each frame
var shots_fired := 0
var target_hits := 0

var _cooldown := 0.0
var _next_barrel := 0
var _slides: Array[SecondOrder] = [] ## per barrel: slide offset (head space, +Z = back)
var _rests: Array[Vector3] = [] ## per barrel: its mount position
var _tracers: Array[Dictionary] = [] ## {node, velocity, life}
var _rng := RandomNumberGenerator.new()


func _ready() -> void:
	spider.rebuilt.connect(_setup)
	_setup()


## True when the current layout's head carries a gun.
func has_gun() -> bool:
	return spider.rig != null and not spider.rig.muzzles.is_empty()


## Angle (radians) between where the gun points and the direction to `point`.
func aim_error(point: Vector3) -> float:
	if not has_gun():
		return PI
	var head := spider.rig.head.global_transform
	var to_point := point - head.origin
	return (-head.basis.z).angle_to(to_point) if to_point.length() > 0.01 else 0.0


func _setup() -> void:
	_slides.clear()
	_rests.clear()
	_next_barrel = 0
	if not has_gun():
		return
	for barrel in spider.rig.gun_barrels:
		_slides.append(SecondOrder.new(barrel_spring.x, barrel_spring.y, barrel_spring.z))
		_rests.append(barrel.position)


func _physics_process(delta: float) -> void:
	_cooldown -= delta
	if trigger and has_gun() and spider.visible and not spider.airborne:
		while _cooldown <= 0.0:
			_fire()
			_cooldown += 1.0 / fire_rate
	else:
		_cooldown = maxf(_cooldown, 0.0) # don't bank shots while the trigger is up
	trigger = false
	_update_barrels(delta)
	_update_tracers(delta)


# One shot from the next barrel in turn.
func _fire() -> void:
	var barrel := _next_barrel
	_next_barrel = (_next_barrel + 1) % spider.rig.muzzles.size()
	var muzzle := spider.rig.muzzles[barrel]
	var forward := -muzzle.global_basis.z.normalized()
	var direction := forward.rotated(muzzle.global_basis.x.normalized(), _rng.randf_range(-spread, spread))
	direction = direction.rotated(muzzle.global_basis.y.normalized(), _rng.randf_range(-spread, spread))
	_spawn_tracer(muzzle.global_position, direction)
	_flash(muzzle)
	_eject_casing(spider.rig.gun_barrels[barrel])
	_slides[barrel].kick(Vector3(0, 0, barrel_kick))
	if springs != null:
		springs.kick_head(-forward * recoil_push, recoil_nod, _rng.randf_range(-1, 1) * recoil_jolt, _rng.randf_range(-1, 1) * recoil_jolt)
	shots_fired += 1


func _update_barrels(delta: float) -> void:
	for i in _slides.size():
		var slide := _slides[i].update(delta, Vector3.ZERO)
		spider.rig.gun_barrels[i].position = _rests[i] + Vector3(0, 0, maxf(slide.z, -0.02)) # never pokes out forward much


# --- effects ---------------------------------------------------------------------------------

func _spawn_tracer(from: Vector3, direction: Vector3) -> void:
	var box := BoxMesh.new()
	box.size = Vector3(0.025, 0.025, 0.5)
	var tracer := MeshInstance3D.new()
	tracer.mesh = box
	tracer.material_override = _glow_material(TRACER_COLOR, 4.0)
	tracer.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_world().add_child(tracer)
	tracer.global_transform = Transform3D(Basis.looking_at(direction), from + direction * 0.25)
	_tracers.append({"node": tracer, "velocity": direction * bullet_speed, "life": max_range / bullet_speed})


# Move each tracer; the stretch it covers this frame is checked against the ground (raycast) and
# the target (closest point), and it sparks and vanishes on whichever it reaches first.
func _update_tracers(delta: float) -> void:
	var space := spider.get_world_3d().direct_space_state
	for i in range(_tracers.size() - 1, -1, -1):
		var tracer := _tracers[i]
		var node: MeshInstance3D = tracer.node
		var from := node.global_position
		var to: Vector3 = from + tracer.velocity * delta
		tracer.life -= delta
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, GROUND_MASK))
		var hit_point = hit.position if not hit.is_empty() else null
		if target != null:
			var closest := Geometry3D.get_closest_point_to_segment(target.global_position, from, to)
			if closest.distance_to(target.global_position) < 0.25:
				hit_point = closest
				target_hits += 1
				target_hit.emit(closest)
		if hit_point != null or tracer.life <= 0.0:
			if hit_point != null:
				_spark(hit_point)
			node.queue_free()
			_tracers.remove_at(i)
		else:
			node.global_position = to


func _flash(muzzle: Node3D) -> void:
	var sphere := SphereMesh.new()
	sphere.radius = 0.06
	sphere.height = 0.12
	var flash := MeshInstance3D.new()
	flash.mesh = sphere
	flash.material_override = _glow_material(Color(1.0, 0.85, 0.4), 6.0)
	flash.scale = Vector3(1.0, 1.0, 1.8) * _rng.randf_range(0.8, 1.4) # stretched along the barrel
	flash.position = Vector3(0, 0, -0.05)
	muzzle.add_child(flash)
	var light := OmniLight3D.new()
	light.light_color = Color(1.0, 0.7, 0.3)
	light.light_energy = 3.0
	light.omni_range = 3.0
	muzzle.add_child(light)
	for node: Node in [flash, light]:
		node.create_tween().tween_interval(0.04).finished.connect(node.queue_free)


func _spark(at: Vector3) -> void:
	var sphere := SphereMesh.new()
	sphere.radius = 0.5
	sphere.height = 1.0
	var spark := MeshInstance3D.new()
	spark.mesh = sphere
	var material := _glow_material(Color(1.0, 0.8, 0.4), 5.0)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	spark.material_override = material
	spark.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_world().add_child(spark)
	spark.global_position = at
	spark.scale = Vector3.ONE * 0.06
	var tween := spark.create_tween().set_parallel()
	tween.tween_property(spark, "scale", Vector3.ONE * 0.3, 0.12)
	tween.tween_property(material, "albedo_color:a", 0.0, 0.12)
	tween.chain().tween_callback(spark.queue_free)


# A brass casing flung out of the gun's outer side, up and a little back, tumbling.
func _eject_casing(barrel: Node3D) -> void:
	var shape := BoxShape3D.new()
	shape.size = Vector3(0.022, 0.022, 0.055)
	var box := BoxMesh.new()
	box.size = shape.size
	var casing := RigidBody3D.new()
	casing.collision_layer = 1 << (CASING_LAYER - 1)
	casing.collision_mask = GROUND_MASK
	var collision := CollisionShape3D.new()
	collision.shape = shape
	casing.add_child(collision)
	var visual := MeshInstance3D.new()
	visual.mesh = box
	var brass := StandardMaterial3D.new()
	brass.albedo_color = Color(0.85, 0.65, 0.25)
	brass.metallic = 0.8
	brass.roughness = 0.35
	visual.material_override = brass
	casing.add_child(visual)
	_world().add_child(casing)
	var head := barrel.global_basis
	casing.global_transform = barrel.global_transform
	var side := signf(barrel.position.x) if barrel.position.x != 0.0 else 1.0
	casing.linear_velocity = (head.x * side * _rng.randf_range(1.8, 3.0) + head.y * _rng.randf_range(1.5, 2.5) + head.z * 0.6) + spider.velocity
	casing.angular_velocity = Vector3(_rng.randf_range(-20, 20), _rng.randf_range(-20, 20), _rng.randf_range(-20, 20))
	casing.create_tween().tween_interval(CASING_LIFETIME).finished.connect(casing.queue_free)


func _world() -> Node:
	return spider.get_parent()


static func _glow_material(color: Color, energy: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
	return material
