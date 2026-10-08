## Draws the gait spider's plan, on top of everything:
##   grey +     each leg's rest spot (moves with the body)
##   yellow     a swinging foot's landing target (+) and the line from where it lifted off
##   red +      a target with no floor under it (it landed on the nearest spot with floor)
##   cyan       the knee's aim (up and out from the hip)
class_name GaitDebug
extends MeshInstance3D

const LIFT := 0.03

var spider: GaitSpider
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
	_line(Vector3.ZERO, Vector3.ZERO, Color.TRANSPARENT)
	for leg in spider.legs:
		_cross(spider.rest_world(leg), 0.06, Color(0.6, 0.6, 0.6, 0.8))
		if leg.swinging:
			var color := Color(1.0, 0.85, 0.2) if leg.landed_ok else Color(1.0, 0.3, 0.3)
			_cross(leg.target, 0.1, color)
			_line(leg.lift_off + Vector3.UP * LIFT, leg.target + Vector3.UP * LIFT, Color(color, 0.5))
		var hip: Vector3 = spider.body.global_transform * leg.hip_local
		var out := Vector3(hip.x - spider.global_position.x, 0.0, hip.z - spider.global_position.z).normalized()
		_line(hip, hip + (Vector3.UP * spider.knee_up + out * spider.knee_out) * 0.4, Color(0.3, 0.9, 1.0, 0.35))
	_lines.surface_end()


func _cross(at: Vector3, size: float, color: Color) -> void:
	_line(at + Vector3(-size, LIFT, 0), at + Vector3(size, LIFT, 0), color)
	_line(at + Vector3(0, LIFT, -size), at + Vector3(0, LIFT, size), color)


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(a)
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(b)
