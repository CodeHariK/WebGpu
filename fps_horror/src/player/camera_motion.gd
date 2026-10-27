## Camera feel, on the player's Camera3D. All offsets are local to the head, so they never move
## the body or its collision.
##   bob      side-to-side and up-down sway while walking, bigger and faster when sprinting
##   landing  a dip after a fall, deeper the faster you landed
##   steps    when the body pops up a stair, the camera starts that much lower and catches up
##   sprint   the field of view widens a little
##   shake    a quick jolt (gun shots), fading out in a fraction of a second
extends Camera3D

@export var bob_height := 0.045
@export var bob_side := 0.03
@export var bob_rate := 0.9  ## full bob cycles per metre walked
@export var landing_dip := 0.035  ## metres of dip per m/s of landing speed
@export var recover_rate := 10.0
@export var sprint_fov_add := 6.0

var _phase := 0.0
var _bob_weight := 0.0
var _dip := 0.0
var _step := 0.0
var _base_fov := 70.0
var _shake := 0.0

@onready var _body: CharacterBody3D = owner


func _ready() -> void:
	_base_fov = fov
	_body.landed.connect(func(speed: float) -> void: _dip -= minf(speed, 12.0) * landing_dip)
	_body.stepped.connect(func(h: float) -> void: _step -= h)


func _process(delta: float) -> void:
	var flat := Vector2(_body.velocity.x, _body.velocity.z).length()
	var moving := _body.is_on_floor() and flat > 0.5
	_bob_weight = move_toward(_bob_weight, 1.0 if moving else 0.0, delta * 4.0)
	_phase += flat * bob_rate * delta * PI
	var bob := Vector3(cos(_phase) * bob_side, absf(sin(_phase)) * bob_height, 0.0) * _bob_weight
	_dip = lerpf(_dip, 0.0, 1.0 - exp(-recover_rate * 0.6 * delta))
	_step = lerpf(_step, 0.0, 1.0 - exp(-recover_rate * delta))
	_shake = move_toward(_shake, 0.0, delta * 5.0)
	var jolt := Vector3(randf_range(-1, 1), randf_range(-1, 1), randf_range(0, 1)) * _shake * 0.012
	position = bob + Vector3(0.0, _dip + _step, 0.0) + jolt
	var target_fov := _base_fov + (sprint_fov_add if _body.get("sprinting") else 0.0)
	fov = lerpf(fov, target_fov, 1.0 - exp(-6.0 * delta))


## Jolt the camera; `amount` 0..1 (stacks up to 1).
func shake(amount: float) -> void:
	_shake = minf(_shake + amount, 1.0)
