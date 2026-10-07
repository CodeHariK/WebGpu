## Second-order dynamics (t3ssel8r, "Giving Personality to Procedural Animations using Math"):
## y follows a moving input x like a spring-mass-damper, tuned by three intuitive numbers:
##
##   f  frequency (Hz)    how fast it responds and how fast it wobbles
##   ζ  damping           0 = rings forever, 0–1 = overshoots and settles, ≥ 1 = no overshoot
##   r  initial response  0 = eases in, 1 = starts right away, > 1 overshoots the start,
##                        < 0 = anticipates: moves the wrong way first (a wind-up)
##
## The system:  y + k1·y' + k2·y'' = x + k3·x'
##   k1 = ζ / (π f)    k2 = 1 / (2π f)²    k3 = r ζ / (2π f)
## stepped with semi-implicit Euler. A frame longer than the critical time step
##   T_crit = 0.8 · (√(4 k2 + k1²) − k1)      (0.8 to be safe)
## is split into enough equal sub-steps to stay stable, so the spring keeps exactly the
## character you tuned however long the frame (the video's sub-stepping version).
## Works on Vector3; for a float, use Vector3(value, 0, 0) and read .x.
class_name SecondOrder
extends RefCounted

var value := Vector3.ZERO ## y: the smoothed output
var velocity := Vector3.ZERO ## y': handy for tilting into motion

var _k1 := 0.0
var _k2 := 0.0
var _k3 := 0.0
var _critical_step := 1.0 ## longest stable sub-step, seconds
var _previous_input := Vector3.ZERO


func _init(frequency: float, damping: float, response: float, start := Vector3.ZERO) -> void:
	set_parameters(frequency, damping, response)
	reset(start)


## Change f, ζ, r without losing the current motion.
func set_parameters(frequency: float, damping: float, response: float) -> void:
	frequency = maxf(frequency, 0.001)
	_k1 = damping / (PI * frequency)
	_k2 = 1.0 / pow(TAU * frequency, 2.0)
	_k3 = response * damping / (TAU * frequency)
	_critical_step = 0.8 * (sqrt(4.0 * _k2 + _k1 * _k1) - _k1)


## Jump straight to `to`, at rest (e.g. after a teleport).
func reset(to: Vector3) -> void:
	value = to
	velocity = Vector3.ZERO
	_previous_input = to


## Hit it: add `impulse` to the output's velocity (e.g. recoil). It flies off and the spring
## brings it back with its own wobble.
func kick(impulse: Vector3) -> void:
	velocity += impulse


## Advance by `delta` seconds toward input `x` and return the new output. The input's own
## velocity is estimated from the previous input.
func update(delta: float, x: Vector3) -> Vector3:
	if delta <= 0.0:
		return value
	var input_velocity := (x - _previous_input) / delta
	_previous_input = x
	var iterations := ceili(delta / _critical_step) # extra sub-steps only when the frame is too long
	var step := delta / iterations
	for i in iterations:
		value += velocity * step # position by velocity
		velocity += step * (x + _k3 * input_velocity - value - _k1 * velocity) / _k2 # velocity by acceleration
	return value
