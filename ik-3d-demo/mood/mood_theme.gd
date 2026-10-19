## One look for the mood demo: every number MoodLook applies (sky, fog, ambient, sun or moon,
## lamps, glow, colour grade, particles, vignette) plus the ground and path colours and which prop
## kit (MoodKit) dresses the scene. The same controls make every theme — only the values change.
## The camera is part of the look too (cam_*): how wide, how far, how high and steep, how loosely
## it follows, how much it sways and rolls, depth of field (far blur; near + far = tilt-shift
## miniature) and lens aberration — MoodCamera reads them.
## mix() blends two themes (numbers and colours lerp; the kit and particle style switch over).
class_name MoodTheme
extends RefCounted

const NAMES := ["spooky", "candy", "desert", "autumn", "cyberpunk", "solarpunk", "medieval", "magical", "toybox", "skyislands", "jungle", "cloud", "toxic"]
## Fields mix() blends (floats and colours lerp, the grade lerps per stop).
const BLENDED := ["sky_top", "sky_horizon", "fog", "fog_begin", "fog_end", "fog_height_density", "ambient",
	"ambient_energy", "sun", "sun_energy", "sun_pitch", "sun_yaw", "glow", "saturation", "contrast", "lamp",
	"lamp_energy", "lamp_range", "lamp_flicker", "particle_amount", "vignette", "ground", "ground_roughness", "path",
	"cam_fov", "cam_distance", "cam_height", "cam_pitch", "cam_lag", "cam_sway", "cam_roll", "cam_dof_far", "cam_dof_near", "cam_aberration"]

var name := "spooky"
var kit := "spooky" ## MoodKit prop set
var sky_top := Color(0.2, 0.04, 0.12)
var sky_horizon := Color(0.55, 0.12, 0.3)
var fog := Color(0.6, 0.17, 0.32)
var fog_begin := 9.0
var fog_end := 55.0
var fog_height_density := 0.06
var ambient := Color(0.36, 0.2, 0.58)
var ambient_energy := 0.6
var sun := Color(0.75, 0.7, 1.0) ## sun or moon
var sun_energy := 0.5
var sun_pitch := -22.0 ## degrees: low = long shadows
var sun_yaw := 150.0
var glow := 1.0
var saturation := 1.25
var contrast := 1.12
var grade: Array[Color] = [Color(0.08, 0.02, 0.16), Color(0.55, 0.27, 0.5), Color(1.0, 0.8, 0.5)] ## shadows, mids, highlights
var lamp := Color(1.0, 0.55, 0.25) ## the lamps' light (lanterns, torches, neon, crystals …)
var lamp_energy := 3.0
var lamp_range := 4.5
var lamp_flicker := 1.0 ## 0 = steady (neon, solar) … 1 = a candle flame
var particles := "embers" ## MoodLook particle style
var particle_amount := 1.0
var vignette := 0.55
var ground := Color(0.24, 0.17, 0.2)
var ground_roughness := 1.0 ## low = wet, shiny (cyberpunk streets)
var path := Color(0.42, 0.27, 0.18)
# Camera.
var cam_fov := 50.0 ## degrees: narrow = tense, cinematic … wide = open, fast
var cam_distance := 5.5 ## metres behind the character
var cam_height := 2.2 ## metres above it (the point the arm swings from)
var cam_pitch := 16.0 ## degrees looking down: low = things loom, high = overview
var cam_lag := 3.0 ## 1/s follow speed: low = floaty, dreamy … high = snappy
var cam_sway := 0.5 ## handheld wobble: 0 = locked … 1 = uneasy
var cam_roll := 2.0 ## degrees of slow drifting Dutch tilt (0 = level)
var cam_dof_far := 0.0 ## metres where the background starts to blur (0 = off)
var cam_dof_near := 0.0 ## metres inside which the foreground blurs (0 = off; with far: tilt-shift)
var cam_aberration := 0.15 ## colour fringing at the screen edges (0 = clean lens)


