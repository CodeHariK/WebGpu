## Debug lines for an enemy body: its route (red on the floor, cyan on walls and the ceiling) and
## its desired velocity (green). Redrawn each frame into one ImmediateMesh, drawn on top of
## everything. Only for testing: leave it out of the real game.
extends MeshInstance3D

const EnemyBody := preload("res://src/enemy/enemy_body.gd")

var body: EnemyBody
var _lines := ImmediateMesh.new()


func _ready() -> void:
	top_level = true
	mesh = _lines
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.vertex_color_use_as_albedo = true
	mat.no_depth_test = true
	material_override = mat


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	if body == null or not visible:
		return
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	var from := body.global_position
	for point in body.route.points:
		_line(from, point.position, Color(0.2, 0.9, 1.0) if point.off_floor else Color(1.0, 0.2, 0.15))
		from = point.position
	_line(body.global_position, body.global_position + body.desired_velocity, Color(0.3, 1.0, 0.3))
	_lines.surface_end()


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(a)
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(b)
