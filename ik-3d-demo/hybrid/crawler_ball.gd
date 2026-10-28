## The crawler's physics body: a ball at its chest (the cultist's rolling ball, kept for what
## physics is good at). Two states:
##   carried   frozen (kinematic) and moved with the body every frame. It's still the hitbox:
##             shots hit it (take_hit), and it shoves crates out of the way.
##   released  a free RigidBody: knocked off a wall, hit hard, leaping. Gravity, bounces and
##             rolling are all real. The crawler rides on it until it has settled on the ground,
##             then the walker takes over again (Crawler._tumble).
##
## probe() each physics frame while released: one ray down for the ground under it, and a
## timer for how long it has lain still there.
class_name CrawlerBall
extends RigidBody3D

var radius := 0.3
var world_mask := 1
var settle_speed := 1.2 ## slower than this on the ground counts as still (m/s)
var settle_time := 0.2 ## …for this long (s)
var min_air_time := 0.3 ## never settles sooner than this after release (s)
var ground_drag := 3.0 ## linear damping while touching the ground: it digs its limbs in

var crawler: Node ## gets take_hit() calls
var rays: CrawlerRays ## every ray goes through this log
var grounded := false
var ground_point := Vector3.ZERO
var ground_normal := Vector3.UP

var _still := 0.0
var _released_for := 0.0


func _init() -> void:
	name = "Ball"
	top_level = true
	mass = 3.0
	collision_layer = 2
	collision_mask = 1
	continuous_cd = true
	angular_damp = 2.0
	freeze_mode = RigidBody3D.FREEZE_MODE_KINEMATIC
	freeze = true
	var material := PhysicsMaterial.new()
	material.friction = 0.9
	material.bounce = 0.15
	physics_material_override = material
	var shape := CollisionShape3D.new()
	var sphere := SphereShape3D.new()
	sphere.radius = radius
	shape.shape = sphere
	add_child(shape)


func carry(at: Vector3) -> void:
	global_position = at


func release(velocity: Vector3) -> void:
	freeze = false
	linear_velocity = velocity
	angular_velocity = Vector3.ZERO
	_still = 0.0
	_released_for = 0.0
	grounded = false


func catch_ball() -> void:
	freeze = true
	linear_damp = 0.0


func probe(delta: float) -> void:
	_released_for += delta
	var from := global_position
	var hit := rays.cast(get_world_3d().direct_space_state, from, from + Vector3.DOWN * (radius + 0.25), world_mask, CrawlerRays.Kind.BALL)
	grounded = not hit.is_empty()
	linear_damp = ground_drag if grounded and _released_for > min_air_time else 0.0
	if grounded:
		ground_point = hit.position
		ground_normal = hit.normal
	var still := grounded and linear_velocity.length() < settle_speed and _released_for > min_air_time
	_still = _still + delta if still else 0.0


func settled() -> bool:
	return _still >= settle_time


## Shot or struck: the crawler decides what that does.
func take_hit(point: Vector3, force: Vector3, _attacker: Node3D = null) -> void:
	if crawler:
		crawler.hit(point, force)
