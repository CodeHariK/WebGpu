## When and where the crawler's hands and feet step. The spider's rules for walking, the
## cultist's for everything else.
##
##   homes     where each limb would rest right now: its rest spot on the surface frame, pushed
##             ahead by the velocity (lead_time, so steps land in front), dropped onto the
##             surface with one ray along -up (every frame: it decides WHEN to step)
##   footholds when a limb steps, crawler_foothold.gd tries a ring of candidate spots round the
##             home (down / hook-over-the-edge / reach rays) and picks the best reachable one
##             (WHERE to step). A limb with nowhere better to go stays where it is.
##   groups    diagonal pairs take turns (spider): when nothing is stepping and any limb of the
##             next pair is more than step_distance from its home, that whole pair steps. So one
##             pair always holds the body up, and planted limbs never slide.
##   timing    a step must land before the body has moved on a stride, so steps get quicker when
##             it moves fast (spider leg_step_time): step_time at a walk, fast_step_time at most
##   rescue    a planted limb steps now, out of turn, if it's about to overstretch (spider) or the
##             IK couldn't reach it last frame (`ik_errors` from crawler_rig.gd: the cultist's
##             feedback check, done without reading bones back)
##   air       the cultist's flailing: each limb chases a spot circling round its usual one in
##             the body's side plane, each out of step with the others
##   landing   coming down, every limb steps to its home at once (cultist)
##
## Mobile: one ray per limb per frame for the homes; up to ~25 when a limb steps (about four
## steps a second while walking).
class_name CrawlerGait
extends RefCounted

var step_distance := 0.3 ## a limb this far from its home wants to step (m)
var lead_time := 0.3 ## homes are pushed ahead by velocity × this (s)
var max_lead := 0.35 ## …but no further than this (m)
var step_time := 0.26 ## a step's duration at a walk (s)
var fast_step_time := 0.12 ## the quickest a step gets (s)
var overreach := 0.95 ## step now if a planted limb is this share of its reach from its root
var ik_slack := 0.08 ## step now if the IK missed the limb's spot by more than this (m)
var flail_speed := 14.0 ## rad/s
var flail_size := 0.35 ## m
var world_mask := 1

var steps: Array[CrawlerStep] = []
var foothold := CrawlerFoothold.new()
var rays: CrawlerRays: ## every ray goes through this log (the foothold search shares it)
	set(value):
		rays = value
		foothold.rays = value
var homes := PackedVector3Array()
var flat_homes := PackedVector3Array() ## homes before the ray: on the surface frame's plane
var wants: Array[bool] = [false, false, false, false] ## (debug) limb is far enough to want a step
var home_normals := PackedVector3Array()
var home_hits: Array[bool] = []
var airborne := false

var _next_group := 0
var _flight := 0.0
const _FLAIL_PHASE := [0.0, 2.1, 4.2, 1.0]


func _init() -> void:
	for limb in CrawlerLayout.COUNT:
		steps.append(CrawlerStep.new())
	homes.resize(CrawlerLayout.COUNT)
	flat_homes.resize(CrawlerLayout.COUNT)
	home_normals.resize(CrawlerLayout.COUNT)
	home_hits.resize(CrawlerLayout.COUNT)


## Put every limb straight on its home (spawning). `body`: the posed body (for the roots).
func place_all(space: PhysicsDirectSpaceState3D, frame: Transform3D, body: Transform3D) -> void:
	_update_homes(space, frame, Vector3.ZERO, body)
	for limb in CrawlerLayout.COUNT:
		var spot := foothold.pick(space, limb, flat_homes[limb], frame, body, _others(limb))
		steps[limb].place(spot.position, spot.normal, spot.hit)


## One frame. `frame`: the surface frame under the body; `body`: the posed body;
## `ik_errors`: how far each limb's IK missed last frame; `flying`: no surface under it at all.
func update(space: PhysicsDirectSpaceState3D, delta: float, frame: Transform3D, velocity: Vector3,
		body: Transform3D, ik_errors: PackedFloat32Array, flying: bool) -> void:
	if flying != airborne:
		airborne = flying
		_flight = 0.0
		for step in steps:
			step.set_airborne(flying)
		if not flying:
			_update_homes(space, frame, velocity, body)
			_step_all(space, frame, body, step_time * 0.7)
	if airborne:
		_flight += delta * flail_speed
		for limb in CrawlerLayout.COUNT:
			steps[limb].update(delta, _flail_spot(limb, body))
		return
	_update_homes(space, frame, velocity, body)
	var duration := _step_duration(velocity)
	_maybe_start_group(space, frame, body, duration)
	_rescue(space, frame, body, ik_errors, duration)
	for step in steps:
		step.update(delta, Vector3.ZERO)


