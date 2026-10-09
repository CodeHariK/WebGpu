# Function lab: look inside each shader_lib function, one stage at a time.
#   1-4 / Up Down   pick the function      Left Right   previous / next stage
#   wheel           zoom                   drag         pan        R  reset view
# The top image is the function's output (black = 0, white = 1). The graph underneath plots the
# values along the red line; each light/dark stripe in the graph is one grid unit.
# Run: godot --path . res://shader_lib/lab/function_lab.tscn   (--capture-lab saves all 16 stages)
extends Control

const FUNCTIONS := [
	{
		"name": "hash(p)", "file": "hash.gdshaderinc", "span": 16.0,
		"stages": [
			["Cell number", "p = floor(p)  (shown here as x + y·16)",
			"Whole-number cell coordinates. Neighbouring cells have neighbouring numbers, so the image is a smooth ramp: completely orderly, nothing random yet."],
			["Squash", "p = fract(p * 0.3183099 + 0.1)",
			"Multiply by 1/π and keep only the decimals. Neat inputs 1, 2, 3 become messy values between 0 and 1. Each axis is squashed on its own (here we show x), so the image is just stripes: still not random."],
			["Scramble product", "p *= 17.0;  x·y·z·(x+y+z)",
			"Stretch to 0..17 and multiply everything together. The result is huge (up to ~250 000) and swings wildly between neighbouring cells. Shown on a log scale so it fits in 0..1."],
			["Keep the decimals", "return fract(x·y·z·(x+y+z))",
			"Throw away the whole part. The decimals of a wildly swinging number look random: one fixed random value per cell. Same input → same output every frame."],
		],
	},
	{
		"name": "noise(x)  value noise", "file": "value_noise.gdshaderinc", "span": 12.0,
		"stages": [
			["Corner values", "a = hash(floor(p))",
			"Every cell gets one random value from hash(). Square blocks: this is what the 4 (2D) or 8 (3D) corners hold before blending."],
			["Straight-line blend", "mix(mix(a, b, f.x), mix(c, d, f.x), f.y)",
			"Blend the 4 corner values by the position inside the cell, f = fract(p). Smooth, but look at the graph: sharp kinks at every grid line, which show up as creases."],
			["Smoothstep blend", "f = f·f·(3 − 2f)  then the same mix",
			"Ease the position before blending: slow near the corners, fast in the middle. The kinks are gone; the graph is a soft wave. This is noise()."],
			["Two sizes added", "noise(p)·0.7 + noise(p·3)·0.3",
			"Add a smaller copy for detail. Big blobs give shape, small ones give texture. The sand and water shaders stack noise like this."],
		],
	},
	{
		"name": "tex_noise2(x)  texture noise", "file": "texture_noise.gdshaderinc", "span": 12.0,
		"stages": [
			["Raw pixels", "texelFetch(noise_tex, ivec2(floor(p)), 0).r",
			"The noise texture is 128×128 random grey pixels (one channel). Reading exact pixels gives the same blocks as hash(), but it is 1 read instead of ~10 multiplies. This is tex_hash()."],
			["GPU blend only", "textureLod(noise_tex, (p + 0.5) / 128.0, 0.0).r",
			"Let the texture filter blend the 4 nearest pixels for free. That is the straight-line blend: the graph still has kinks at every pixel."],
			["Smoothstep trick", "f = smoothstep(fract(p));  read at floor(p) + f",
			"Move the sample point by the smoothstepped f before reading. The GPU's straight blend now eases in and out: the same soft wave as value noise, in ONE texture read."],
			["Maths noise, to compare", "noise(vec3(p, 0.0))   // 8 hash() calls",
			"The maths version from value_noise. Different random values, same look. Texture version: 1 read. Maths version: ~80 multiplies."],
		],
	},
	{
		"name": "ripple(p, spacing)", "file": "ripple.gdshaderinc", "span": 10.0,
		"stages": [
			["Distance along the wind", "d = p.x·0.35 + p.y·0.94   (shown as fract(d / 8))",
			"Project the position onto the wind direction. Points with the same d lie on a straight line across the beach. Shown wrapped every 8 m so it fits in 0..1."],
			["Stripes", "fract(d / spacing)",
			"Count ripples: fract climbs 0→1 once per ripple and snaps back. A sawtooth in the graph, straight parallel stripes `spacing` metres apart in the image."],
			["Bend with noise", "fract(d / spacing + warp)\nwarp = tex_noise2(p·0.35)·3 + tex_noise2(p·1.1)·0.6",
			"Shift each stripe forward/back by noise before fract. Big slow noise ×3 = sweeping curves, small noise ×0.6 = wiggles. Now they look like real ripples from above."],
			["Ripple shape", "pow(phase, 3.0)",
			"Cube the sawtooth: it stays low for a long time, rises steeply, then drops. Long gentle slope on the windward side, short steep face on the lee side: the real ripple profile."],
		],
	},
]

