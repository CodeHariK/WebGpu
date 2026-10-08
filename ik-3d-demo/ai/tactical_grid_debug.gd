## Draws a TacticalGrid: a small square per cell (coloured by height, so floors read apart), its
## links (WALK thin grey, DROP orange arrows, LADDER magenta, JUMP cyan arcs) and a GridWalker's
## current path (white). On top of everything (no depth test).
class_name TacticalGridDebug
extends MeshInstance3D

const LIFT := 0.04 ## draw just above the surfaces
const MARK := 0.12 ## half size of a cell's square
const LOW := Color(0.25, 0.75, 1.0) ## colour of the lowest cells …
const HIGH := Color(1.0, 0.85, 0.2) ## … and the highest

var grid: TacticalGrid
var walker: GridWalker
var show_walk_links := true

var _lines := ImmediateMesh.new()
var _cells_dirty := true
var _static := ImmediateMesh.new() ## cells + links: rebuilt only when the grid changes
var _static_instance: MeshInstance3D


func _ready() -> void:
	mesh = _lines
	top_level = true
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	material_override = _material()
	_static_instance = MeshInstance3D.new()
	_static_instance.mesh = _static
	_static_instance.material_override = _material()
	_static_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_static_instance)
	_static_instance.top_level = true


## Redraw the cells and links (after a rescan or a toggle).
func refresh() -> void:
	_cells_dirty = true


func _process(_delta: float) -> void:
	_static_instance.visible = visible
	if grid == null:
		return
	if _cells_dirty:
		_draw_grid()
		_cells_dirty = false
	_lines.clear_surfaces()
	if visible and walker != null and not walker.path.is_empty():
		_lines.surface_begin(Mesh.PRIMITIVE_LINES)
		var previous := walker.spider.global_position
		for i in range(walker.waypoint, walker.path.size()):
			var point: Vector3 = walker.path[i].position + Vector3.UP * (LIFT * 3.0)
			_line(_lines, previous, point, Color.WHITE)
			_cross(_lines, point, 0.15, Color.WHITE)
			previous = point
		_lines.surface_end()


func _draw_grid() -> void:
	_static.clear_surfaces()
	if grid.cell_count() == 0:
		return
	var low := INF
	var high := -INF
	for point in grid.positions:
		low = minf(low, point.y)
		high = maxf(high, point.y)
	_static.surface_begin(Mesh.PRIMITIVE_LINES)
	for cell in grid.cell_count():
		var point := grid.positions[cell] + Vector3.UP * LIFT
		var shade := LOW.lerp(HIGH, inverse_lerp(low, high, point.y) if high > low else 0.0)
		_square(point, shade)
		for link: Array in grid.links[cell]:
			var to: Vector3 = grid.positions[link[0]] + Vector3.UP * LIFT
			match link[2]:
				TacticalGrid.Link.WALK:
					if show_walk_links and link[0] > cell: # each pair once
						_line(_static, point, to, Color(0.6, 0.6, 0.6, 0.35))
				TacticalGrid.Link.DROP:
					_arrow(point, to, Color(1.0, 0.55, 0.1))
				TacticalGrid.Link.LADDER:
					_arc(point, to, Color(1.0, 0.3, 0.9))
				TacticalGrid.Link.JUMP:
					_arc(point, to, Color(0.2, 0.95, 1.0))
	_static.surface_end()


func _square(at: Vector3, color: Color) -> void:
	var a := at + Vector3(-MARK, 0, -MARK)
	var b := at + Vector3(MARK, 0, -MARK)
	var c := at + Vector3(MARK, 0, MARK)
	var d := at + Vector3(-MARK, 0, MARK)
	_line(_static, a, b, color)
	_line(_static, b, c, color)
	_line(_static, c, d, color)
	_line(_static, d, a, color)


func _arrow(from: Vector3, to: Vector3, color: Color) -> void:
	_line(_static, from, to, color)
	var back := (from - to).normalized() * 0.2
	var side := back.cross(Vector3.UP).normalized() * 0.1
	_line(_static, to, to + back + side, color)
	_line(_static, to, to + back - side, color)


func _arc(from: Vector3, to: Vector3, color: Color) -> void:
	var peak := maxf(from.y, to.y) + 0.8
	var previous := from
	for i in range(1, 11):
		var t := i / 10.0
		var point := from.lerp(to, t)
		point.y = lerpf(from.y, to.y, t) + (peak - maxf(from.y, to.y)) * 4.0 * t * (1.0 - t)
		_line(_static, previous, point, color)
		previous = point


func _cross(target_mesh: ImmediateMesh, at: Vector3, size: float, color: Color) -> void:
	_line(target_mesh, at - Vector3(size, 0, 0), at + Vector3(size, 0, 0), color)
	_line(target_mesh, at - Vector3(0, 0, size), at + Vector3(0, 0, size), color)


static func _line(target_mesh: ImmediateMesh, a: Vector3, b: Vector3, color: Color) -> void:
	target_mesh.surface_set_color(color)
	target_mesh.surface_add_vertex(a)
	target_mesh.surface_set_color(color)
	target_mesh.surface_add_vertex(b)


static func _material() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.no_depth_test = true
	return material
