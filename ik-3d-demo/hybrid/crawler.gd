## A four-limbed crawler, half spider, half cultist: it walks like the spider (planted limbs that
## never slide, diagonal pairs taking turns, the body fitted to where its hands and feet are,
## springs for weight) over any surface like the cultist (walls, ceilings, round edges), and
## falls, tumbles and leaps like the cultist (a real physics ball). One rigid mesh per bone,
## custom two-bone IK, no Skeleton3D.
##
## Two modes:
##   WALK    surface_walker.gd moves it (kinematic, rays only); crawler_gait.gd steps the limbs;
##           crawler_pose.gd fits and springs the body; the ball rides along as a frozen hitbox
##   TUMBLE  crawler_ball.gd is released and physics moves it: knocked off a wall, hit hard,
##           leaping, walked off an edge. Limbs flail in the air and scramble on the ground;
##           once the ball has settled, the walker takes over again where it lies
##
## Control (from a brain or a player), each physics frame:
##   ahead, turn   -1..1 inputs (forward/back, left/right about the surface normal), or
##   goal          a world point: it steers there by itself (overrides ahead/turn)
##   look_target   a node the head watches (null: looks ahead)
##   leap(at)      crouch, then jump at a point (cultist lunge: a ballistic arc)
##   hit(p, f)     a blow at p with force f: small ones make it flinch, big ones (or any while on a
##                 wall or ceiling) knock it loose
##
## Files: crawler_layout.gd (body plan), surface_walker.gd, crawler_gait.gd + crawler_step.gd,
## crawler_pose.gd, crawler_ball.gd, crawler_rig.gd. Reuses limbs/limb_chain.gd,
## limbs/limb_renderer.gd and spider/second_order.gd.
class_name Crawler
extends Node3D

enum Mode { WALK, TUMBLE }

@export var look_target: Node3D
@export var knock_force := 8.0 ## blows at least this strong knock it loose (N·s)
@export var crouch_time := 0.25 ## the wind-up before a leap (s)
@export var crouch_depth := 0.18 ## m

var ahead := 0.0
var turn := 0.0
var goal: Variant = null ## Vector3 or null
var stop_distance := 0.8 ## goal reached this close (m)

var mode := Mode.WALK
var walker := SurfaceWalker.new()
var gait := CrawlerGait.new()
var pose: CrawlerPose
var ball := CrawlerBall.new()
var rig := CrawlerRig.new()
var rays := CrawlerRays.new() ## every ray it casts, for the debug view and the counts

var _crouch := 0.0 ## time left in a leap's wind-up (0 = not crouching)
var _leap_to := Vector3.ZERO
var _heading := Vector3.FORWARD


func _ready() -> void:
	walker.rays = rays
	gait.rays = rays
	ball.rays = rays
	add_child(rig)
	ball.crawler = self
	add_child(ball)
	pose = CrawlerPose.new(global_transform)
	place(global_position, -global_basis.z)


## Put it down on whatever is under `at`, facing `forward` (spawning, teleports).
func place(at: Vector3, forward: Vector3) -> void:
	var space := get_world_3d().direct_space_state
	var hit := rays.cast(space, at + Vector3.UP, at + Vector3.DOWN * 3.0, walker.world_mask, CrawlerRays.Kind.PLACE)
	var ground: Vector3 = hit.position if not hit.is_empty() else at
	var normal: Vector3 = hit.normal if not hit.is_empty() else Vector3.UP
	mode = Mode.WALK
	ball.catch_ball()
	walker.reset(ground, normal, forward)
	var body := Transform3D(walker.frame.basis, walker.frame * Vector3(0.0, CrawlerLayout.RIDE_HEIGHT, 0.0))
	pose.reset(body)
	gait.airborne = false
	gait.place_all(space, walker.frame, body)
	ball.carry(body.origin)
	_heading = forward
	_crouch = 0.0


func leap(at: Vector3) -> void:
	if mode == Mode.WALK and _crouch <= 0.0:
		_crouch = crouch_time
		_leap_to = at


func hit(point: Vector3, force: Vector3) -> void:
	if mode == Mode.TUMBLE:
		ball.apply_impulse(force, point - ball.global_position)
		return
	var on_floor := walker.up().dot(Vector3.UP) > 0.5
	if force.length() >= knock_force or not on_floor:
		_tumble(walker.velocity)
		ball.apply_impulse(force, point - ball.global_position)
	else:
		pose.kick(force / ball.mass)


func _physics_process(delta: float) -> void:
	rays.begin_frame()
	var space := get_world_3d().direct_space_state
	if mode == Mode.WALK:
		_walk(space, delta)
	else:
		_roll(space, delta)
	var watch: Variant = look_target.global_position if look_target else null
	rig.update(delta, pose.transform, gait.steps, watch)


func _walk(space: PhysicsDirectSpaceState3D, delta: float) -> void:
	var inputs := _steer()
	if _crouch > 0.0:
		inputs = Vector2.ZERO
		_crouch -= delta
		if _crouch <= 0.0:
			_tumble(_ballistic(pose.transform.origin, _leap_to))
			return
	if not walker.move(space, delta, inputs.x, inputs.y):
		_tumble(walker.velocity)
		return
	_heading = walker.forward()
	gait.update(space, delta, walker.frame, walker.velocity, pose.transform, rig.errors, false)
	var target := pose.walk_target(walker.frame, gait.steps, gait.stepping_count())
	if _crouch > 0.0:
		target.origin -= target.basis.y * crouch_depth * (1.0 - _crouch / crouch_time)
	pose.update(delta, target)
	ball.carry(pose.transform.origin)


func _roll(space: PhysicsDirectSpaceState3D, delta: float) -> void:
	ball.probe(delta)
	var flat := ball.linear_velocity - Vector3.UP * ball.linear_velocity.y
	if flat.length() > 0.5:
		_heading = flat.normalized()
	var up := ball.ground_normal if ball.grounded else Vector3.UP
	var frame := Transform3D(SurfaceWalker.surface_basis(up, _heading), ball.ground_point)
	gait.update(space, delta, frame, flat, pose.transform, rig.errors, not ball.grounded)
	pose.update(delta, pose.tumble_target(ball.global_position, up, _heading))
	if ball.settled():
		mode = Mode.WALK
		ball.catch_ball()
		walker.reset(ball.ground_point, ball.ground_normal, _heading)


func _tumble(velocity: Vector3) -> void:
	mode = Mode.TUMBLE
	_crouch = 0.0
	ball.global_position = pose.transform.origin
	ball.release(velocity)


## (ahead, turn) from the goal if there is one, else the inputs as given.
func _steer() -> Vector2:
	if not goal is Vector3:
		return Vector2(ahead, turn)
	var to: Vector3 = (goal as Vector3) - walker.frame.origin
	var up := walker.up()
	to -= up * to.dot(up)
	if to.length() < stop_distance:
		return Vector2.ZERO
	var angle := walker.forward().signed_angle_to(to, up)
	var steer := clampf(angle * 2.0, -1.0, 1.0)
	return Vector2(1.0 if absf(angle) < 1.0 else 0.3, steer)


## The launch velocity that lands on `to` from `from` (gravity included); flatter for short hops.
func _ballistic(from: Vector3, to: Vector3) -> Vector3:
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity") * ball.gravity_scale
	var time := clampf(from.distance_to(to) / 7.0, 0.35, 0.8)
	var velocity := (to - from) / time
	velocity.y += 0.5 * gravity * time
	return velocity