var fn := 0
var stage := 1
var span := 16.0
var offset := Vector2.ZERO
var mat: ShaderMaterial
var title_label: Label
var code_label: Label
var text_label: Label
var dragging := false


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mat = ShaderMaterial.new()
	mat.shader = load("res://shader_lib/lab/function_lab.gdshader")
	mat.set_shader_parameter("noise_tex", load("res://textures/noise_128.png"))  # Lossless, no mipmaps
	var rect := ColorRect.new()
	rect.material = mat
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(rect)

	var panel := PanelContainer.new()
	panel.position = Vector2(16, 16)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.05, 0.06, 0.09, 0.82)
	style.set_corner_radius_all(8)
	style.set_content_margin_all(14)
	panel.add_theme_stylebox_override("panel", style)
	add_child(panel)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 6)
	panel.add_child(box)
	title_label = Label.new()
	title_label.add_theme_font_size_override("font_size", 22)
	box.add_child(title_label)
	code_label = Label.new()
	code_label.add_theme_color_override("font_color", Color(1.0, 0.82, 0.45))
	code_label.add_theme_font_size_override("font_size", 16)
	box.add_child(code_label)
	text_label = Label.new()
	text_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	text_label.custom_minimum_size = Vector2(540, 0)
	text_label.add_theme_font_size_override("font_size", 15)
	box.add_child(text_label)
	var keys := Label.new()
	keys.text = "1-4 / ↑↓ function   ←→ stage   wheel zoom   drag pan   R reset"
	keys.add_theme_color_override("font_color", Color(0.6, 0.65, 0.75))
	keys.add_theme_font_size_override("font_size", 13)
	box.add_child(keys)

	_select(0, 1)
	if "--capture-lab" in OS.get_cmdline_user_args() or "--capture-lab" in OS.get_cmdline_args():
		_capture_all()


func _select(f: int, s: int) -> void:
	if wrapi(f, 0, FUNCTIONS.size()) != fn or span <= 0.0:
		offset = Vector2.ZERO
	fn = wrapi(f, 0, FUNCTIONS.size())
	stage = clampi(s, 1, 4)
	span = FUNCTIONS[fn]["span"]
	_refresh()


func _refresh() -> void:
	var info: Dictionary = FUNCTIONS[fn]
	var st: Array = info["stages"][stage - 1]
	title_label.text = "%s   stage %d/4 — %s" % [info["name"], stage, st[0]]
	code_label.text = st[1]
	text_label.text = st[2] + "\n(shader_lib/%s)" % info["file"]
	mat.set_shader_parameter("fn", fn)
	mat.set_shader_parameter("stage", stage)
	mat.set_shader_parameter("span", span)
	mat.set_shader_parameter("offset", offset)
	var s := get_viewport_rect().size
	mat.set_shader_parameter("aspect", s.x / maxf(1.0, s.y * 0.74))


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_1, KEY_2, KEY_3, KEY_4:
				_select(event.keycode - KEY_1, 1)
			KEY_UP:
				_select(fn - 1, 1)
			KEY_DOWN:
				_select(fn + 1, 1)
			KEY_RIGHT:
				stage = mini(stage + 1, 4)
			KEY_LEFT:
				stage = maxi(stage - 1, 1)
			KEY_R:
				span = FUNCTIONS[fn]["span"]
				offset = Vector2.ZERO
			KEY_ESCAPE:
				get_tree().quit()
		_refresh()
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			span = maxf(1.0, span / 1.15)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			span = minf(400.0, span * 1.15)
		elif event.button_index == MOUSE_BUTTON_LEFT:
			dragging = event.pressed
		_refresh()
	elif event is InputEventMouseMotion and dragging:
		var w := get_viewport_rect().size.x
		offset += Vector2(-event.relative.x, event.relative.y) * span / w
		_refresh()


func _capture_all() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots/lab"))
	for f in FUNCTIONS.size():
		for s in range(1, 5):
			_select(f, s)
			for i in 3:
				await RenderingServer.frame_post_draw
			var img := get_viewport().get_texture().get_image()
			img.save_png("res://shots/lab/lab_%d_%d.png" % [f + 1, s])
	print("lab capture done")
	get_tree().quit()