## How many limbs are mid-step (the body dips a little for each).
func stepping_count() -> int:
	var count := 0
	for step in steps:
		count += 1 if step.stepping else 0
	return count


func _step_duration(velocity: Vector3) -> float:
	var keep_up := step_distance * 1.2 / maxf(velocity.length(), 0.01)
	return minf(step_time, maxf(keep_up, fast_step_time))


func _maybe_start_group(space: PhysicsDirectSpaceState3D, frame: Transform3D, body: Transform3D, duration: float) -> void:
	for limb in CrawlerLayout.COUNT:
		wants[limb] = steps[limb].position.distance_to(homes[limb]) > step_distance
	if stepping_count() > 0:
		return
	for attempt in CrawlerLayout.GROUPS.size():
		var group: int = (_next_group + attempt) % CrawlerLayout.GROUPS.size()
		var any := false
		for limb: int in CrawlerLayout.GROUPS[group]:
			any = any or wants[limb]
		if any:
			var moved := false
			for limb: int in CrawlerLayout.GROUPS[group]:
				moved = _step(space, frame, limb, body, duration, false) or moved
			if moved:
				_next_group = (group + 1) % CrawlerLayout.GROUPS.size()
				return


func _rescue(space: PhysicsDirectSpaceState3D, frame: Transform3D, body: Transform3D, ik_errors: PackedFloat32Array, duration: float) -> void:
	for limb in CrawlerLayout.COUNT:
		var step := steps[limb]
		if step.stepping:
			continue
		var root: Vector3 = body * CrawlerLayout.ROOTS[limb]
		var stretched := root.distance_to(step.position) > CrawlerLayout.reach(limb) * overreach
		var missed := limb < ik_errors.size() and ik_errors[limb] > ik_slack
		if stretched or missed:
			_step(space, frame, limb, body, duration, true)


## Step `limb` to its best foothold. Skipped (returns false) when that is where it already is,
## or when nothing is reachable and it isn't `forced` (it holds on where it is instead).
func _step(space: PhysicsDirectSpaceState3D, frame: Transform3D, limb: int, body: Transform3D, duration: float, forced: bool) -> bool:
	var spot := foothold.pick(space, limb, flat_homes[limb], frame, body, _others(limb))
	var step := steps[limb]
	if spot.position.distance_to(step.position) < 0.05:
		return false
	if not spot.hit and step.on_surface and not forced:
		return false
	step.step_to(spot.position, spot.normal, spot.hit, duration, body.origin, body.basis.y)
	return true


func _step_all(space: PhysicsDirectSpaceState3D, frame: Transform3D, body: Transform3D, duration: float) -> void:
	for limb in CrawlerLayout.COUNT:
		_step(space, frame, limb, body, duration, true)


## Where the other limbs are planted or landing (a foothold mustn't crowd them).
func _others(limb: int) -> PackedVector3Array:
	var others := PackedVector3Array()
	for other in CrawlerLayout.COUNT:
		if other != limb:
			others.append(steps[other].landing())
	return others


func _update_homes(space: PhysicsDirectSpaceState3D, frame: Transform3D, velocity: Vector3, _body: Transform3D) -> void:
	var up := frame.basis.y
	var lead := (velocity * lead_time).limit_length(max_lead)
	for limb in CrawlerLayout.COUNT:
		var spot := frame * CrawlerLayout.rest(limb) + lead
		flat_homes[limb] = spot
		var hit := _ray(space, spot + up * 0.5, spot - up * 0.6)
		home_hits[limb] = not hit.is_empty()
		homes[limb] = hit.position if home_hits[limb] else spot
		home_normals[limb] = hit.normal if home_hits[limb] else up


## In the air: a spot below and beside the body, swung round a circle in the body's side plane.
func _flail_spot(limb: int, body: Transform3D) -> Vector3:
	var rest := CrawlerLayout.rest(limb) + Vector3(0.0, -CrawlerLayout.RIDE_HEIGHT * 0.6, 0.0)
	var angle: float = _flight + _FLAIL_PHASE[limb]
	return body * rest + body.basis * (Vector3(0.0, sin(angle), cos(angle)) * flail_size)


func _ray(space: PhysicsDirectSpaceState3D, from: Vector3, to: Vector3) -> Dictionary:
	return rays.cast(space, from, to, world_mask, CrawlerRays.Kind.HOME)
