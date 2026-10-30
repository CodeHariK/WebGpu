## The skyship's shield: a bubble round the ship that protects its laser turrets (ShipTurrets).
## It isn't always on — it cycles: down for `down_time` s (the ship hangs still: hit the turrets
## now), `warn_time` s of flicker as a warning, then up for `up_time` s, then down again. Only
## while the ship fights; staggered or crashing, it's off.
class_name ShipShield
extends RefCounted

const BUBBLE := 4.3 ## m: the bubble's radius round the ship (before Skyship.SIZE)
## The bubble: added light, bright at its rim and nearly clear face-on (so the ship stays visible),
## with faint bands crawling up it.
const BUBBLE_SHADER := """
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_back, shadows_disabled;
uniform vec3 tint = vec3(0.3, 0.9, 1.0);
uniform float strength = 1.0;
void fragment() {
	float rim = pow(1.0 - abs(dot(NORMAL, VIEW)), 2.5);
	float bands = 0.5 + 0.5 * sin(UV.y * 140.0 - TIME * 3.0);
	ALBEDO = tint * (rim * (1.0 + 0.4 * bands) + 0.02) * strength;
}
"""

var up_time := 5.0 ## s the shield stays up
var down_time := 7.0 ## s it stays down (the window to hit the turrets)
var warn_time := 1.0 ## s it flickers before it comes up
var on := false ## the shield is up now
var left := 0.0 ## s until it next switches
var burst := 0.0 ## 1 when the bubble just dropped (fades)

var _ship: Skyship
var _bubble: MeshInstance3D
var _bubble_material := ShaderMaterial.new()


func setup(boss: Skyship) -> void:
	_ship = boss
	var shader := Shader.new()
	shader.code = BUBBLE_SHADER
	_bubble_material.shader = shader
	_bubble = ShipKit.add(boss, ShipKit.sphere(BUBBLE, 48), _bubble_material)
	_bubble.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF


## Down, with a full down window ahead (a reset, after a stagger).
func rebuild() -> void:
	on = false
	left = down_time


## Down → (flicker) → up → down … only while the ship fights.
func tick(delta: float) -> void:
	burst = maxf(burst - delta * 1.5, 0.0)
	if _ship.state != Skyship.State.FIGHT:
		if on:
			_drop()
		left = down_time
		return
	left -= delta
	if left > 0.0:
		return
	if on:
		_drop()
	else:
		on = true
		left = up_time


func _drop() -> void:
	on = false
	left = down_time
	burst = 1.0


## Flickering: about to come up (the warning).
func warning() -> bool:
	return not on and _ship.state == Skyship.State.FIGHT and left < warn_time


func draw() -> void:
	var flicker := warning() and fmod(left * 12.0, 1.0) < 0.5
	_bubble.visible = on or flicker or burst > 0.0
	_bubble.scale = Vector3.ONE * (1.0 + burst * 0.6) # dropping: it swells and fades
	var pulse := 0.8 + 0.2 * sin(Time.get_ticks_msec() * 0.004)
	_bubble_material.set_shader_parameter("strength", pulse if on else (0.5 if flicker else burst * 2.0))
