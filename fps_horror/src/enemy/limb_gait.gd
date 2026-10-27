## How the ball moves: four limbs take turns stepping, and every step kicks the ball. Ported from
## LimbPlacementController (and Limb.InitializeStep) in the original.
##
##   step cycle  every step_interval one limb (left hand, right hand, left foot, right foot, round
##               and round) checks where it should be. If that spot is more than min_step from
##               where it's planted, it re-plants there and kicks the ball:
##                 feet push away from the new spot, hands pull toward it, both bent strongly
##                 toward the body's desired_velocity; plus a little hop (step_bounce), minus the
##                 ball's current velocity (so each kick steers rather than adds up)
##   limb spots  a ray from beside the shoulders (or hips), angled outward and down, `reach_down`
##               long; ahead of the ball by its velocity, so the limbs reach where it's going.
##               We also lead by desired_velocity: that's what gets a resting ball moving (in the
##               original, IK error made the limbs step now and then, which started it off).
##               And a ball that wants to move but is standing still steps anyway (otherwise
##               a ball pinned against something never steps again)
##   launch      when the body asks (onto/off a wall, toward the ceiling), all limbs plant, then
##               after 0.25 s one big impulse toward the next waypoint (and up)
##   landing     touching ground again after a jump re-plants every limb at once
##
## In this step a limb is just a point (shown as a small marker). Step 7 adds the cultist: IK
## arms and legs reaching for these points, Bezier arcs between steps, flailing in the air.
## Far-away enemies (step 11) can keep using only this file: the same movement, no IK.
##
## Mobile: one ray per step (about 8 a second), nothing else per frame.
extends Node3D

const EnemyBody := preload("res://src/enemy/enemy_body.gd")

enum Limb { LEFT_HAND, RIGHT_HAND, LEFT_FOOT, RIGHT_FOOT }

@export var step_interval := 0.12  ## seconds between two limbs stepping
@export var min_step := 0.5  ## a limb re-plants once its spot is this far away (m)
@export var kick_speed := 8.0  ## impulse of a step along the move direction
@export var step_bounce := 5.0  ## upward part of each step's impulse
@export var lead := 0.45  ## how far ahead (s of velocity) limbs reach; also slows kicks when braking
@export var hand_bias := 0.8  ## how much hand kicks follow desired_velocity (vs. the limb's motion)
@export var foot_bias := 0.9
@export var launch_scale := 5.0
@export var max_launch := 40.0
@export var body_length := 1.0  ## hands this far ahead of feet
@export var shoulder_width := 0.5
@export var hip_width := 2.0  ## how far apart the limbs land
@export var reach_down := 2.5
@export var show_limbs := true

var body: EnemyBody
var planted: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO, Vector3.ZERO, Vector3.ZERO]
var on_surface: Array[bool] = [false, false, false, false]
var heading := Vector3.FORWARD  ## last direction the ball moved in
var launching := false

var _timer := 0.0
var _next := 0
var _was_grounded := EnemyBody.Grounded.NONE
var _markers: Array[MeshInstance3D] = []


func _ready() -> void:
	body = get_node("../Body")
	body.launch_request.connect(_launch)
	_timer = randf() * step_interval
	_next = randi() % 4
	_plant_all()
	if show_limbs:
		_make_markers()


func _physics_process(delta: float) -> void:
	if body.linear_velocity.length() > 0.5:
		heading = body.linear_velocity.normalized()
	if body.grounded != _was_grounded:
		if body.grounded != EnemyBody.Grounded.NONE:
			_plant_all()
		_was_grounded = body.grounded
	if body.grounded == EnemyBody.Grounded.NONE:
		_dangle()
	elif not launching:
		_timer += delta
		while _timer >= step_interval:
			_timer -= step_interval
			_step(_next)
			_next = (_next + 1) % 4
	_move_markers(delta)


## One limb's turn: re-plant if its spot moved far enough, kicking the ball.
func _step(limb: int) -> void:
	var spot := limb_spot(limb)
	var wants_to_go := body.desired_velocity != Vector3.ZERO
	var stuck := wants_to_go and body.linear_velocity.length() < 0.5  # pinned: step anyway
	if planted[limb].distance_to(spot.position) <= min_step and not stuck:
		return
	if (on_surface[limb] or spot.on_surface) and wants_to_go:
		var foot := limb >= Limb.LEFT_FOOT
		var motion: Vector3 = planted[limb] - spot.position if foot else spot.position - planted[limb]
		var direction := motion.normalized().lerp(body.desired_velocity, foot_bias if foot else hand_bias)
		_kick(direction.normalized(), planted[limb].lerp(body.global_position, 0.8))
	planted[limb] = spot.position
	on_surface[limb] = spot.on_surface


