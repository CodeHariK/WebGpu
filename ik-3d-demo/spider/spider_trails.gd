## Motion trails: every `sample_every` frames it records where each leg joint, each knee pole and
## the body centre are, keeps the last `length` samples, and draws each as a fading polyline with a
## small tick at every sample — so the spacing shows speed (bunched ticks = slow, spread = fast)
## and the shapes show the paths: the step curve at the feet, the knees' swing, the poles' yaw.
## Drawn on top of everything (no depth test). Clears itself when the legs are rebuilt.
##
##   white   hips          orange  knees          yellow  ankles (three-bone legs)
##   cyan    feet          magenta knee poles     blue    body centre
class_name SpiderTrails
extends MeshInstance3D

const TICK := 0.012 ## half size of the tick at each sample (m)
const BREAK := 1.5 ## a jump longer than this between samples (a teleport) breaks the line

@export var spider: Spider
@export var sample_every := 3 ## frames between samples
@export var length := 80 ## samples kept per trail (80 × 3 frames ≈ 4 s at 60 fps)

var _trails: Array = [] ## of {"points": PackedVector3Array, "color": Color}
var _frame := 0
var _lines := ImmediateMesh.new()


func _ready() -> void:
	mesh = _lines
	top_level = true # world coordinates
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.no_depth_test = true
	material_override = material
	if spider != null:
		spider.rebuilt.connect(clear)


## Forget every trail (start drawing afresh).
func clear() -> void:
	_trails.clear()


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	if not visible or spider == null or spider.rig == null or not spider.visible:
		return
	_frame += 1
	if _frame % maxi(sample_every, 1) == 0:
		_sample()
	_draw()


# Append one point to every trail: per leg its joints and pole, then the body centre.
func _sample() -> void:
	var rig := spider.rig
	var points: Array[Vector3] = []
	var colors: Array[Color] = []
	for leg in rig.layout.legs.size():
		var joints := rig.joint_count(leg)
		for joint in joints:
			points.append(rig.joint_position(leg, joint))
			colors.append(_joint_color(joint, joints))
		points.append(rig.poles[leg].global_position)
		colors.append(Color(1.0, 0.3, 0.9))
	points.append(rig.skeleton.global_position)
	colors.append(Color(0.3, 0.55, 1.0))
	if _trails.size() != points.size(): # first sample, or the legs changed
		_trails.clear()
		for i in points.size():
			_trails.append({"points": PackedVector3Array(), "color": colors[i]})
	for i in points.size():
		var trail: PackedVector3Array = _trails[i].points
		trail.append(points[i])
		if trail.size() > length:
			trail.remove_at(0)
		_trails[i].points = trail


func _draw() -> void:
	if _trails.is_empty():
		return
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	for trail: Dictionary in _trails:
		var points: PackedVector3Array = trail.points
		var color: Color = trail.color
		for i in points.size():
			var faded := Color(color, float(i + 1) / points.size()) # old samples fade out
			_tick(points[i], faded)
			if i > 0 and points[i - 1].distance_to(points[i]) < BREAK:
				_line(points[i - 1], points[i], faded)
	_lines.surface_end()


static func _joint_color(joint: int, joints: int) -> Color:
	if joint == 0:
		return Color(0.95, 0.95, 0.95) # hip
	if joint == joints - 1:
		return Color(0.2, 0.95, 1.0) # foot
	if joint == 1:
		return Color(1.0, 0.55, 0.1) # knee
	return Color(1.0, 0.9, 0.2) # ankle


func _tick(at: Vector3, color: Color) -> void:
	_line(at - Vector3(TICK, 0, 0), at + Vector3(TICK, 0, 0), color)
	_line(at - Vector3(0, TICK, 0), at + Vector3(0, TICK, 0), color)


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(a)
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(b)
