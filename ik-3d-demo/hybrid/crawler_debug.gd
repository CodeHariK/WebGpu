## Draws the crawler's insides as lines, on top of everything (no depth test). Only reads; never
## changes anything. Each part can be switched off (the demo's panel does that).
##
##   rays        EVERY ray cast (crawler_rays.gd), coloured by what it was for, bright where it
##               hit (with a dot on the hit point), dim where it missed. Every-frame rays show
##               this frame's only; one-off ones (foothold searches) fade out over the log's
##               `linger` time. Each kind can be switched off (show_kind):
##                 white = walker   yellow = homes   green = foothold search   orange = ball
##                 pink = placing it
##   walker      the surface frame at its origin (green up, blue forward) and the velocity
##               (light blue, × the gait's lead time)
##   homes       yellow cross = each limb's home; yellow circle = how far its hand/foot may lag
##               before it wants to step; grey line hand/foot → home, orange once it wants to step
##   footholds   the last foothold search of each limb (crawler_foothold.gd): every candidate as
##               a cross: white = chosen, green = fine but
##               farther, red = nothing there, orange = out of reach, purple = facing away,
##               yellow = too close to another hand/foot
##   arcs        cyan: the path of every step in progress
##   ik          magenta: elbow/knee pole and the line from the joint to it; red: where the IK
##               couldn't reach (target cross and the gap to the tip)
class_name CrawlerDebug
extends MeshInstance3D

const KIND_COLORS := [Color(1, 1, 1), Color(1.0, 0.9, 0.3), Color(0.35, 1.0, 0.35), Color(1.0, 0.6, 0.2), Color(1.0, 0.5, 0.8)]
const VERDICT_COLORS := [Color.WHITE, Color(0.3, 0.9, 0.3), Color(0.9, 0.2, 0.2), Color(1.0, 0.55, 0.1), Color(0.7, 0.3, 1.0), Color(1.0, 0.9, 0.2)]

var crawler: Crawler
var show_rays := true
var show_kind: Array[bool] = [true, true, true, true, true] ## per CrawlerRays.Kind
var show_walker := true
var show_homes := true
var show_footholds := true
var show_arcs := true
var show_ik := true

var _lines := ImmediateMesh.new()


func _ready() -> void:
	top_level = true
	mesh = _lines
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.no_depth_test = true
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material_override = material


func _process(_delta: float) -> void:
	_lines.clear_surfaces()
	if crawler == null or not visible:
		return
	_lines.surface_begin(Mesh.PRIMITIVE_LINES)
	_line(Vector3.ZERO, Vector3.ZERO, Color.BLACK) # never an empty surface
	crawler.rays.prune()
	if show_rays:
		_draw_rays()
	if show_walker:
		_draw_walker()
	for limb in CrawlerLayout.COUNT:
		if show_homes:
			_draw_home(limb)
		if show_footholds:
			_draw_foothold(limb)
		if show_arcs and crawler.gait.steps[limb].stepping:
			_draw_polyline(crawler.gait.steps[limb].arc(12), Color(0.2, 0.9, 1.0))
		if show_ik:
			_draw_ik(limb)
	_lines.surface_end()


## Every logged ray: bright + a hit dot where it hit, dim where it missed, fading with age.
func _draw_rays() -> void:
	var record := crawler.rays
	var now := Time.get_ticks_msec() / 1000.0
	for ray: Array in record.recent:
		if not show_kind[ray[3]]:
			continue
		var fade := 1.0 - clampf((now - ray[4]) / maxf(record.linger, 0.001), 0.0, 1.0)
		var color: Color = KIND_COLORS[ray[3]]
		if ray[2]:
			color.a = 0.25 + 0.75 * fade
			_line(ray[0], ray[1], color)
			_cross(ray[1], 0.012, color)
		else:
			color = color.darkened(0.55)
			color.a = 0.15 + 0.45 * fade
			_line(ray[0], ray[1], color)


func _draw_walker() -> void:
	var walker := crawler.walker
	var at := walker.frame.origin
	_line(at, at + walker.up() * 0.3, Color(0.3, 1.0, 0.3))
	_line(at, at + walker.forward() * 0.3, Color(0.3, 0.5, 1.0))
	_line(at, at + walker.velocity * crawler.gait.lead_time, Color(0.5, 0.8, 1.0))


func _draw_home(limb: int) -> void:
	var gait := crawler.gait
	var home := gait.homes[limb]
	var normal := gait.home_normals[limb]
	_cross(home, 0.05, Color(1.0, 0.9, 0.2))
	_circle(home, normal, gait.step_distance, Color(1.0, 0.9, 0.2, 0.6))
	_line(gait.steps[limb].position, home, Color(1.0, 0.55, 0.1) if gait.wants[limb] else Color(0.6, 0.6, 0.6))


func _draw_foothold(limb: int) -> void:
	var foothold := crawler.gait.foothold
	for candidate: Array in foothold.candidates[limb]:
		var chosen: bool = candidate[1] == CrawlerFoothold.Verdict.CHOSEN
		_cross(candidate[0], 0.06 if chosen else 0.03, VERDICT_COLORS[candidate[1]])


func _draw_ik(limb: int) -> void:
	var chain := crawler.rig.chains[limb]
	_line(chain.points[1], chain.pole, Color(1.0, 0.3, 1.0))
	_cross(chain.pole, 0.03, Color(1.0, 0.3, 1.0))
	if chain.error() > 0.02:
		_line(chain.tip(), chain.target, Color(1.0, 0.1, 0.1))
		_cross(chain.target, 0.05, Color(1.0, 0.1, 0.1))


func _circle(centre: Vector3, normal: Vector3, radius: float, color: Color) -> void:
	var basis := SurfaceWalker.surface_basis(normal, Vector3.FORWARD)
	var previous := centre + basis.x * radius
	for i in range(1, 17):
		var angle := TAU * i / 16.0
		var point := centre + (basis.x * cos(angle) + basis.z * sin(angle)) * radius
		_line(previous, point, color)
		previous = point


func _draw_polyline(points: PackedVector3Array, color: Color) -> void:
	for i in points.size() - 1:
		_line(points[i], points[i + 1], color)


func _cross(at: Vector3, size: float, color: Color) -> void:
	_line(at - Vector3.RIGHT * size, at + Vector3.RIGHT * size, color)
	_line(at - Vector3.UP * size, at + Vector3.UP * size, color)
	_line(at - Vector3.BACK * size, at + Vector3.BACK * size, color)


func _line(a: Vector3, b: Vector3, color: Color) -> void:
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(a)
	_lines.surface_set_color(color)
	_lines.surface_add_vertex(b)
