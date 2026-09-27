extends Node3D
##
## Folio ExplosiveCrates (folio World/ExplosiveCrates + Fireballs).
##
## Loads explosiveCrates.glb and turns each crate into a light, movable
## RigidBody3D that DETONATES when the car rams it fast enough (or when caught in
## another crate's blast -> chain reaction). On detonation, after folio's 0.4s
## fuse: a red->orange fireball flash grows and fades, a radial impulse shoves
## every nearby rigid body outward (folio fireRadius / explosionRadius = 5), and
## the crate is removed.
##
## Materials use the folio palette (crates are palette-textured).
##

@export var glb_path := "res://assets/folio/explosiveCrates/explosiveCrates.glb"
@export var palette_path := "res://assets/folio/explosiveCrates/explosiveCrates_palette.png"
@export var crate_mass := 0.6
@export var trigger_speed := 3.0    # car speed (m/s) needed to set a crate off
@export var fuse_seconds := 0.4     # folio delay between impact and blast
@export var blast_radius := 5.0
@export var blast_strength := 9.0

var _pal_mat: ShaderMaterial
var _crates: Array[RigidBody3D] = []

func _ready() -> void:
	var scn: PackedScene = load(glb_path)
	if scn == null:
		push_warning("FolioExplosiveCrates: could not load " + glb_path)
		return
	_pal_mat = ShaderMaterial.new()
	_pal_mat.shader = load("res://material/shaders/folio/mesh_scenery.gdshader")
	var pal: Texture2D = load(palette_path)
	if pal:
		_pal_mat.set_shader_parameter("palette", pal)
	_pal_mat.set_shader_parameter("has_water", false)
	_pal_mat.set_shader_parameter("has_reveal", false)
	var root: Node3D = scn.instantiate()
	add_child(root)
	for c in root.get_children():
		if c is MeshInstance3D:
			_make_crate(c as MeshInstance3D)

func _make_crate(mi: MeshInstance3D) -> void:
	var xform := mi.global_transform
	var rb := RigidBody3D.new()
	rb.mass = crate_mass
	rb.can_sleep = true
	rb.contact_monitor = true
	rb.max_contacts_reported = 6
	rb.name = str(mi.name) + "Body"
	add_child(rb)
	rb.global_transform = xform

	var parent := mi.get_parent()
	if parent:
		parent.remove_child(mi)
	rb.add_child(mi)
	mi.transform = Transform3D.IDENTITY
	# Folio palette material.
	if mi.mesh:
		for i in range(mi.mesh.get_surface_count()):
			mi.set_surface_override_material(i, _pal_mat)

	# Box collider from the crate's AABB.
	if mi.mesh:
		var aabb := mi.mesh.get_aabb()
		var box := BoxShape3D.new()
		box.size = aabb.size.max(Vector3(0.1, 0.1, 0.1))
		var cs := CollisionShape3D.new()
		cs.shape = box
		rb.add_child(cs)
		cs.position = aabb.get_center()

	rb.set_meta("exploded", false)
	rb.body_entered.connect(_on_crate_hit.bind(rb))
	_crates.append(rb)

# Any body touching the crate: detonate if it's the car above trigger_speed.
# (Chain reactions come from the blast query in _explode, so a crate hit only
# by another crate does not need to test speed here.)
func _on_crate_hit(other: Node, rb: RigidBody3D) -> void:
	if rb == null or bool(rb.get_meta("exploded", false)):
		return
	if other and other.get_class() == "ArcadeVehicle":
		var spd := 0.0
		if other.has_method("get_linear_velocity"):
			spd = (other as RigidBody3D).linear_velocity.length()
		if spd >= trigger_speed:
			_explode(rb)

func _explode(rb: RigidBody3D) -> void:
	if rb == null or bool(rb.get_meta("exploded", false)):
		return
	rb.set_meta("exploded", true)
	var pos := rb.global_position
	# Folio fuse: wait, then flash + shove + remove.
	await get_tree().create_timer(fuse_seconds).timeout
	if not is_inside_tree():
		return
	_spawn_fireball(pos)
	var ss := get_tree().get_first_node_in_group("folio_soundscape")
	if ss and ss.has_method("play_hit"):
		ss.play_hit("explosion", pos, 0.9 + randf() * 0.2)
	_blast(pos, rb)
	if is_instance_valid(rb):
		rb.queue_free()

# Radial impulse to every rigid body in range; chains other crates.
func _blast(pos: Vector3, source: RigidBody3D) -> void:
	var space := get_world_3d().direct_space_state
	var shape := SphereShape3D.new()
	shape.radius = blast_radius
	var q := PhysicsShapeQueryParameters3D.new()
	q.shape = shape
	q.transform = Transform3D(Basis(), pos)
	q.collision_mask = 0xFFFFFFFF
	q.collide_with_bodies = true
	var hits := space.intersect_shape(q, 48)
	for h in hits:
		var col = h.get("collider")
		if col == source or not (col is RigidBody3D):
			continue
		var body := col as RigidBody3D
		var to := body.global_position - pos
		var dist := to.length()
		var dir := to.normalized() if dist > 0.001 else Vector3.UP
		var falloff := clampf(1.0 - dist / blast_radius, 0.0, 1.0)
		# A frozen (far/inactive) body ignores impulses; wake it so it reacts.
		if body.freeze:
			body.freeze = false
		body.apply_impulse((dir + Vector3.UP * 0.4).normalized() * blast_strength * falloff)
		# Chain: other crates in range go off too.
		if body in _crates and not bool(body.get_meta("exploded", false)):
			_explode(body)

func _spawn_fireball(pos: Vector3) -> void:
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	var mi := MeshInstance3D.new()
	mi.mesh = sphere
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(1.0, 0.35, 0.06)
	mat.emission_enabled = true
	mat.emission = Color(1.0, 0.45, 0.12)
	mat.emission_energy_multiplier = 6.0
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mi.material_override = mat
	add_child(mi)
	mi.global_position = pos
	mi.scale = Vector3.ONE * 0.5
	# folio: grow to fireRadius over ~0.6s, then fade out.
	var tw := create_tween()
	tw.set_parallel(true)
	tw.tween_property(mi, "scale", Vector3.ONE * blast_radius, 0.6).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	tw.tween_property(mat, "albedo_color:a", 0.0, 1.1).set_delay(0.25)
	tw.tween_property(mat, "emission_energy_multiplier", 0.0, 1.1).set_delay(0.25)
	tw.set_parallel(false)
	tw.tween_callback(mi.queue_free)


# Test helper: detonate the first live crate and return its position.
func detonate_test() -> Vector3:
	for rb in _crates:
		if is_instance_valid(rb) and not bool(rb.get_meta("exploded", false)):
			var p := rb.global_position
			_explode(rb)
			return p
	return Vector3.ZERO
