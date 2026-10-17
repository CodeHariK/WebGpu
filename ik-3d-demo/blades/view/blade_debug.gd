## Debug view (C). For every Bezier segment of each moving blade's path (orange) and each taut
## chain (cyan):
##   control polygon  P0 → P1 → P2 → P3, dim: the straight lines the curve is pulled toward
##   control points   white crosses (P0 / P3 on the curve, P1 / P2 the handles off it), with ids
##   smooth curve     the Bezier itself
##   tangents         green ticks along the curve: its direction (and, by length, its speed in t)
## plus the blade's own tangent right now (yellow), the joins between segments (magenta), and
## whatever each move adds (draw_debug: the rush arc, the bind routes).
class_name BladeDebug
extends Node3D

var enabled := false

var _mesh := ImmediateMesh.new()
var _labels: Array[Label3D] = []


func _ready() -> void:
	var instance := MeshInstance3D.new()
	instance.mesh = _mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.no_depth_test = true
	instance.material_override = material
	add_child(instance)


func draw(
		demo: BladesDemo,
		taut_chains: Array[PackedVector3Array]
) -> void:
	_mesh.clear_surfaces()
	_label_points(demo.blades)
	if not enabled:
		return
	_mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for blade in demo.blades:
		if blade.phase == Blade.IDLE or blade.path.size() < 4:
			continue
		for k in range(0, blade.path.size() - 3, 4):
			curve_lines(blade.path.slice(k, k + 4), Color(1.0, 0.55, 0.1))
			if k > 0:
				cross(blade.path[k], Color(1.0, 0.2, 0.9), 0.18)
		var tangent := Bezier.path_tangent(blade.path, blade.eased)
		if tangent.length() > 1e-4:
			line(blade.tip, blade.tip + tangent.normalized() * 0.9, Color(1.0, 1.0, 0.2))
	for c in taut_chains:
		curve_lines(c, Color(0.3, 0.9, 1.0))
	for move: BladeMove in demo.moves.values():
		move.draw_debug(self)
	_mesh.surface_end()


## One cubic Bezier: control polygon, smooth curve, tangent ticks, control points.
func curve_lines(
		c: PackedVector3Array,
		color: Color
) -> void:
	var dim := Color(color, 0.45)
	line(c[0], c[1], dim)
	line(c[1], c[2], dim)
	line(c[2], c[3], dim)
	var last := c[0]
	for i in range(1, 33):
		var p := Bezier.point(c[0], c[1], c[2], c[3], i / 32.0)
		line(last, p, color)
		last = p
	for i in 9: # tangent ticks: direction, length ∝ speed along t
		var t := i / 8.0
		var p := Bezier.point(c[0], c[1], c[2], c[3], t)
		var d := Bezier.tangent(c[0], c[1], c[2], c[3], t)
		line(p, p + d * 0.06, Color(0.3, 1.0, 0.4))
	for p in c:
		cross(p, Color.WHITE, 0.08)


func line(
		a: Vector3,
		b: Vector3,
		color: Color
) -> void:
	_mesh.surface_set_color(color)
	_mesh.surface_add_vertex(a)
	_mesh.surface_add_vertex(b)


func cross(
		p: Vector3,
		color: Color,
		size: float
) -> void:
	for axis in [Vector3.RIGHT, Vector3.UP, Vector3.BACK]:
		line(p - axis * size, p + axis * size, color)


# Control point ids on each moving blade's path: L0… left, R0… right; the join reads "L3/4".
func _label_points(blades: Array[Blade]) -> void:
	var used := 0
	if enabled:
		for blade in blades:
			if blade.phase == Blade.IDLE or blade.path.size() < 4:
				continue
			var prefix := "L" if blade.side < 0.0 else "R"
			for i in blade.path.size():
				if i > 0 and i % 4 == 0 and blade.path[i].is_equal_approx(blade.path[i - 1]):
					_labels[used - 1].text += "/%d" % i # same point as the last one: the join
					continue
				if used == _labels.size():
					_labels.append(_new_label())
				var label := _labels[used]
				label.text = "%s%d" % [prefix, i]
				label.modulate = Color(1.0, 0.75, 0.5) if blade.side < 0.0 else Color(1.0, 0.95, 0.5)
				label.position = blade.path[i] + Vector3(0, 0.18, 0)
				label.visible = true
				used += 1
	for k in range(used, _labels.size()):
		_labels[k].visible = false


func _new_label() -> Label3D:
	var label := Label3D.new()
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	label.pixel_size = 0.005
	label.font_size = 40
	label.outline_size = 8
	add_child(label)
	return label
