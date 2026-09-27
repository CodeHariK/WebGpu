extends Node3D
##
## Folio car visual (folio World/VisualVehicle).
##
## Instances the authored car glb, re-shades it with the folio pipeline
## (palette-texture parts -> mesh_scenery, solid-colour parts like the orange body
## / lights -> mesh_default, emissive parts -> mesh_glow) and, crucially, drives
## the wheels from the ArcadeVehicle physics instead of rigidly parenting them:
##
##   * the body sits at its authored height (no fake "chassis lift"),
##   * each wheel is placed at its physics WheelConfig hardpoint and moved
##     vertically every frame by that wheel's live suspension travel, so on a turn
##     the loaded (outer) wheels visibly compress more than the inner ones,
##   * front wheels steer and all wheels roll with speed,
##   * rear wheels kick up smoke while the car is drifting.
##
## Visual only — the ArcadeVehicle (our RigidBody3D parent) owns the physics.
##
@export var glb_path := "res://assets/folio/vehicle/default.glb"
@export var palette_path := "res://assets/folio/vehicle/default_palette.png"

## Authored tire radius of the wheel template in the glb (see the glb inspection:
## wheel AABB ~0.86 across => radius ~0.43). Used only to rescale the visual wheel
## if the physics WheelConfig radius differs.
const TEMPLATE_WHEEL_RADIUS := 0.43
## Hard clamp on the *visual* steer angle so the front wheels never look broken,
## matching the physics steering clamp (STEER_MAX_LOCK_RAD).
const VISUAL_STEER_MAX := 0.9

var _veh: Node3D                 # the ArcadeVehicle parent (RigidBody3D)
var _pal_mat: ShaderMaterial
var _solid_shader: Shader
var _solid_cache := {}
var _glow_cache := {}            # color html -> unshaded glow material

var _wheel_template: Node3D      # the folio wheel assembly, cloned to 4 corners
var _wheels: Array[Node3D] = []  # visual wheels, indexed to match physics wheels
var _hardpoints: Array = []      # Vector3 hardpoint (car/physics space) per wheel
var _rest: Array = []            # suspension rest length per wheel
var _wheel_scale := 1.0
var _visual_steer_max := VISUAL_STEER_MAX
var _spin := 0.0                 # accumulated wheel roll angle (radians)

var _drift_fx: Array[GPUParticles3D] = []  # rear-wheel drift smoke
var _smoke_timer := 0.0                     # keeps smoke puffing briefly after a slip
const SMOKE_LINGER := 0.35                  # seconds smoke keeps emitting after a slide
const SMOKE_SLIP_ON := 1.5                  # sideways m/s that counts as a slide

func _ready() -> void:
	_veh = get_parent() as Node3D
	var scn: PackedScene = load(glb_path)
	if scn == null:
		return

	# folio material pipeline.
	_pal_mat = ShaderMaterial.new()
	_pal_mat.shader = load("res://material/shaders/folio/mesh_scenery.gdshader")
	var pal: Texture2D = load(palette_path)
	if pal:
		_pal_mat.set_shader_parameter("palette", pal)
	_pal_mat.set_shader_parameter("has_water", false)
	_pal_mat.set_shader_parameter("has_reveal", false)
	_solid_shader = load("res://material/shaders/folio/mesh_default.gdshader")

	var root: Node3D = scn.instantiate()
	add_child(root)

	# folio car forward is +X; our ArcadeVehicle forward is -Z. Rotating THIS node
	# by +90 deg maps the whole model (body + the wheels we add below) into car
	# space, so a wheel authored/placed in folio local coords lands on the matching
	# physics wheel with no per-axis bookkeeping.
	rotation.y = PI * 0.5

	# Pull the wheel assembly out as a template, then strip every wheel node from
	# the body so only the chassis/body/lights remain. The wheels are rebuilt below
	# and driven by the suspension.
	var wc := _find_wheel(root)
	if wc:
		_wheel_template = wc.duplicate() as Node3D
		_wheel_template.transform = Transform3D.IDENTITY
		# The folio wheel assembly bundles a tall suspension strut (wheelSuspension)
		# under the wheel container; drop it so the wheels don't trail visible rods.
		_strip_named(_wheel_template, "suspension")
		_dress(_wheel_template)
	_strip_wheels(root)
	_dress(root)

	_build_wheels()
	_build_drift_fx()

# ---------------------------------------------------------------------------
# Wheels: build from the physics config, then follow the suspension each frame.
# ---------------------------------------------------------------------------

