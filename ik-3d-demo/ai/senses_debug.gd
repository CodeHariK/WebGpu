## Draws a HunterBrain's senses, on top of everything:
##   the view cone (fan to view_range) coloured by awareness: grey unaware, yellow suspicious,
##   orange searching, red alert — solid lines to each player point it can see
##   magenta cross = last known position (+ arrow: where it thinks you were going)
##   yellow cross = what it's looking at / checking      orange squares = spots left to search
##   white rings = noises (NoiseBus), fading
## and a label over the spider: state + suspicion bar.
class_name SensesDebug
extends MeshInstance3D

const COLORS: Array[Color] = [Color(0.7, 0.7, 0.7, 0.6), Color(1.0, 0.9, 0.2), Color(1.0, 0.55, 0.1), Color(1.0, 0.15, 0.1)] ## per awareness (enum order)

var brain: HunterBrain

var _lines := ImmediateMesh.new()
var _label := Label3D.new()


func _ready() -> void:
	mesh = _lines
	top_level = true
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	material_override = TacticalGridDebug._material()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.no_depth_test = true
	_label.font_size = 48
	_label.pixel_size = 0.006
	_label.outline_size = 12
	add_child(_label)
	_label.top_level = true


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	var on := visible and brain != null and brain.enabled and brain.spider != null
	_label.visible = on
	if not on:
		return
	var senses := brain.senses
	var color := COLORS[senses.awareness]
	_label.global_position = brain.spider.global_position + Vector3.UP * 1.3
	_label.modulate = color
	var filled := roundi(senses.suspicion * 10.0)
	_label.text = "%s\n%s" % [brain.state, "▮".repeat(filled) + "▯".repeat(10 - filled)]
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	_draw_cone(senses, color)
	for point in brain.player_points(): # rays to the player's points: solid = seen
		var to_point := point - senses.eye
		if to_point.length() <= senses.view_range and senses.forward.angle_to(to_point) <= senses.view_angle * 0.5:
			var seen := brain.grid.ray(senses.eye, point).is_empty()
			_line(senses.eye, point, Color(color, 0.9) if seen else Color(0.4, 0.4, 0.4, 0.3))
	if senses.last_known != Vector3.INF:
		_cross(senses.last_known, 0.35, Color(1.0, 0.2, 1.0))
		var heading := Vector3(senses.last_known_velocity.x, 0.0, senses.last_known_velocity.z) * HunterBrain.PREDICT
		if heading.length() > 0.2:
			_line(senses.last_known, senses.last_known + heading, Color(1.0, 0.2, 1.0, 0.6))
	if senses.interest != Vector3.INF and senses.awareness == CreatureSenses.Awareness.SUSPICIOUS:
		_cross(senses.interest, 0.25, Color(1.0, 0.9, 0.2))
	if senses.awareness == CreatureSenses.Awareness.SEARCHING:
		for spot in brain.search_points:
			_square(spot + Vector3.UP * 0.05, 0.25, Color(1.0, 0.55, 0.1))
	var now := Time.get_ticks_msec() * 0.001
	for noise: Dictionary in NoiseBus.recent:
		var age: float = (now - noise.time) / NoiseBus.KEEP
		if age < 1.0:
			_ring(noise.position, noise.radius * lerpf(0.3, 1.0, age), Color(1, 1, 1, 1.0 - age))
	_lines.surface_end()


func _draw_cone(senses: CreatureSenses, color: Color) -> void:
	var steps := 12
	var previous := Vector3.INF
	for i in steps + 1:
		var angle := lerpf(-0.5, 0.5, float(i) / steps) * senses.view_angle
		var point := senses.eye + senses.forward.rotated(Vector3.UP, angle) * senses.view_range
		if i == 0 or i == steps:
			_line(senses.eye, point, color)
		if previous != Vector3.INF:
			_line(previous, point, Color(color, 0.5))
		previous = point


func _ring(at: Vector3, radius: float, color: Color) -> void:
	var steps := 24
	for i in steps:
		var a := TAU * i / steps
		var b := TAU * (i + 1) / steps
		_line(at + Vector3(cos(a), 0, sin(a)) * radius, at + Vector3(cos(b), 0, sin(b)) * radius, color)


func _cross(at: Vector3, size: float, color: Color) -> void:
	_line(at - Vector3(size, 0, size), at + Vector3(size, 0, size), color)
	_line(at - Vector3(size, 0, -size), at + Vector3(size, 0, -size), color)


func _square(at: Vector3, half: float, color: Color) -> void:
	var corners := [Vector3(-half, 0, -half), Vector3(half, 0, -half), Vector3(half, 0, half), Vector3(-half, 0, half)]
	for i in 4:
		_line(at + corners[i], at + corners[(i + 1) % 4], color)


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	TacticalGridDebug._line(_lines, a, b, color)
