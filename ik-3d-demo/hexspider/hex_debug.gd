## Draws what the hex spider is thinking, on top of everything. Two hexes per leg:
##   cyan hex      the FOOT hex, on the ground round the leg's rest spot (grey +), moving with the
##                 body. The foot steps when it ends up outside it.
##   magenta hex   the POLE hex, in the air above and outside the hip; the + is the corner the knee
##                 points at (magenta line from the knee). It rises while the foot is in the air.
## While a leg swings (and SHOW_AFTER seconds after it lands):
##   blue hex      the foot hex placed `lead` seconds ahead: the corners it tried
##   green +       a usable corner        red x  rejected (no floor / too steep / too high or low /
##   grey line     the ray down                         crowded); red ray = no floor
##   yellow        the chosen corner, and the foot's path
class_name HexDebug
extends MeshInstance3D

const SHOW_AFTER := 0.3
const LIFT := 0.03
const FOOT := Color(0.2, 0.9, 1.0)
const POLE := Color(1.0, 0.35, 0.9)
const PICK := Color(0.35, 0.55, 1.0)
const CHOSEN := Color(1.0, 0.85, 0.2)

var spider: HexSpider
var show_rays := true
var show_poles := true

var _lines := ImmediateMesh.new()


func _ready() -> void:
	mesh = _lines
	top_level = true
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.no_depth_test = true
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material_override = material


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	if spider == null or spider.legs.is_empty() or not visible:
		return
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	_line(Vector3.ZERO, Vector3.ZERO, Color.TRANSPARENT) # a surface needs one line
	for leg in spider.legs:
		var rest := spider._rest_world(leg)
		_hex(rest, spider.ring_radius, FOOT, -spider.rotation.y)
		_cross(rest, 0.06, Color(0.6, 0.6, 0.6))
		if show_poles:
			_draw_pole(leg)
		if leg.stepping or leg.planted_for < SHOW_AFTER:
			_draw_pick(leg)
		if leg.stepping:
			_line(leg.step_from + Vector3.UP * LIFT, leg.step_to + Vector3.UP * LIFT, Color(CHOSEN, 0.6))
	_lines.surface_end()


func _draw_pole(leg: HexLeg) -> void:
	var centre := spider.body.global_transform * spider.pole_centre_local(leg) + Vector3.UP * leg.pole_lift_now(spider.pole_lift)
	_hex(centre, spider.pole_radius, POLE, -spider.rotation.y, 0.0)
	_cross(leg.pole, 0.04, POLE, 0.0)
	_line(leg.knee, leg.pole, Color(POLE, 0.5))


func _draw_pick(leg: HexLeg) -> void:
	if leg.candidates.is_empty():
		return
	_hex(leg.centre, spider.ring_radius, PICK if leg.chosen >= 0 else Color(1, 0.3, 0.3), -spider.rotation.y)
	for i in leg.candidates.size():
		var c: Dictionary = leg.candidates[i]
		var hit: Vector3 = c.hit
		var at: Vector3 = hit if hit != Vector3.INF else Vector3(c.point.x, leg.centre.y, c.point.z)
		if show_rays:
			var end: Vector3 = hit if hit != Vector3.INF else c.point + Vector3.DOWN * HexPicker.PROBE_DOWN
			_line(c.top, end, Color(0.6, 0.6, 0.6, 0.3) if hit != Vector3.INF else Color(1.0, 0.3, 0.3, 0.6))
		if i == leg.chosen:
			_hex(at, 0.07, CHOSEN, 0.0)
		elif c.ok:
			_cross(at, 0.05, Color(0.3, 1.0, 0.4))
		else:
			_x(at, 0.06, Color(1.0, 0.3, 0.3))


func _hex(centre: Vector3, radius: float, color: Color, yaw: float, lift := LIFT) -> void:
	for i in 6:
		var a := HexPicker._flat(yaw + i * PI / 3.0) * radius
		var b := HexPicker._flat(yaw + (i + 1) * PI / 3.0) * radius
		_line(centre + a + Vector3.UP * lift, centre + b + Vector3.UP * lift, color)


func _cross(at: Vector3, size: float, color: Color, lift := LIFT) -> void:
	_line(at + Vector3(-size, lift, 0), at + Vector3(size, lift, 0), color)
	_line(at + Vector3(0, lift, -size), at + Vector3(0, lift, size), color)


func _x(at: Vector3, size: float, color: Color) -> void:
	_line(at + Vector3(-size, LIFT, -size), at + Vector3(size, LIFT, size), color)
	_line(at + Vector3(-size, LIFT, size), at + Vector3(size, LIFT, -size), color)


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(a)
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(b)
