## Moving-head spotlights hanging from the truss, each with a visible beam (disco_beam.gdshader),
## sweeping the floor in time with DiscoBeat. Shadowless spots and additive cone beams: no
## volumetric fog, so it stays cheap enough for mobile.
##
## Aim moves (change every bar):  sweep  — side to side, crossing
##                                circle — chasing round the floor
##                                centre — all beams snap onto the middle (the boss) on the beat
##                                scatter — each lamp jumps to a random tile on every beat
## A soft OmniLight in the middle fakes the floor's glow bouncing up (the boss will catch it).
class_name DiscoLights
extends Node3D

const P := preload("res://disco/disco_palette.gd")
const MOVES := ["sweep", "circle", "centre", "scatter"]
const ANGLE := 11.0 ## degrees, spot half-angle
const BEAM_LENGTH := 20.0
const AIM_EASE := 5.0 ## 1/s: how fast the heads turn toward their aim

var beat: DiscoBeat
var floor_half := 9.9 ## metres: aims stay inside the main floor
var hang_y := 12.0
var spots_xz: Array[Vector2] = [Vector2(-7.5, -7.5), Vector2(7.5, -7.5), Vector2(-7.5, 7.5), Vector2(7.5, 7.5)]
var move := "sweep"
var beams_visible := true:
	set(value):
		beams_visible = value
		for beam in _beams:
			beam.visible = value
var energy := 1.0

var _heads: Array[Node3D] = []
var _lights: Array[SpotLight3D] = []
var _beams: Array[MeshInstance3D] = []
var _aims: Array[Vector3] = [] ## where each head points now (eased)
var _scatter: Array[Vector3] = []
var _bounce: OmniLight3D


func _ready() -> void:
	for i in spots_xz.size():
		_add_head(i)
	_bounce = OmniLight3D.new()
	_bounce.position = Vector3(0.0, 1.5, 0.0)
	_bounce.omni_range = 14.0
	_bounce.light_energy = 1.0
	add_child(_bounce)
	if beat:
		beat.bar_hit.connect(func(bar: int) -> void: move = MOVES[bar % MOVES.size()])
		beat.beat_hit.connect(_on_beat)


func _process(delta: float) -> void:
	if beat == null:
		return
	var t := beat.time * beat.bpm / 60.0 # time in beats
	var thump := beat.pulse(5.0)
	var blend := 1.0 - exp(-AIM_EASE * delta)
	for i in _heads.size():
		_aims[i] = _aims[i].lerp(_aim(i, t), blend)
		_heads[i].look_at(_aims[i], Vector3.UP)
		var colour := P.pick(beat.bar() + i)
		_lights[i].light_color = colour
		_lights[i].light_energy = (6.0 + thump * 10.0) * energy
		var beam_material := _beams[i].material_override as ShaderMaterial
		beam_material.set_shader_parameter("color", colour)
		beam_material.set_shader_parameter("strength", (0.25 + thump * 0.35) * energy)
	_bounce.light_color = P.pick(beat.bar())
	_bounce.light_energy = (1.2 + thump * 1.8) * energy


# Where lamp `i` wants to point at time `t` (in beats), on the floor.
func _aim(i: int, t: float) -> Vector3:
	var side := -1.0 if i % 2 == 0 else 1.0
	var r := floor_half * 0.8
	match move:
		"sweep":
			return Vector3(sin(t * PI * 0.5 + i) * r * side, 0.0, cos(t * PI * 0.25 + i * 1.3) * r * 0.6)
		"circle":
			var angle := t * PI * 0.5 + i * TAU / _heads.size()
			return Vector3(cos(angle), 0.0, sin(angle)) * r * 0.75
		"centre":
			var spread := (1.0 - beat.pulse(3.0)) * 3.0
			return Vector3(spots_xz[i].x, 0.0, spots_xz[i].y).normalized() * spread
		"scatter":
			return _scatter[i]
	return Vector3.ZERO


func _on_beat(_beat: int) -> void:
	for i in _scatter.size():
		_scatter[i] = Vector3(randf_range(-1.0, 1.0), 0.0, randf_range(-1.0, 1.0)) * floor_half * 0.85


func _add_head(i: int) -> void:
	var head := Node3D.new()
	head.position = Vector3(spots_xz[i].x, hang_y, spots_xz[i].y)
	add_child(head)
	var housing := MeshInstance3D.new()
	var can := CylinderMesh.new()
	can.top_radius = 0.32
	can.bottom_radius = 0.4
	can.height = 0.8
	housing.mesh = can
	housing.rotation.x = PI * 0.5
	housing.material_override = P.surface(Color(0.1, 0.09, 0.13), 0.4, 0.7)
	head.add_child(housing)

	var light := SpotLight3D.new()
	light.spot_angle = ANGLE
	light.spot_range = 32.0
	light.spot_attenuation = 0.6
	light.shadow_enabled = false
	head.add_child(light)

	var cone := CylinderMesh.new()
	cone.top_radius = 0.18
	cone.bottom_radius = tan(deg_to_rad(ANGLE)) * BEAM_LENGTH
	cone.height = BEAM_LENGTH
	cone.cap_top = false
	cone.cap_bottom = false
	cone.radial_segments = 24
	var beam := MeshInstance3D.new()
	beam.mesh = cone
	beam.rotation.x = PI * 0.5 # the cone's top (+Y) at the lamp, its far end down -Z
	beam.position.z = -BEAM_LENGTH * 0.5
	beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var material := ShaderMaterial.new()
	material.shader = preload("res://disco/disco_beam.gdshader")
	material.set_shader_parameter("beam_length", BEAM_LENGTH)
	beam.material_override = material
	head.add_child(beam)

	_heads.append(head)
	_lights.append(light)
	_beams.append(beam)
	_aims.append(Vector3.ZERO)
	_scatter.append(Vector3.ZERO)
