## The stand-in player in a car: drives round a road (a looping Curve3D) at a speed you set.
##   W / S   faster / slower (0 … MAX_SPEED)      Space  stop / go
## The engine is a NoiseBus noise every ENGINE_EVERY seconds that carries further the faster the
## car goes (ENGINE_RADIUS + ENGINE_PER_SPEED × speed) — spiders hear it coming round corners.
## Positions along the road are curve offsets (metres from the start); predict() and
## time_to_offset() answer "where will it be" and "when will it get there".
class_name TestCar
extends TestPlayer

const MAX_SPEED := 16.0 ## m/s
const ACCELERATION := 6.0 ## m/s² while W / S is held
const ENGINE_EVERY := 0.3 ## seconds
const ENGINE_RADIUS := 6.0 ## metres the idling engine carries …
const ENGINE_PER_SPEED := 1.2 ## … plus this per m/s
const CAR_HEIGHT := 0.45 ## the body's centre above the road

var road: Curve3D
var offset := 0.0 ## metres along the road
var speed := 8.0 ## m/s
var _engine_left := 0.0
var _resume_speed := 8.0


func in_car() -> bool:
	return true


func eye() -> Vector3:
	return orb.global_position + Vector3.UP * 0.8 # the driver


func predict(seconds: float) -> Vector3:
	return _road_point(offset + speed * seconds)


## Seconds until the car reaches road `point_offset` (negative: it passed it in the last 10% of
## a lap; INF when stopped).
func time_to_offset(point_offset: float) -> float:
	var length := road.get_baked_length()
	var ahead := wrapf(point_offset - offset, 0.0, length)
	if ahead > length * 0.9:
		ahead -= length
	return ahead / speed if speed > 0.1 else INF


## The road offset nearest `point`.
func offset_of(point: Vector3) -> float:
	return road.get_closest_offset(point)


func velocity() -> Vector3:
	return facing * speed


func handle_click(_event: InputEvent, _camera: Camera3D) -> bool:
	return false


func _process(delta: float) -> void:
	if orb == null or road == null:
		return
	if Input.is_key_pressed(KEY_W):
		speed = minf(speed + ACCELERATION * delta, MAX_SPEED)
	if Input.is_key_pressed(KEY_S):
		speed = maxf(speed - ACCELERATION * delta, 0.0)
	offset = wrapf(offset + speed * delta, 0.0, road.get_baked_length())
	var here := _road_point(offset)
	var ahead := _road_point(offset + 1.0) - here
	ahead.y = 0.0
	if ahead.length() > 0.01:
		facing = ahead.normalized()
	orb.global_position = here
	orb.look_at(here + facing, Vector3.UP)
	_engine_left -= delta
	if _engine_left <= 0.0:
		_engine_left = ENGINE_EVERY
		NoiseBus.emit(here, ENGINE_RADIUS + ENGINE_PER_SPEED * speed)


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key != null and key.pressed and not key.echo and key.keycode == KEY_SPACE:
		if speed > 0.1:
			_resume_speed = speed
			speed = 0.0
		else:
			speed = _resume_speed


func _road_point(at: float) -> Vector3:
	return road.sample_baked(wrapf(at, 0.0, road.get_baked_length())) + Vector3.UP * CAR_HEIGHT