func _build_wheels() -> void:
	if _wheel_template == null or _veh == null or not _veh.has_method("get_config"):
		return
	var cfg = _veh.get_config()
	if cfg == null:
		return
	var wcs: Array = cfg.get_wheel_configs()

	# Match the visual wheel size to the physics tire radius (they should already
	# agree, but keep it robust to config edits).
	var radius := TEMPLATE_WHEEL_RADIUS
	if wcs.size() > 0 and wcs[0] != null:
		radius = float(wcs[0].get_radius())
	_wheel_scale = radius / TEMPLATE_WHEEL_RADIUS
	_visual_steer_max = minf(deg_to_rad(float(cfg.get_max_steer_angle_deg())), VISUAL_STEER_MAX)

	for i in range(wcs.size()):
		var conf = wcs[i]
		if conf == null:
			continue
		var hp: Vector3 = conf.get_hardpoint_offset()
		_hardpoints.append(hp)
		_rest.append(float(conf.get_suspension_rest_length()))
		var w := _wheel_template.duplicate() as Node3D
		add_child(w)
		_wheels.append(w)
		# Start fully extended; _physics_process refines it immediately.
		_apply_wheel_transform(i, _rest[i], 0.0, false)

# Convert a physics hardpoint (car space) into this (folio-rotated) node's local
# space and lay out the wheel: hang it `disp` below the hardpoint, steer the fronts,
# roll all of them, and flip one side so the wheel guard faces outward.
func _apply_wheel_transform(i: int, disp: float, steer: float, is_front: bool) -> void:
	var hp: Vector3 = _hardpoints[i]
	# car (hp.x,hp.y,hp.z) -> this node's local (folio) coords under the +90 deg yaw.
	var pos := Vector3(-hp.z, hp.y - disp, hp.x)
	var yaw := (PI if hp.x > 0.0 else 0.0)   # guard faces outward on the +X side
	if is_front:
		yaw += -steer                        # positive steer (right) = -Y rotation
	var b := Basis(Vector3.UP, yaw) * Basis(Vector3(0.0, 0.0, 1.0), _spin)
	b = b.scaled(Vector3.ONE * _wheel_scale)
	_wheels[i].transform = Transform3D(b, pos)

func _physics_process(delta: float) -> void:
	if _veh == null or _wheels.is_empty():
		return

	# Forward speed drives the wheel roll; steering input drives the front wheels.
	var fwd := -_veh.global_transform.basis.z.normalized()
	var speed := 0.0
	if _veh is RigidBody3D:
		speed = (_veh as RigidBody3D).linear_velocity.dot(fwd)
	var radius := maxf(TEMPLATE_WHEEL_RADIUS * _wheel_scale, 0.05)
	_spin -= (speed / radius) * delta

	var steer_in := Input.get_action_strength("move_right") - Input.get_action_strength("move_left")
	var steer := clampf(steer_in, -1.0, 1.0) * _visual_steer_max

	var have_disp: bool = _veh.has_method("get_wheel_displacement")
	for i in range(_wheels.size()):
		var disp: float = _rest[i]
		if have_disp:
			disp = float(_veh.get_wheel_displacement(i))
		var is_front: bool = _hardpoints[i].z < 0.0   # forward is -Z -> front wheels
		_apply_wheel_transform(i, disp, steer, is_front)

	_update_drift_fx(delta)

# ---------------------------------------------------------------------------
# Drift smoke: soft puffs off the rear wheels while the physics car is drifting.
# ---------------------------------------------------------------------------

func _build_drift_fx() -> void:
	# One emitter per rear wheel (rear = hardpoint.z > 0, since forward is -Z).
	for i in range(_hardpoints.size()):
		var hp: Vector3 = _hardpoints[i]
		if hp.z <= 0.0:
			continue
		var p := _make_smoke()
		add_child(p)
		# Sit the emitter near the contact patch of that wheel (folio local coords).
		p.position = Vector3(-hp.z, hp.y - _rest[i], hp.x)
		_drift_fx.append(p)

func _update_drift_fx(delta: float) -> void:
	if _drift_fx.is_empty():
		return
	# Trigger smoke on the handbrake drift state OR whenever the tyres slip sideways.
	# Slides are brief (grip corrects fast), so we hold the smoke on for SMOKE_LINGER
	# after each slip — otherwise you only get a single-frame puff that's easy to miss.
	var trigger: bool = _veh.has_method("get_is_drifting") and bool(_veh.get_is_drifting())
	if not trigger and _veh is RigidBody3D:
		var vel: Vector3 = (_veh as RigidBody3D).linear_velocity
		var basis := _veh.global_transform.basis
		var fwd_speed: float = vel.dot(-basis.z.normalized())
		var lateral: float = absf(vel.dot(basis.x.normalized()))
		trigger = absf(fwd_speed) > 2.0 and lateral > SMOKE_SLIP_ON
	if trigger:
		_smoke_timer = SMOKE_LINGER
	else:
		_smoke_timer = maxf(0.0, _smoke_timer - delta)
	var emit := _smoke_timer > 0.0
	for p in _drift_fx:
		if p.emitting != emit:
			p.emitting = emit

