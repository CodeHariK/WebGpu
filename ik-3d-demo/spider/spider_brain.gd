## Decides what the spider does next and runs it: charge (and back off), jump in, circle,
## fear (flee + peek), freeze, wander, dive (kamikaze: explodes, respawns), shoot (head gun
## bursts with recoil — only creatures that have one). Each lives in its own file under
## behaviours/.
##
## When one finishes, the next is a weighted random pick, nudged by distance to the target
## (no fleeing when already far away, no charging when there's no room), never the same twice
## in a row. And it reacts: the moment the target comes inside alert_distance it drops what
## it's doing and ambushes (freeze a beat, then rush).
## The brain only drives the Spider's steering hooks + action_pose; SpiderMood still does the
## eyes, the bob and the gait style on top, and R (threat) still works.
class_name SpiderBrain
extends Node

@export var spider: Spider
@export var target: Node3D ## the "player"
## Relative chance of each behaviour being picked (0 = never).
@export var weights := {"charge": 1.0, "jump": 1.0, "circle": 1.2, "fear": 0.6, "freeze": 0.8, "wander": 1.0, "dive": 0.3, "shoot": 1.2}
@export var gun: SpiderGun ## for "shoot" (only creatures with a head gun ever pick it)
@export var far_distance := 6.0 ## beyond this: no point fleeing, worth charging / jumping
@export var near_distance := 1.8 ## inside this: too close to charge, more likely to flee
@export_group("Alert")
## The moment the target comes inside this, drop whatever it's doing and ambush (freeze a beat,
## then rush). 0 = off.
@export var alert_distance := 3.0
@export var alert_cooldown := 6.0 ## seconds before it can be startled again
## Behaviours too committed to drop for an ambush.
@export var alert_ignores: Array[String] = ["ambush", "charge", "jump", "dive"]
@export_group("")
@export var active := true:
	set(value):
		active = value
		if not value and _current != null and not _current.done:
			_current.interrupt()
		if spider != null:
			_clear_hooks()
			spider.steering = value
		if value:
			_current = null

var _behaviours := {} ## name → SpiderBehaviour
var _current: SpiderBehaviour
var _current_name := ""
var _target_near := false ## was the target inside alert_distance last frame (edge trigger)
var _alert_left := 0.0 ## cooldown


func _ready() -> void:
	_behaviours = {
		"charge": ChargeBehaviour.new(),
		"jump": JumpBehaviour.new(),
		"circle": CircleBehaviour.new(),
		"fear": FearBehaviour.new(),
		"freeze": FreezeBehaviour.new(),
		"wander": WanderBehaviour.new(),
		"dive": DiveBehaviour.new(),
		"shoot": ShootBehaviour.new(),
		"ambush": AmbushBehaviour.new(), # not in `weights`: only as a reaction (or forced)
	}
	_behaviours.shoot.gun = gun
	for behaviour: SpiderBehaviour in _behaviours.values():
		behaviour.spider = spider
		behaviour.target = target
	active = active # push the state into the spider


func _physics_process(delta: float) -> void:
	if not active or spider == null or target == null or spider.legs.is_empty():
		return
	spider.steering = true
	_clear_hooks()
	_check_alert(delta)
	if _current == null or (_current.done and not spider.airborne):
		_start(_pick())
	_current.tick(delta)


# Edge trigger: only the moment the target crosses *into* alert_distance counts (it has to back
# out past 1.2× before it can startle again), and never twice within alert_cooldown.
func _check_alert(delta: float) -> void:
	_alert_left -= delta
	if alert_distance <= 0.0:
		return
	var distance := ((target.global_position - spider.global_position) * Vector3(1, 0, 1)).length()
	var near := distance < (alert_distance * 1.2 if _target_near else alert_distance)
	var arrived := near and not _target_near
	_target_near = near
	if not arrived or _alert_left > 0.0 or spider.airborne or not spider.visible:
		return
	if _current != null and not _current.done and _current_name in alert_ignores:
		return
	_alert_left = alert_cooldown
	_start("ambush")


## Switch to a behaviour right now (by name, e.g. "jump").
func force(behaviour_name: String) -> void:
	if _behaviours.has(behaviour_name) and not spider.airborne and _behaviours[behaviour_name].available():
		_start(behaviour_name)


## "charge: backoff" — what it's doing, for the HUD.
func status() -> String:
	if not active or _current == null:
		return "-"
	return "%s: %s" % [_current.label(), _current.phase]


func _start(behaviour_name: String) -> void:
	if _current != null and not _current.done:
		_current.interrupt()
	_current_name = behaviour_name
	_current = _behaviours[behaviour_name]
	_current.start()


func _pick() -> String:
	var distance := (target.global_position - spider.global_position) * Vector3(1, 0, 1)
	var chances := {}
	var total := 0.0
	for behaviour_name: String in weights:
		var chance: float = weights[behaviour_name]
		if not _behaviours.has(behaviour_name) or not _behaviours[behaviour_name].available():
			chance = 0.0 # can't (e.g. no gun)
		elif behaviour_name == _current_name:
			chance = 0.0 # never twice in a row
		elif distance.length() > far_distance:
			chance *= {"fear": 0.0, "charge": 2.0, "jump": 2.0}.get(behaviour_name, 1.0)
		elif distance.length() < near_distance:
			chance *= {"fear": 2.5, "charge": 0.0, "dive": 0.0, "circle": 1.5}.get(behaviour_name, 1.0)
		chances[behaviour_name] = chance
		total += chance
	var roll := randf() * total
	for behaviour_name: String in chances:
		roll -= chances[behaviour_name]
		if roll <= 0.0 and chances[behaviour_name] > 0.0:
			return behaviour_name
	return "wander"


# Every frame starts from neutral; the running behaviour sets only what it wants.
func _clear_hooks() -> void:
	spider.steer = Vector2.ZERO
	spider.steer_turn = 0.0
	spider.action_pose = Vector3.ZERO
	spider.air_splay = 0.0
