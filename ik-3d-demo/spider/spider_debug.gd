## Draws the spider's gait internals as lines, on top of everything (no depth test).
## Reads what Spider computed this frame; never changes it.
##
##   white   raycast under the spider's root (ground height)
##   green   raycast that found a foot's home spot (red = missed, nothing below)
##   yellow  home spot (cross) + step-trigger circle (radius = that leg's stride)
##   gray    planted foot → home; orange once the foot is outside the circle (wants to step)
##   cyan    the arc a stepping foot is following
##   magenta knee pole and the line from the knee to it
##   blue    walking velocity, scaled by lead_time (how far homes are pushed ahead)
class_name SpiderDebug
extends MeshInstance3D

const CIRCLE_SEGMENTS := 24
const CROSS_SIZE := 0.08

@export var spider: Spider

var _lines := ImmediateMesh.new()


func _ready() -> void:
	mesh = _lines
	top_level = true # draw in world coordinates
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.no_depth_test = true
	material_override = material


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	if not visible or spider == null or not spider.visible or spider.homes.size() != spider.legs.size() or spider.homes.is_empty():
		return
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	_draw_ray(spider.ground_ray, Color.WHITE)
	for leg in spider.legs.size():
		_draw_leg(leg)
	var origin := spider.global_position + Vector3.UP * 0.05
	_line(origin, origin + spider.velocity * spider.lead_time, Color(0.3, 0.5, 1.0))
	_lines.surface_end()


func _draw_leg(leg: int) -> void:
	if not spider.rig.layout.legs[leg].walks: # an arm (claw): no home, no steps
		_draw_pole(leg)
		return
	var gait_leg := spider.legs[leg]
	var home := spider.homes[leg]
	_draw_ray(spider.home_rays[leg], Color(0.2, 0.9, 0.3))
	var stride := spider.leg_step_distance(leg)
	var outside := gait_leg.planted.distance_to(home) > stride
	var yellow := Color(1.0, 0.85, 0.1)
	_cross(home, yellow)
	_circle(home, stride, Color(1.0, 0.55, 0.1) if outside else yellow)
	_line(gait_leg.planted, home, Color(1.0, 0.55, 0.1) if outside else Color(0.6, 0.6, 0.6))
	if gait_leg.stepping:
		_draw_arc(gait_leg, spider.step_height * spider.rig.layout.legs[leg].lift_scale)
	_draw_pole(leg)


# Green from the ray start down to the hit (cross at the hit); red all the way if it missed.
func _draw_ray(ray: Dictionary, hit_color: Color) -> void:
	if ray.is_empty():
		return
	if ray.hit != null:
		_line(ray.from, ray.hit, hit_color)
		_cross(ray.hit, hit_color)
	else:
		_line(ray.from, ray.to, Color.RED)


func _draw_arc(gait_leg: SpiderLeg, lift: float) -> void:
	var cyan := Color(0.2, 0.9, 1.0)
	var previous := gait_leg.arc_point(0.0, lift)
	for i in range(1, 13):
		var point := gait_leg.arc_point(i / 12.0, lift)
		_line(previous, point, cyan)
		previous = point


func _draw_pole(leg: int) -> void:
	if not spider.use_knee_poles:
		return
	var skeleton := spider.rig.skeleton
	var knee := skeleton.global_transform * skeleton.get_bone_global_pose(spider.rig.knee_bones[leg]).origin
	var pole := spider.rig.poles[leg].global_position
	var magenta := Color(1.0, 0.3, 0.9)
	_line(knee, pole, magenta)
	_cross(pole, magenta)


# --- primitives ------------------------------------------------------------------------------

func _line(a: Vector3, b: Vector3, color: Color) -> void:
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(a)
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(b)


func _cross(at: Vector3, color: Color) -> void:
	_line(at - Vector3.RIGHT * CROSS_SIZE, at + Vector3.RIGHT * CROSS_SIZE, color)
	_line(at - Vector3.UP * CROSS_SIZE, at + Vector3.UP * CROSS_SIZE, color)
	_line(at - Vector3.BACK * CROSS_SIZE, at + Vector3.BACK * CROSS_SIZE, color)


# Flat circle in the XZ plane, lifted slightly so it isn't hidden in the ground.
func _circle(center: Vector3, radius: float, color: Color) -> void:
	var lifted := center + Vector3.UP * 0.02
	for i in CIRCLE_SEGMENTS:
		var a := TAU * i / CIRCLE_SEGMENTS
		var b := TAU * (i + 1) / CIRCLE_SEGMENTS
		_line(lifted + Vector3(cos(a), 0, sin(a)) * radius, lifted + Vector3(cos(b), 0, sin(b)) * radius, color)