static func preset(theme_name: String) -> MoodTheme:
	var t := MoodTheme.new()
	t.name = theme_name
	t.kit = theme_name
	match theme_name:
		"candy": # pastel and glossy: pink-tinted shadows, never black; bright soft light, sparkles
			t._set_sky(Color(0.45, 0.7, 1.0), Color(1.0, 0.8, 0.9), Color(1.0, 0.82, 0.92), 35.0, 170.0, 0.0)
			t._set_light(Color(1.0, 0.75, 0.92), 0.75, Color(1.0, 0.96, 0.9), 1.15, -50.0, 40.0)
			t._set_post(0.5, 1.45, 1.1, [Color(0.28, 0.1, 0.3), Color(0.6, 0.45, 0.6), Color(1.0, 0.97, 0.95)], 0.12)
			t._set_lamps(Color(1.0, 0.5, 0.8), 1.2, 4.0, 0.0, "sparkles", 0.7)
			t._set_ground(Color(0.62, 0.9, 0.7), 0.6, Color(1.0, 0.7, 0.82))
		"desert": # hard bright sun, warm haze, sandstone; blowing sand
			t._set_sky(Color(0.22, 0.48, 0.9), Color(0.96, 0.82, 0.62), Color(0.95, 0.78, 0.55), 22.0, 120.0, 0.12)
			t._set_light(Color(0.8, 0.62, 0.55), 0.8, Color(1.0, 0.9, 0.7), 1.6, -40.0, 60.0)
			t._set_post(0.4, 1.15, 1.08, [Color(0.14, 0.06, 0.1), Color(0.56, 0.45, 0.37), Color(1.0, 0.95, 0.85)], 0.25)
			t._set_lamps(Color(1.0, 0.6, 0.25), 1.5, 4.0, 1.0, "sand", 0.8)
			t._set_ground(Color(0.86, 0.66, 0.42), 1.0, Color(0.72, 0.44, 0.3))
		"autumn": # low golden backlight through orange trees, morning mist, falling leaves
			t._set_sky(Color(0.45, 0.6, 0.82), Color(0.96, 0.8, 0.62), Color(0.92, 0.75, 0.56), 14.0, 80.0, 0.15)
			t._set_light(Color(0.72, 0.66, 0.72), 0.8, Color(1.0, 0.76, 0.48), 1.4, -18.0, 200.0)
			t._set_post(0.7, 1.12, 1.05, [Color(0.12, 0.06, 0.08), Color(0.56, 0.45, 0.35), Color(1.0, 0.88, 0.65)], 0.3)
			t._set_lamps(Color(1.0, 0.65, 0.3), 1.5, 4.0, 1.0, "leaves", 1.0)
			t._set_ground(Color(0.45, 0.34, 0.2), 1.0, Color(0.52, 0.45, 0.38))
		"cyberpunk": # near-black, neon does all the lighting, wet shiny ground, rain
			t._set_sky(Color(0.02, 0.02, 0.06), Color(0.16, 0.05, 0.26), Color(0.12, 0.05, 0.22), 6.0, 45.0, 0.2)
			t._set_light(Color(0.15, 0.25, 0.42), 0.5, Color(0.4, 0.6, 1.0), 0.15, -50.0, 120.0)
			t._set_post(1.6, 1.3, 1.2, [Color(0.0, 0.05, 0.08), Color(0.35, 0.3, 0.5), Color(1.0, 0.75, 0.95)], 0.5)
			t._set_lamps(Color(1.0, 0.2, 0.8), 4.0, 6.0, 0.15, "rain", 1.0)
			t._set_ground(Color(0.08, 0.08, 0.1), 0.12, Color(0.13, 0.13, 0.16))
		"solarpunk": # clean, bright, green and white; drifting pollen
			t._set_sky(Color(0.35, 0.65, 1.0), Color(0.86, 0.95, 1.0), Color(0.86, 0.95, 0.96), 32.0, 150.0, 0.0)
			t._set_light(Color(0.86, 0.95, 0.9), 1.1, Color(1.0, 0.97, 0.88), 1.4, -60.0, 30.0)
			t._set_post(0.5, 1.2, 1.0, [Color(0.08, 0.12, 0.1), Color(0.5, 0.55, 0.5), Color(1.0, 1.0, 0.95)], 0.1)
			t._set_lamps(Color(0.9, 1.0, 0.9), 0.8, 4.0, 0.0, "pollen", 0.7)
			t._set_ground(Color(0.42, 0.68, 0.32), 1.0, Color(0.92, 0.9, 0.83))
		"medieval": # a fairytale day: warm sun, green hills, cobbles, torches; dust motes
			t._set_sky(Color(0.4, 0.6, 0.88), Color(0.86, 0.88, 0.9), Color(0.76, 0.82, 0.88), 22.0, 110.0, 0.1)
			t._set_light(Color(0.75, 0.78, 0.86), 0.9, Color(1.0, 0.92, 0.78), 1.3, -38.0, 120.0)
			t._set_post(0.5, 1.1, 1.05, [Color(0.06, 0.06, 0.1), Color(0.5, 0.5, 0.5), Color(1.0, 0.97, 0.9)], 0.2)
			t._set_lamps(Color(1.0, 0.55, 0.2), 1.5, 4.0, 1.0, "dust", 0.5)
			t._set_ground(Color(0.38, 0.55, 0.28), 1.0, Color(0.56, 0.53, 0.49))
		"magical": # deep blue night lit by glowing crystals and mushrooms; fireflies
			t._set_sky(Color(0.04, 0.04, 0.18), Color(0.2, 0.15, 0.45), Color(0.2, 0.25, 0.55), 8.0, 60.0, 0.15)
			t._set_light(Color(0.25, 0.3, 0.6), 0.7, Color(0.6, 0.7, 1.0), 0.3, -60.0, 150.0)
			t._set_post(1.4, 1.3, 1.1, [Color(0.02, 0.04, 0.12), Color(0.35, 0.4, 0.6), Color(0.85, 1.0, 0.95)], 0.45)
			t._set_lamps(Color(0.3, 0.9, 1.0), 3.0, 5.0, 0.3, "fireflies", 1.0)
			t._set_ground(Color(0.12, 0.15, 0.25), 1.0, Color(0.3, 0.35, 0.5))
		"toybox": # a giant kid's room: warm window light, primary colours, dusty air, confetti
			t._set_sky(Color(0.95, 0.88, 0.75), Color(1.0, 0.93, 0.8), Color(0.98, 0.9, 0.78), 30.0, 130.0, 0.0)
			t._set_light(Color(0.95, 0.88, 0.8), 0.9, Color(1.0, 0.9, 0.72), 1.4, -35.0, 70.0)
			t._set_post(0.5, 1.3, 1.05, [Color(0.15, 0.1, 0.12), Color(0.55, 0.5, 0.45), Color(1.0, 0.97, 0.9)], 0.25)
			t._set_lamps(Color(1.0, 0.85, 0.5), 1.5, 4.0, 0.0, "confetti", 0.6)
			t._set_ground(Color(0.3, 0.5, 0.85), 1.0, Color(0.6, 0.42, 0.28))
		"skyislands": # high in the sky: pastel gradient, a sea of cloud below, floating rocks
			t._set_sky(Color(0.3, 0.5, 0.95), Color(1.0, 0.82, 0.78), Color(0.9, 0.88, 0.98), 40.0, 170.0, 0.3)
			t._set_light(Color(0.85, 0.85, 1.0), 1.0, Color(1.0, 0.92, 0.82), 1.3, -25.0, 230.0)
			t._set_post(0.6, 1.2, 1.0, [Color(0.15, 0.15, 0.3), Color(0.55, 0.55, 0.6), Color(1.0, 0.97, 0.95)], 0.15)
			t._set_lamps(Color(1.0, 0.75, 0.45), 1.5, 4.0, 0.6, "feathers", 0.5)
			t._set_ground(Color(0.85, 0.86, 0.96), 1.0, Color(0.55, 0.4, 0.28))
		"jungle": # a temple in the jungle: green filtered light, humid haze, shafts, buzzing motes
			t._set_sky(Color(0.35, 0.6, 0.5), Color(0.75, 0.85, 0.6), Color(0.55, 0.7, 0.45), 8.0, 65.0, 0.25)
			t._set_light(Color(0.5, 0.7, 0.5), 0.8, Color(1.0, 0.95, 0.7), 1.3, -55.0, 100.0)
			t._set_post(0.6, 1.2, 1.1, [Color(0.04, 0.1, 0.06), Color(0.4, 0.52, 0.35), Color(1.0, 0.98, 0.8)], 0.35)
			t._set_lamps(Color(1.0, 0.6, 0.25), 1.8, 4.0, 1.0, "jungle", 0.8)
			t._set_ground(Color(0.25, 0.4, 0.18), 1.0, Color(0.5, 0.52, 0.42))
		"cloud": # the cloud kingdom: white and gold, soft bloom everywhere, golden glitter
			t._set_sky(Color(0.5, 0.72, 1.0), Color(1.0, 0.94, 0.85), Color(0.98, 0.93, 0.85), 40.0, 170.0, 0.08)
			t._set_light(Color(0.85, 0.85, 1.0), 0.8, Color(1.0, 0.93, 0.78), 1.1, -45.0, 60.0)
			t._set_post(0.6, 1.25, 1.08, [Color(0.2, 0.18, 0.3), Color(0.6, 0.58, 0.6), Color(1.0, 0.96, 0.86)], 0.12)
			t._set_lamps(Color(1.0, 0.85, 0.45), 2.0, 4.5, 0.2, "glitter", 0.8)
			t._set_ground(Color(0.88, 0.9, 0.98), 1.0, Color(0.95, 0.75, 0.38))
		"toxic": # an industrial wasteland: sickly green-yellow haze, rust, glowing ooze, falling ash
			t._set_sky(Color(0.3, 0.32, 0.22), Color(0.6, 0.62, 0.35), Color(0.48, 0.52, 0.28), 6.0, 50.0, 0.3)
			t._set_light(Color(0.45, 0.5, 0.35), 0.7, Color(0.9, 0.95, 0.7), 0.6, -30.0, 300.0)
			t._set_post(1.0, 0.9, 1.15, [Color(0.05, 0.07, 0.03), Color(0.42, 0.45, 0.3), Color(0.95, 1.0, 0.75)], 0.5)
			t._set_lamps(Color(0.85, 1.0, 0.35), 2.5, 5.0, 1.0, "ash", 1.0)
			t._set_ground(Color(0.28, 0.26, 0.22), 0.7, Color(0.42, 0.42, 0.4))
	match theme_name:
		"candy": t._set_camera(65.0, 8.5, 4.5, 26.0, 2.2, 0.0, 0.0, 22.0, 0.0, 0.0)
		"desert": t._set_camera(72.0, 10.0, 5.0, 20.0, 1.5, 0.2, 0.0, 0.0, 0.0, 0.0)
		"autumn": t._set_camera(58.0, 6.5, 2.0, 10.0, 2.5, 0.15, 0.0, 25.0, 0.0, 0.0)
		"cyberpunk": t._set_camera(60.0, 5.0, 1.4, 4.0, 5.0, 0.1, 0.0, 0.0, 0.0, 0.6)
		"solarpunk": t._set_camera(65.0, 8.0, 4.0, 22.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0)
		"medieval": t._set_camera(60.0, 7.0, 3.5, 20.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0)
		"magical": t._set_camera(66.0, 7.0, 3.0, 15.0, 1.4, 0.35, 0.0, 18.0, 0.0, 0.15)
		"toybox": t._set_camera(50.0, 8.0, 6.0, 38.0, 2.5, 0.0, 0.0, 13.0, 6.0, 0.0) # tilt-shift miniature
		"skyislands": t._set_camera(75.0, 10.0, 4.5, 24.0, 1.8, 0.25, 0.0, 0.0, 0.0, 0.0)
		"jungle": t._set_camera(55.0, 5.0, 2.0, 12.0, 3.0, 0.3, 0.0, 20.0, 0.0, 0.0)
		"cloud": t._set_camera(68.0, 8.0, 3.5, 18.0, 1.2, 0.2, 0.0, 22.0, 0.0, 0.05)
		"toxic": t._set_camera(52.0, 5.5, 2.3, 15.0, 3.0, 0.8, 4.0, 0.0, 0.0, 0.5)
	return t


