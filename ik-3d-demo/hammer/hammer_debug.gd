## The hammer demo's debug view (C), for the last attack:
##   blue     his feet's path: the jump arc, the hop back, the landing (crosses: takeoff, contact, landing)
##   orange   the hammer head's path over the whole attack (dots every 1/30 s: close = slow, far = fast)
##   red      the contact: the robot's head top, where the hammer lands
##   white    his shoulders now (the swing's centre) and the hammer's line from them
##   green    the arms as the IK solved them; red crosses: where the hands are told to grip
class_name HammerDebug
extends Node3D

const SAMPLES_PER_SECOND := 30.0

var enabled := false

var _mesh := ImmediateMesh.new()
var _label := Label3D.new()


func _ready() -> void:
	var instance := MeshInstance3D.new()
	instance.mesh = _mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.no_depth_test = true
	instance.material_override = material
	add_child(instance)
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.no_depth_test = true
	_label.font_size = 28
	_label.pixel_size = 0.004
	add_child(_label)


func draw(
		demo: HammerDemo,
		pose: SmashAttack.Pose
) -> void:
	_mesh.clear_surfaces()
	_label.visible = enabled
	if not enabled:
		return
	_mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	var attack := demo.attack
	if demo.target >= 0:
		var count := ceili(attack.length() * SAMPLES_PER_SECOND)
		var last_feet := Vector3.INF
		var last_head := Vector3.INF
		for k in count + 1:
			var at := attack.sample(k / SAMPLES_PER_SECOND)
			var head := attack.hammer_point(at, attack.reach)
			if last_feet != Vector3.INF:
				_line(last_feet, at.feet, Color(0.3, 0.6, 1.0))
				_line(last_head, head, Color(1.0, 0.55, 0.15))
			_cross(head, Color(1.0, 0.55, 0.15), 0.03)
			last_feet = at.feet
			last_head = head
		for t in [attack.crouch_time, attack.contact_time(), attack.contact_time() + attack.hitstop + attack.recover_time]:
			_cross(attack.sample(t).feet, Color(0.3, 0.6, 1.0), 0.12)
		_cross(attack.head_top(), Color(1.0, 0.2, 0.2), 0.2)
	var body := demo.body
	_cross(body.shoulders, Color.WHITE, 0.08)
	_line(body.shoulders, body.head_point, Color(1, 1, 1, 0.6))
	for i in 2:
		var points := body.arm_points(i)
		for k in range(1, points.size()):
			_line(points[k - 1], points[k], Color(0.3, 1.0, 0.4))
		_cross(body.hand_targets[i], Color(1.0, 0.3, 0.3), 0.06)
	_mesh.surface_end()
	_label.position = body.shoulders + Vector3.UP * 1.0
	_label.text = "%s  angle %d°  lean %.2f  squash %.2f" % [pose.phase, roundi(rad_to_deg(pose.angle)), pose.lean, pose.squash]


func _line(
		a: Vector3,
		b: Vector3,
		color: Color
) -> void:
	_mesh.surface_set_color(color)
	_mesh.surface_add_vertex(a)
	_mesh.surface_set_color(color)
	_mesh.surface_add_vertex(b)


func _cross(
		at: Vector3,
		color: Color,
		size: float
) -> void:
	for axis in [Vector3.RIGHT, Vector3.UP, Vector3.BACK]:
		_line(at - axis * size, at + axis * size, color)
