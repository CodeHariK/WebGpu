## Draws a Squad, on top of everything:
##   heatmap   the selected member's scores for its role on every candidate cell (dark blue =
##             worst … yellow = best); the best cell gets a white ring
##   claims    per member, a square on its claimed cell and a line from the spider, in its role's
##             colour (pressure red, flank left cyan, flank right magenta, behind orange,
##             surround green)
##   labels    role + state over each spider (its brain's state while the squad isn't alert), and
##             its bark above that, in white
##   magenta cross  the squad's last sighting of the player
class_name SquadDebug
extends MeshInstance3D

const ROLE_COLORS: Array[Color] = [Color(1.0, 0.25, 0.2), Color(0.2, 0.9, 1.0), Color(1.0, 0.3, 1.0), Color(1.0, 0.6, 0.1), Color(0.4, 1.0, 0.35)]
const LOW := Color(0.1, 0.15, 0.6)
const HIGH := Color(1.0, 0.9, 0.2)
const LIFT := 0.06

var squad: Squad
var selected := 0 ## whose heatmap is drawn
var show_heatmap := true

var _lines := ImmediateMesh.new()
var _labels: Array[Label3D] = []
var _bark_labels: Array[Label3D] = []


func _ready() -> void:
	mesh = _lines
	top_level = true
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	material_override = TacticalGridDebug._material()


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	if squad == null or squad.members.is_empty() or not visible:
		for label in _labels + _bark_labels:
			label.visible = false
		return
	for label in _labels + _bark_labels: # members draw theirs again below (a removed one stays hidden)
		label.visible = false
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	_line(Vector3.ZERO, Vector3.ZERO, Color.TRANSPARENT) # a surface needs at least one line
	var alert := squad.knowledge.knows()
	if show_heatmap and alert:
		_draw_heatmap(squad.members[selected % squad.members.size()])
	if squad.knowledge.last_known != Vector3.INF:
		var at := squad.knowledge.last_known
		_line(at - Vector3(0.4, 0, 0.4), at + Vector3(0.4, 0, 0.4), Color(1.0, 0.2, 1.0))
		_line(at - Vector3(0.4, 0, -0.4), at + Vector3(0.4, 0, -0.4), Color(1.0, 0.2, 1.0))
	for i in squad.members.size():
		_draw_member(i, squad.members[i])
	_lines.surface_end()


func _draw_heatmap(member: Squad.Member) -> void:
	if member.scores.is_empty():
		return
	var low := INF
	var high := -INF
	for cell: int in member.scores:
		low = minf(low, member.scores[cell])
		high = maxf(high, member.scores[cell])
	var grid := squad.grid
	for cell: int in member.scores:
		var t: float = 0.0 if high - low < 0.001 else (member.scores[cell] - low) / (high - low)
		var color := LOW.lerp(HIGH, t * t) # squared: the good cells stand out
		_fill(grid.positions[cell] + Vector3.UP * LIFT, grid.cell_size * 0.32, color)
	var best := PositionScorer.best(member.scores)
	if best >= 0:
		_square(grid.positions[best] + Vector3.UP * LIFT, grid.cell_size * 0.5, Color.WHITE)


func _draw_member(index: int, member: Squad.Member) -> void:
	var color := ROLE_COLORS[member.role]
	if member.cell >= 0 and squad.knowledge.knows():
		var spot := squad.grid.positions[member.cell] + Vector3.UP * LIFT
		_square(spot, squad.grid.cell_size * 0.45, color)
		_square(spot, squad.grid.cell_size * 0.38, color)
		_line(member.spider.global_position + Vector3.UP * 0.3, spot, Color(color, 0.7))
	while _labels.size() <= index:
		_labels.append(_new_label(40))
		_bark_labels.append(_new_label(64))
	var label := _labels[index]
	label.visible = true
	label.modulate = color
	label.global_position = member.spider.global_position + Vector3.UP * 1.2
	var role_name: String = PositionScorer.Role.keys()[member.role].to_lower().replace("_", " ")
	var doing: String = member.state if squad.knowledge.knows() else member.brain.state
	label.text = "%s%s\n%s" % ["▸ " if index == selected % squad.members.size() else "", role_name, doing]
	var bark_label := _bark_labels[index]
	bark_label.visible = false
	for bark: Dictionary in squad.barks:
		if bark.member == member:
			bark_label.visible = true
			bark_label.text = bark.text
			bark_label.global_position = member.spider.global_position + Vector3.UP * 1.9


func _new_label(size: int) -> Label3D:
	var label := Label3D.new()
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	label.font_size = size
	label.pixel_size = 0.006
	label.outline_size = 12
	add_child(label)
	label.top_level = true
	return label


# A filled square as a few lines (the lines material has no triangles mode here).
func _fill(at: Vector3, half: float, color: Color) -> void:
	for k in 4:
		var z := lerpf(-half, half, k / 3.0)
		_line(at + Vector3(-half, 0, z), at + Vector3(half, 0, z), color)


func _square(at: Vector3, half: float, color: Color) -> void:
	var corners := [Vector3(-half, 0, -half), Vector3(half, 0, -half), Vector3(half, 0, half), Vector3(-half, 0, half)]
	for i in 4:
		_line(at + corners[i], at + corners[(i + 1) % 4], color)


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	TacticalGridDebug._line(_lines, a, b, color)