## A blend of `a` and `b` (t = 0 … 1). The kit and particles are b's from halfway.
static func mix(a: MoodTheme, b: MoodTheme, t: float) -> MoodTheme:
	var out := MoodTheme.new()
	for field: String in BLENDED:
		out.set(field, lerp(a.get(field), b.get(field), t))
	for i in 3:
		out.grade[i] = a.grade[i].lerp(b.grade[i], t)
	var later := b if t >= 0.5 else a
	out.name = later.name
	out.kit = later.kit
	out.particles = later.particles
	return out


func _set_sky(top: Color, horizon: Color, fog_color: Color, begin: float, end: float, height: float) -> void:
	sky_top = top
	sky_horizon = horizon
	fog = fog_color
	fog_begin = begin
	fog_end = end
	fog_height_density = height


func _set_light(ambient_color: Color, ambient_power: float, sun_color: Color, sun_power: float, pitch: float, yaw: float) -> void:
	ambient = ambient_color
	ambient_energy = ambient_power
	sun = sun_color
	sun_energy = sun_power
	sun_pitch = pitch
	sun_yaw = yaw


func _set_post(bloom: float, sat: float, con: float, stops: Array, edge: float) -> void:
	glow = bloom
	saturation = sat
	contrast = con
	for i in 3:
		grade[i] = stops[i]
	vignette = edge


func _set_lamps(color: Color, energy: float, reach: float, flicker: float, style: String, amount: float) -> void:
	lamp = color
	lamp_energy = energy
	lamp_range = reach
	lamp_flicker = flicker
	particles = style
	particle_amount = amount


func _set_camera(fov: float, distance: float, height: float, pitch: float, lag: float, sway: float, roll: float, dof_far: float, dof_near: float, aberration: float) -> void:
	cam_fov = fov
	cam_distance = distance
	cam_height = height
	cam_pitch = pitch
	cam_lag = lag
	cam_sway = sway
	cam_roll = roll
	cam_dof_far = dof_far
	cam_dof_near = dof_near
	cam_aberration = aberration


func _set_ground(color: Color, roughness: float, path_color: Color) -> void:
	ground = color
	ground_roughness = roughness
	path = path_color
