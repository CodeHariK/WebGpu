## Applies a MoodTheme to the scene every frame — sky, fog, ambient, the sun or moon, the lamps,
## glow, colour grade, particles, vignette, the ground and path colours — and blends smoothly to
## the next theme (switch_to). `theme` is the target and is what the panel edits live.
##
## The levers, and what each does to a look:
##   sky + fog   the background gradient and the distance haze — fog hides the far world and turns
##               it into flat silhouettes; its colour is most of the mood
##   ambient     the colour of every shadow (purple at night, pink for candy, never black)
##   sun / moon  the one shadow-casting light: low = long shadows, golden; high = short, crisp
##   lamps       small cheap lights (no shadows) + emissive parts that bloom
##   glow        bloom on anything brighter than 1
##   grade       a 3-stop colour-correction gradient (shadows → mids → highlights) + saturation and
##               contrast: the single biggest step from "plain 3D" to "styled"
##   particles   embers, sparkles, sand, leaves, rain, pollen, dust motes, fireflies
##   vignette    darker screen edges
## Every layer can be switched off on its own (use_*), to see what it adds.
class_name MoodLook
extends Node

signal theme_changed(theme: MoodTheme)

const BLEND_TIME := 1.5 ## seconds to blend to a new theme

var theme: MoodTheme ## the target (edited live by the panel)
var shown: MoodTheme ## what's on screen this frame (a blend while switching)

# Layer switches.
var use_fog := true
var use_grade := true
var use_glow := true
var use_lanterns := true
var use_embers := true ## the particles, whatever the theme's are
var use_vignette := true
var use_ambient := true

var environment: Environment
var sky_material: ProceduralSkyMaterial
var sun: DirectionalLight3D
var lamps: Array = [] ## [OmniLight3D, StandardMaterial3D (its glowing part), Color tint or null]
var embers: GPUParticles3D
var vignette_material: ShaderMaterial
var ground_material := StandardMaterial3D.new()
var path_material := StandardMaterial3D.new()

var _from: MoodTheme
var _blend := 1.0
var _grade := Gradient.new()
var _time := 0.0


func _init() -> void:
	theme = MoodTheme.preset("spooky")
	shown = theme
	_from = theme


func _ready() -> void:
	_grade.offsets = PackedFloat32Array([0.0, 0.5, 1.0])
	_grade.colors = PackedColorArray([Color.BLACK, Color.GRAY, Color.WHITE])
	var texture := GradientTexture1D.new()
	texture.gradient = _grade
	environment.adjustment_color_correction = texture
	for material: StandardMaterial3D in [ground_material, path_material]:
		material.diffuse_mode = BaseMaterial3D.DIFFUSE_LAMBERT_WRAP
	MoodParticles.configure(embers, theme.particles)


## Blend to the preset `theme_name` over BLEND_TIME (the props switch at once: theme_changed).
func switch_to(theme_name: String) -> void:
	_from = shown
	theme = MoodTheme.preset(theme_name)
	_blend = 0.0
	MoodParticles.configure(embers, theme.particles)
	theme_changed.emit(theme)


## Register a lamp: its light, its glowing material, and an optional fixed tint (null = the theme's
## lamp colour) — e.g. alternating neon colours.
func add_lamp(light: OmniLight3D, glow_material: StandardMaterial3D, tint: Variant = null) -> void:
	lamps.append([light, glow_material, tint])


func _process(delta: float) -> void:
	_time += delta
	_blend = minf(_blend + delta / BLEND_TIME, 1.0)
	shown = theme if _blend >= 1.0 else MoodTheme.mix(_from, theme, smoothstep(0.0, 1.0, _blend))
	_apply(shown)


func _apply(t: MoodTheme) -> void:
	var env := environment
	sky_material.sky_top_color = t.sky_top
	sky_material.sky_horizon_color = t.sky_horizon
	sky_material.ground_horizon_color = t.fog if use_fog else t.sky_horizon
	sky_material.ground_bottom_color = t.fog.darkened(0.4)
	env.ambient_light_color = t.ambient if use_ambient else Color(0.5, 0.5, 0.5)
	env.ambient_light_energy = t.ambient_energy if use_ambient else 0.3
	env.fog_enabled = use_fog
	env.fog_light_color = t.fog
	env.fog_depth_begin = t.fog_begin
	env.fog_depth_end = t.fog_end
	env.fog_height_density = t.fog_height_density
	env.glow_enabled = use_glow
	env.glow_intensity = t.glow
	env.adjustment_saturation = t.saturation if use_grade else 1.0
	env.adjustment_contrast = t.contrast if use_grade else 1.0
	for i in 3:
		_grade.set_color(i, t.grade[i] if use_grade else Color(i * 0.5, i * 0.5, i * 0.5))
	env.ssr_enabled = t.ground_roughness < 0.4 # wet streets reflect the neon (Forward+ only)
	sun.light_color = t.sun
	sun.light_energy = t.sun_energy
	sun.rotation_degrees = Vector3(t.sun_pitch, t.sun_yaw, 0)
	for i in lamps.size():
		var lamp: Array = lamps[i]
		if not is_instance_valid(lamp[0]):
			continue
		var color: Color = lamp[2] if lamp[2] != null else t.lamp
		var flicker := 1.0 + t.lamp_flicker * (0.12 * sin(_time * 11.0 + i * 1.7) + 0.06 * sin(_time * 23.0 + i))
		var light: OmniLight3D = lamp[0]
		light.light_color = color
		light.light_energy = t.lamp_energy * flicker * (1.0 if use_lanterns else 0.0)
		light.omni_range = t.lamp_range
		light.visible = use_lanterns
		var glowing: StandardMaterial3D = lamp[1]
		glowing.albedo_color = color
		glowing.emission = color
		glowing.emission_energy_multiplier = (4.0 if use_lanterns else 0.3) * flicker
	embers.amount_ratio = clampf(t.particle_amount, 0.0, 1.0) if use_embers else 0.0
	embers.emitting = embers.amount_ratio > 0.01
	vignette_material.set_shader_parameter("strength", t.vignette if use_vignette else 0.0)
	ground_material.albedo_color = t.ground
	ground_material.roughness = t.ground_roughness
	ground_material.metallic_specular = 0.5 if t.ground_roughness > 0.5 else 0.9
	path_material.albedo_color = t.path
	path_material.roughness = maxf(t.ground_roughness, 0.3)
