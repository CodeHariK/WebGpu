## Shadow is safe: the whole arena floor turns red — everywhere except the round shadow right under
## the ship (rimmed in cyan). Then laser strikes rain down on everything outside it. The ship
## doesn't hold still: it drifts slowly to one spot after another across the arena, so you keep
## walking under it to stay in its shadow (and the bots, who also stand there, are in your way).
##
## The red floor is one big plane with a small shader (the safe circle cut out, stripes crawling
## over the danger); the strikes are a pool of thin beams flashed at random spots outside the
## circle. Outside the circle while FIRE runs, you get hurt (the demo's i-frames pace it).
##
## params: safe (m, the shadow's radius), drift (m/s the ship moves), hop (s before it picks a
##         new spot), rate (strikes per second), damage, + the stage lengths.
class_name ShadowLaser
extends ShipAttack

const STRIKES := 40
const SHADER := """
shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, shadows_disabled;
uniform vec3 safe_center;
uniform float safe_radius = 3.0;
uniform float arena = 15.5;
uniform float strength = 0.0;
uniform float firing = 0.0;
uniform float clock = 0.0;
varying vec3 world;
void vertex() { world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	if (length(world.xz) > arena) discard;
	float d = distance(world.xz, safe_center.xz);
	float rim = smoothstep(safe_radius - 0.18, safe_radius, d) - smoothstep(safe_radius, safe_radius + 0.18, d);
	float outside = smoothstep(safe_radius, safe_radius + 0.35, d);
	float stripes = 0.5 + 0.5 * sin((world.x + world.z) * 2.5 - clock * 7.0);
	float danger = outside * strength * (0.22 + 0.18 * stripes + 0.3 * firing);
	float shade = (1.0 - outside) * strength * 0.35;
	ALBEDO = mix(mix(vec3(1.0, 0.1, 0.04), vec3(0.0), 1.0 - outside), vec3(0.3, 0.9, 1.0), rim);
	ALPHA = clamp(max(max(danger, shade), rim * strength), 0.0, 1.0);
}
"""

var _floor: MeshInstance3D
var _material := ShaderMaterial.new()
var _strikes: Array = [] ## [MeshInstance3D, life left]
var _goal := Vector3.ZERO
var _hop := 0.0
var _rain := 0.0 ## strikes owed (fractional)


func _init() -> void:
	key = "shadow"
	title = "shadow is safe"


func _build() -> void:
	var shader := Shader.new()
	shader.code = SHADER
	_material.shader = shader
	var plane := PlaneMesh.new()
	plane.size = Vector2.ONE * (SkyshipArena.RADIUS * 2.0 + 1.0)
	_floor = ShipKit.add(self, plane, _material, Vector3.UP * 0.035)
	_floor.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_material.set_shader_parameter("arena", SkyshipArena.RADIUS)
	var beam_mesh := ShipKit.cylinder(0.18, 0.18, 1.0, 8)
	var beam_material := ShipKit.glow(Color(1.0, 0.3, 0.1), 6.0)
	for i in STRIKES:
		var beam := ShipKit.add(self, beam_mesh, beam_material)
		beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		beam.visible = false
		_strikes.append([beam, 0.0])
	_on_stop()


func _on_begin(_p: Dictionary) -> void:
	_floor.visible = true
	_hop = 0.0
	_rain = 0.0


func _tick(now: Stage, t: float, delta: float) -> void:
	var safe := float(params.get("safe", 3.0))
	var strength := 1.0
	match now:
		Stage.TELEGRAPH: strength = t
		Stage.RECOVER: strength = 1.0 - t
	if now != Stage.RECOVER:
		_wander(delta)
	_material.set_shader_parameter("safe_center", ship.ground_point())
	_material.set_shader_parameter("safe_radius", safe)
	_material.set_shader_parameter("strength", strength)
	_material.set_shader_parameter("firing", 1.0 if now == Stage.FIRE else 0.0)
	_material.set_shader_parameter("clock", time)
	for strike: Array in _strikes:
		strike[1] -= delta
		(strike[0] as Node3D).visible = strike[1] > 0.0
	if now != Stage.FIRE:
		return
	_rain += float(params.get("rate", 22.0)) * delta
	while _rain >= 1.0:
		_rain -= 1.0
		_strike(safe)
	if ShipKit.flat(demo.player).distance_to(ship.ground_point()) > safe:
		demo.hurt(int(params.get("damage", 12)), "outside the shadow")


# The ship heads for one spot after another (a fresh one every `hop` s, well away from where it is).
func _wander(delta: float) -> void:
	_hop -= delta
	if _hop <= 0.0:
		_hop = float(params.get("hop", 2.6))
		var here := ship.ground_point()
		for attempt in 12:
			_goal = SkyshipArena.random_point(SkyshipArena.RADIUS - 8.0)
			if _goal.distance_to(here) > 14.0:
				break
	ship.drift(_goal, float(params.get("drift", 2.2)))


func _strike(safe: float) -> void:
	var center := ship.ground_point()
	var at := Vector3.ZERO
	for attempt in 8:
		at = SkyshipArena.random_point(SkyshipArena.RADIUS - 0.3)
		if at.distance_to(center) > safe + 0.4:
			break
	for strike: Array in _strikes:
		if strike[1] <= 0.0:
			strike[1] = 0.12
			ShipKit.beam(strike[0], at + Vector3.UP * 30.0, at, 1.0)
			(strike[0] as Node3D).visible = true
			if ship.sparks != null:
				ship.sparks.burst(at, Vector3.UP, 5, 4.0, 0.8, 0.35)
			return


func _on_stop() -> void:
	if ship != null:
		ship.drift(Vector3.INF, 0.0)
	_floor.visible = false
	for strike: Array in _strikes:
		strike[1] = 0.0
		(strike[0] as Node3D).visible = false