func _make_smoke() -> GPUParticles3D:
	var p := GPUParticles3D.new()
	p.amount = 48
	p.lifetime = 1.0
	p.local_coords = false      # puffs stay where they were born as the car slides on
	p.emitting = false
	# Global-space puffs on a small node get culled easily; give a generous bound.
	p.visibility_aabb = AABB(Vector3(-12, -6, -12), Vector3(24, 16, 24))

	var pm := ParticleProcessMaterial.new()
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	pm.emission_sphere_radius = 0.2
	pm.direction = Vector3(0.0, 1.0, 0.0)
	pm.spread = 40.0
	pm.initial_velocity_min = 1.0
	pm.initial_velocity_max = 3.0
	pm.gravity = Vector3(0.0, 1.6, 0.0)   # smoke rises
	pm.scale_min = 0.8
	pm.scale_max = 1.8
	# Fade the puffs out over their life (grey tyre smoke -> transparent).
	var grad := Gradient.new()
	grad.set_color(0, Color(0.55, 0.55, 0.58, 0.9))
	grad.set_color(1, Color(0.7, 0.7, 0.72, 0.0))
	var gt := GradientTexture1D.new()
	gt.gradient = grad
	pm.color_ramp = gt
	p.process_material = pm

	var mesh := SphereMesh.new()
	mesh.radius = 0.35
	mesh.height = 0.7
	mesh.radial_segments = 8
	mesh.rings = 4
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = Color(0.95, 0.95, 1.0, 0.6)
	mat.vertex_color_use_as_albedo = true
	mesh.material = mat
	p.draw_pass_1 = mesh
	return p

# ---------------------------------------------------------------------------
# glb helpers (wheel discovery + folio re-shading).
# ---------------------------------------------------------------------------

func _find_wheel(n: Node) -> Node3D:
	if str(n.name).to_lower().begins_with("wheelcontainer"):
		return n as Node3D
	for c in n.get_children():
		var r := _find_wheel(c)
		if r:
			return r
	return null

# Free (immediately — the template is not in the tree yet) every descendant whose
# name contains `needle`. Used to drop the suspension strut from the wheel template.
func _strip_named(n: Node, needle: String) -> void:
	for c in n.get_children():
		if str(c.name).to_lower().find(needle) >= 0:
			n.remove_child(c)
			c.free()
		else:
			_strip_named(c, needle)

# Remove every wheel node from the body so only the chassis/body/lights remain;
# the wheels are rebuilt from the physics config and driven by the suspension.
func _strip_wheels(n: Node) -> void:
	for c in n.get_children():
		if str(c.name).to_lower().begins_with("wheel"):
			c.queue_free()
		else:
			_strip_wheels(c)

func _dress(n: Node) -> void:
	for c in n.get_children():
		if c is MeshInstance3D:
			var mi := c as MeshInstance3D
			var glow = _glow_color_for(str(mi.name).to_lower())
			if glow != null:
				mi.material_override = _glow_mat(glow)
			elif mi.mesh:
				for i in range(mi.mesh.get_surface_count()):
					var fmat := _mat_for(mi.get_active_material(i))
					if fmat:
						mi.set_surface_override_material(i, fmat)
		_dress(c)

# folio emissive parts: energy cells glow purple, headlights warm, back lights red.
func _glow_color_for(name: String):
	if name.find("cellsenergy") >= 0:
		return Color(0.42, 0.30, 1.0)   # #6053ff-ish purple
	if name.find("headlight") >= 0:
		return Color(1.0, 0.86, 0.55)   # warm headlight
	if name.find("backlight") >= 0:
		return Color(1.0, 0.16, 0.12)   # red tail
	return null

func _glow_mat(col: Color) -> Material:
	var key := col.to_html(false)
	if not _glow_cache.has(key):
		var m := ShaderMaterial.new()
		m.shader = load("res://material/shaders/folio/mesh_glow.gdshader")
		m.set_shader_parameter("glow_color", col)
		_glow_cache[key] = m
	return _glow_cache[key]

func _mat_for(src: Material) -> Material:
	var std := src as StandardMaterial3D
	if std and std.albedo_texture != null:
		return _pal_mat
	var col: Color = std.albedo_color if std else Color.WHITE
	var key := col.to_html(false)
	if not _solid_cache.has(key):
		var m := ShaderMaterial.new()
		m.shader = _solid_shader
		m.set_shader_parameter("base_color", col)
		m.set_shader_parameter("has_water", false)
		m.set_shader_parameter("has_reveal", false)
		_solid_cache[key] = m
	return _solid_cache[key]
