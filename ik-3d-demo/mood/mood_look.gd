## The whole "Pumpkin Jack" mood in one place: environment (fog, ambient, glow, colour grade), the
## moon / sun, the lanterns, embers and vignette — blended between a plain DAY look and the tunable
## NIGHT look by `night` (0 … 1). Every layer can be switched off on its own, to see what it adds.
##
## The night look, layer by layer:
##   fog       depth fog in a warm magenta, thick from fog_begin to fog_end, plus height fog lying
##             low on the ground — hides the distance and turns far trees into flat silhouettes
##   ambient   cold purple at low energy: every shadow is purple, never black or grey
##   moon      one low, cool directional light — the only one casting shadows
##   lanterns  short-range orange omni lights (no shadows, cheap) + emissive flames for the bloom
##   glow      bloom on anything brighter than 1 (flames, embers, eyes)
##   grade     a colour-correction gradient: dark violet shadows → magenta mids → orange highlights,
##             plus a little extra saturation and contrast — this one step is most of the look
##   embers    drifting orange sparks around the camera        vignette  darkened screen edges
## All of it works on Godot's Mobile renderer (no volumetric fog, no SDFGI).
class_name MoodLook
extends Node

const DAY := {
	"background": Color(0.55, 0.72, 0.92), "ambient": Color(0.95, 0.92, 0.85), "ambient_energy": 1.0,
	"fog": Color(0.7, 0.8, 0.92), "fog_begin": 40.0, "fog_end": 160.0, "fog_height_density": 0.0,
	"sun": Color(1.0, 0.95, 0.85), "sun_energy": 1.3, "sun_pitch": -55.0,
	"glow": 0.3, "saturation": 1.1, "contrast": 1.0, "lanterns": 0.0, "embers": 0.0, "vignette": 0.15,
	"grade": [Color(0.0, 0.0, 0.0), Color(0.5, 0.5, 0.5), Color(1.0, 1.0, 1.0)],
}

# Night, tunable from the panel.
var background := Color(0.32, 0.07, 0.18)
var ambient := Color(0.36, 0.2, 0.58)
var ambient_energy := 0.6
var fog_hue := 0.94 ## the fog colour's hue (0 … 1): 0.94 magenta-red, 0.05 orange, 0.75 violet
var fog_saturation := 0.72
var fog_brightness := 0.6
var fog_begin := 9.0
var fog_end := 55.0
var fog_height_density := 0.06
var moon := Color(0.75, 0.7, 1.0)
var moon_energy := 0.5
var moon_pitch := -22.0 ## degrees: low, long shadows
var glow := 1.0
var saturation := 1.25
var contrast := 1.12
var lantern_energy := 3.0
var lantern_range := 4.5
var ember_amount := 1.0
var vignette := 0.55
var grade_colors := [Color(0.08, 0.02, 0.16), Color(0.55, 0.27, 0.5), Color(1.0, 0.8, 0.5)]

# Layer switches.
var use_fog := true
var use_grade := true
var use_glow := true
var use_lanterns := true
var use_embers := true
var use_vignette := true
var use_ambient := true

var night := 1.0 ## 0 = day … 1 = night
var environment: Environment
var sun: DirectionalLight3D
var lanterns: Array[OmniLight3D] = []
var flames: Array[StandardMaterial3D] = []
var embers: GPUParticles3D
var vignette_material: ShaderMaterial

var _grade := Gradient.new()
var _time := 0.0
var _blend_to := -1.0 ## animating toward this (−1 = not animating)


func _ready() -> void:
	_grade.offsets = PackedFloat32Array([0.0, 0.5, 1.0])
	_grade.colors = PackedColorArray([Color.BLACK, Color.GRAY, Color.WHITE])
	var texture := GradientTexture1D.new()
	texture.gradient = _grade
	environment.adjustment_color_correction = texture


## Ease to day (0) or night (1) over a couple of seconds.
func blend_to(value: float) -> void:
	_blend_to = value


func _process(delta: float) -> void:
	_time += delta
	if _blend_to >= 0.0:
		night = move_toward(night, _blend_to, delta * 0.5)
		if is_equal_approx(night, _blend_to):
			_blend_to = -1.0
	_apply()


func fog_color() -> Color:
	return Color.from_hsv(fog_hue, fog_saturation, fog_brightness)


func _apply() -> void:
	var t := smoothstep(0.0, 1.0, night)
	var env := environment
	env.background_color = DAY.background.lerp(background if use_fog else Color(0.05, 0.04, 0.1), t)
	env.ambient_light_color = DAY.ambient.lerp(ambient if use_ambient else Color(0.5, 0.5, 0.5), t)
	env.ambient_light_energy = lerpf(DAY.ambient_energy, ambient_energy if use_ambient else 0.25, t)
	env.fog_enabled = use_fog or t < 0.99
	env.fog_light_color = DAY.fog.lerp(fog_color(), t)
	env.fog_depth_begin = lerpf(DAY.fog_begin, fog_begin if use_fog else 400.0, t)
	env.fog_depth_end = lerpf(DAY.fog_end, fog_end if use_fog else 800.0, t)
	env.fog_height_density = lerpf(DAY.fog_height_density, fog_height_density if use_fog else 0.0, t)
	env.glow_enabled = use_glow or t < 0.01
	env.glow_intensity = lerpf(DAY.glow, glow, t)
	env.adjustment_saturation = lerpf(DAY.saturation, saturation if use_grade else 1.0, t)
	env.adjustment_contrast = lerpf(DAY.contrast, contrast if use_grade else 1.0, t)
	for i in 3:
		_grade.set_color(i, DAY.grade[i].lerp(grade_colors[i] if use_grade else DAY.grade[i], t))
	sun.light_color = DAY.sun.lerp(moon, t)
	sun.light_energy = lerpf(DAY.sun_energy, moon_energy, t)
	sun.rotation_degrees.x = lerpf(DAY.sun_pitch, moon_pitch, t)
	var lit := t * (1.0 if use_lanterns else 0.0)
	for i in lanterns.size():
		var flicker := 1.0 + 0.12 * sin(_time * 11.0 + i * 1.7) + 0.06 * sin(_time * 23.0 + i)
		lanterns[i].light_energy = lantern_energy * lit * flicker
		lanterns[i].omni_range = lantern_range
		lanterns[i].visible = lit > 0.01
		flames[i].emission_energy_multiplier = lerpf(0.2, 4.0, lit) * flicker
	embers.amount_ratio = clampf(ember_amount * t * (1.0 if use_embers else 0.0), 0.0, 1.0)
	embers.emitting = embers.amount_ratio > 0.01
	vignette_material.set_shader_parameter("strength", lerpf(DAY.vignette, vignette if use_vignette else 0.0, t))
