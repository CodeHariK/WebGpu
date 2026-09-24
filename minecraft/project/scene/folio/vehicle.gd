extends Node3D
##
## Folio car visual (folio World/VisualVehicle): instance the authored car glb and
## re-shade it with the folio pipeline (palette-texture parts -> mesh_scenery,
## solid-colour parts like the orange body/lights -> mesh_default). Visual only —
## the ArcadeVehicle owns the physics; this rides as its child mesh.
##
@export var glb_path := "res://assets/folio/vehicle/default.glb"
@export var palette_path := "res://assets/folio/vehicle/default_palette.png"

# folio authors the body ~0.907 above the model root (see chassis.001 in the glb)
# and re-hangs the wheels under it. We parent the whole glb at the ArcadeVehicle
# sphere centre, so without this the body floats ~0.9 above the wheels. Drop the
# model by this much (and lift the wheels back by it) so the body sits low over
# the wheels like the folio car.
const CHASSIS_LIFT := 0.907

var _pal_mat: ShaderMaterial
var _solid_shader: Shader
var _solid_cache := {}
var _glow_cache := {}  # color html -> unshaded glow material

func _ready() -> void:
	var scn: PackedScene = load(glb_path)
	if scn == null:
		return
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
	root.position.y = -CHASSIS_LIFT   # body drops onto the wheels (folio stance)
	_place_wheels(root)   # glb ships ONE wheel template; clone it to 4 corners
	_dress(root)
	# folio car forward is +X; our ArcadeVehicle forward is -Z -> rotate +90 deg.
	rotation.y = PI * 0.5

# Clone the single wheelContainer to the 4 folio wheel positions (offset x0.90 z0.75),
# flipping the left-side wheels 180 deg so the wheel guards face outward.
func _place_wheels(root: Node) -> void:
	var wc := _find_wheel(root)
	if wc == null:
		return
	var b: Vector3 = wc.position
	var ax := absf(b.x)
	var az := absf(b.z)
	var y := b.y + CHASSIS_LIFT   # compensate the body drop -> wheels stay grounded
	var corners := [
		Vector3(ax, y, -az), Vector3(-ax, y, -az),
		Vector3(ax, y, az), Vector3(-ax, y, az),
	]
	var parent := wc.get_parent()
	for i in range(4):
		var w: Node3D = wc if i == 0 else wc.duplicate()
		if i > 0:
			parent.add_child(w)
		w.position = corners[i]
		w.rotation.y = PI if corners[i].z > 0.0 else 0.0

func _find_wheel(n: Node) -> Node3D:
	if str(n.name).to_lower().begins_with("wheelcontainer"):
		return n as Node3D
	for c in n.get_children():
		var r := _find_wheel(c)
		if r:
			return r
	return null

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
