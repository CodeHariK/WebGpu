extends Node3D
##
## Folio `areas.glb` loader WITH physics parsing (folio World/Areas).
##
## folio encodes each object's physics behaviour in its node NAME, so we don't
## just drop the glb in as static decor -- we parse it and build the matching
## Godot body per object:
##   *PhysicalFixed              -> StaticBody3D    (immovable: podiums, walls, maps)
##   *PhysicalKinematic*         -> AnimatableBody3D (script/anim-driven: bumpers, screens)
##   *PhysicalDynamic            -> RigidBody3D     (movable: crates, benches, letters, cups)
## `refRailsPhysicalFixed` (the racing-circuit rails + its `trimesh` collision
## child) is removed entirely on request. Everything else keeps a collider.
##
## Collision shapes come from the object's own mesh(es): a concave trimesh for
## static bodies (accurate, cheap when not moving) and a convex hull for
## kinematic / dynamic bodies (required for moving bodies). Materials use the
## folio pipeline, same routing as scenery.gd (emissive->glow, textured->palette
## sample, solid->base colour).
##
## NOTE: crates being *explosive* ("bomb") is a separate folio system
## (ExplosiveCrates) -- here they are made movable; the explosion FX is TODO.
##

@export var glb_path := "res://assets/folio/areas/areas.glb"
@export var palette_path := "res://assets/folio/areas/areas_palette.png"
@export var dynamic_mass := 2.0

var _scenery_shader: Shader
var _glow_shader: Shader
var _solid_shader: Shader
var _default_palette: Texture2D
var _solid_cache := {}
var _tex_cache := {}
var _glow_cache := {}

func _ready() -> void:
	var scn: PackedScene = load(glb_path)
	if scn == null:
		push_warning("FolioAreas: could not load " + glb_path)
		return
	_scenery_shader = load("res://material/shaders/folio/mesh_scenery.gdshader")
	_glow_shader = load("res://material/shaders/folio/mesh_glow.gdshader")
	_solid_shader = load("res://material/shaders/folio/mesh_default.gdshader")
	_default_palette = load(palette_path)
	var root: Node3D = scn.instantiate()
	add_child(root)
	_walk(root)

# Walk the tree: remove the rails, wrap physics objects into typed bodies, and
# dress plain decorative meshes. Iterate a copy of the child list because we
# reparent nodes into new bodies as we go.
func _walk(n: Node) -> void:
	for c in n.get_children():
		var lname := str(c.name).to_lower()
		# Rails removed entirely (mesh + its `trimesh` collision child).
		if lname.find("refrails") >= 0:
			c.queue_free()
			continue
		var kind := _phys_kind(lname)
		if kind != "":
			_make_body(c as Node3D, kind)   # owns its subtree; don't recurse into it
		else:
			if c is MeshInstance3D:
				_dress_mesh(c as MeshInstance3D)
			_walk(c)

func _phys_kind(lname: String) -> String:
	if lname.find("physicalkinematic") >= 0:
		return "kinematic"
	if lname.find("physicaldynamic") >= 0:
		return "dynamic"
	if lname.find("physicalfixed") >= 0:
		return "fixed"
	return ""

# Wrap one folio physics object (a mesh or a group of meshes) in the right body.
func _make_body(obj: Node3D, kind: String) -> void:
	if obj == null:
		return
	var xform := obj.global_transform

	var body: PhysicsBody3D
	match kind:
		"fixed":
			body = StaticBody3D.new()
		"kinematic":
			body = AnimatableBody3D.new()
		_:
			var rb := RigidBody3D.new()
			rb.mass = dynamic_mass
			rb.can_sleep = true
			body = rb
	body.name = str(obj.name) + "Body"
	add_child(body)
	body.global_transform = xform

	# Move the visual under the body (identity local so it rides the body).
	var parent := obj.get_parent()
	if parent:
		parent.remove_child(obj)
	body.add_child(obj)
	obj.transform = Transform3D.IDENTITY

	# Materials for every mesh in the object.
	_dress_recursive(obj)

	# One collision shape per mesh, placed at the mesh's transform under the body.
	var use_box := kind != "fixed"
	_add_shapes(body, obj, use_box)

func _add_shapes(body: PhysicsBody3D, node: Node, use_box: bool) -> void:
	if node is MeshInstance3D:
		var mi := node as MeshInstance3D
		if mi.mesh:
			var cs := CollisionShape3D.new()
			if use_box:
				# Moving bodies (dynamic / kinematic): an AABB box is robust for
				# folio's thin planar meshes (blackboards, letters, signs) where a
				# convex hull fails, and is cheap + stable to simulate.
				var aabb := mi.mesh.get_aabb()
				var box := BoxShape3D.new()
				box.size = aabb.size.max(Vector3(0.05, 0.05, 0.05))
				cs.shape = box
				body.add_child(cs)
				cs.global_transform = mi.global_transform * Transform3D(Basis(), aabb.get_center())
			else:
				# Static bodies: an accurate concave trimesh (fine when not moving).
				cs.shape = mi.mesh.create_trimesh_shape()
				body.add_child(cs)
				cs.global_transform = mi.global_transform
	for c in node.get_children():
		_add_shapes(body, c, use_box)

func _dress_recursive(n: Node) -> void:
	if n is MeshInstance3D:
		_dress_mesh(n as MeshInstance3D)
	for c in n.get_children():
		_dress_recursive(c)

# ---- Materials (folio pipeline; same routing as scenery.gd) -----------------

func _dress_mesh(mi: MeshInstance3D) -> void:
	var mesh := mi.mesh
	if mesh == null:
		return
	for i in range(mesh.get_surface_count()):
		var fmat := _folio_material_for(mi.get_active_material(i))
		if fmat:
			mi.set_surface_override_material(i, fmat)

func _folio_material_for(src: Material) -> Material:
	var std := src as StandardMaterial3D
	var mname := ""
	if src:
		mname = str(src.resource_name).to_lower()
	if mname.begins_with("emissive"):
		var gcol := Color(1.0, 0.6, 0.2)
		if std:
			gcol = std.emission if std.emission_enabled else std.albedo_color
		return _glow_mat(gcol)
	if std and std.albedo_texture != null:
		return _textured_mat(std.albedo_texture)
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

func _textured_mat(tex: Texture2D) -> Material:
	if not _tex_cache.has(tex):
		var m := ShaderMaterial.new()
		m.shader = _scenery_shader
		m.set_shader_parameter("palette", tex if tex else _default_palette)
		m.set_shader_parameter("has_water", false)
		m.set_shader_parameter("has_reveal", false)
		_tex_cache[tex] = m
	return _tex_cache[tex]

func _glow_mat(col: Color) -> Material:
	var key := col.to_html(false)
	if not _glow_cache.has(key):
		var m := ShaderMaterial.new()
		m.shader = _glow_shader
		m.set_shader_parameter("glow_color", col)
		_glow_cache[key] = m
	return _glow_cache[key]