## Push the ball along `direction` from `at` (a point between the limb and the ball's centre, so
## steps also spin it a little).
func _kick(direction: Vector3, at: Vector3) -> void:
	var speed := kick_speed
	if body.linear_velocity.dot(body.desired_velocity) < 0.0:  # moving the wrong way: gentler
		speed *= body.desired_velocity.length() * lead
	var impulse := direction * speed + Vector3.UP * step_bounce - body.linear_velocity
	body.apply_impulse(impulse, at - body.global_position)


## The body wants to jump: plant everything, crouch for a moment, then leap at the next waypoint.
func _launch() -> void:
	if launching:
		return
	launching = true
	_plant_all()
	await get_tree().create_timer(0.25, false, true).timeout
	launching = false
	if not is_instance_valid(body):
		return
	var to: Vector3 = body.next_waypoint().position
	var aim := to + Vector3.UP * body.global_position.distance_to(to) * 2.0
	var impulse := (aim - body.global_position) * launch_scale - body.linear_velocity
	body.apply_impulse(impulse.limit_length(max_launch))


func _plant_all() -> void:
	for limb in 4:
		var spot := limb_spot(limb)
		planted[limb] = spot.position
		on_surface[limb] = spot.on_surface


## In the air the limbs hang where they'd reach for (no rays). Step 7 makes them flail.
func _dangle() -> void:
	for limb in 4:
		planted[limb] = _reach(limb)[1]
		on_surface[limb] = false


class Spot:
	var position: Vector3
	var on_surface: bool


## Where `limb` wants to be: the ray hit, or the end of its reach if nothing is there.
func limb_spot(limb: int) -> Spot:
	var reach := _reach(limb)
	var spot := Spot.new()
	var from: Vector3 = reach[0]
	var to: Vector3 = from + (reach[1] - from).normalized() * reach_down
	var query := PhysicsRayQueryParameters3D.create(from, to, body.world_mask, [body.get_rid()])
	var hit := body.get_world_3d().direct_space_state.intersect_ray(query)
	spot.on_surface = not hit.is_empty()
	spot.position = hit.position if spot.on_surface else reach[1]
	return spot


## [ray start beside the shoulder or hip, the far point it aims at] for one limb.
func _reach(limb: int) -> Array[Vector3]:
	var up := body.ground_normal
	if up == Vector3.UP and absf(heading.y) > 0.99:
		up = Vector3.BACK  # heading straight up or down: any other "up" will do
	var foot := limb >= Limb.LEFT_FOOT
	var left := limb == Limb.LEFT_HAND or limb == Limb.LEFT_FOOT
	var centre := body.global_position + up + (body.linear_velocity + body.desired_velocity) * lead
	centre += heading * body_length * 0.5 * (-1.0 if foot else 1.0)
	var side := heading.cross(up).normalized() * (-1.0 if left else 1.0)
	return [centre + side * shoulder_width * 0.5, centre + side * hip_width * 0.5 - up * (reach_down + 1.0)]


func _make_markers() -> void:
	var colors := [Color(0.9, 0.8, 0.7), Color(0.9, 0.8, 0.7), Color(0.8, 0.15, 0.1), Color(0.8, 0.15, 0.1)]
	for limb in 4:
		var mesh := SphereMesh.new()
		mesh.radius = 0.09
		mesh.height = 0.18
		mesh.radial_segments = 8
		mesh.rings = 4
		var mat := StandardMaterial3D.new()
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.albedo_color = colors[limb]
		mesh.material = mat
		var marker := MeshInstance3D.new()
		marker.mesh = mesh
		marker.top_level = true
		marker.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(marker)
		marker.global_position = planted[limb]
		_markers.append(marker)


## Markers slide to their planted spot quickly, so you can see each step.
func _move_markers(delta: float) -> void:
	var blend := 1.0 - exp(-delta * 18.0)
	for limb in _markers.size():
		_markers[limb].global_position = _markers[limb].global_position.lerp(planted[limb], blend)
