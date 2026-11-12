extends SceneTree
## Builds waterfall_simple/waterfall_full.tscn (make waterfall-full-build): the simple waterfall's
## pieces laid out like the whole Blender scene, from waterfall_simple/full_layout.json
## (exported once from the Blender scene, in the sheet's frame):
##   8 sheets (two tiers of four: the one sheet mesh, each its own place, size and pattern_offset)
##   the pool below and the pool above, a froth tube along the foot of each tier and one along the
##   lip of the lower falls, stones scattered up the sides of both tiers, puffs and spray at both
##   feet, a few bubble squares, Blender's camera at frame 60 with its depth of field
## Every piece uses the finished shaders of waterfall_simple/ (the step tracks' last steps).
## The meshes it needs are saved beside it (meshes/*.res) so the scene doesn't carry them.

const DIR := "res://waterfall_simple/"
const StoneScatter := preload("res://waterfall_simple/stone_scatter.gd")
const FrothTube := preload("res://waterfall_simple/froth_tube.gd")

var scene_root := Node3D.new()


func _initialize() -> void:
	var layout: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(DIR + "full_layout.json"))
	scene_root.name = "WaterfallFull"
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIR + "meshes"))
	var sheet_mesh := _mesh_from("waterfall_sheet.glb", "sheet.res")
	var sheet_material := _material("waterfall_simple.gdshader")
	var lower := Rect2() # x span (position.x .. end.x) of each tier's sheets
	var upper := Rect2()
	var lower_foot := Vector3.ZERO
	var upper_foot := Vector3.ZERO
	var n_lower := 0
	var n_upper := 0
	for i in layout["sheets"].size():
		var s: Dictionary = layout["sheets"][i]
		var sheet := MeshInstance3D.new()
		sheet.name = String(s["name"]).replace(".", "_")
		sheet.mesh = sheet_mesh
		sheet.material_override = sheet_material
		sheet.transform = _transform(s["transform"])
		sheet.set_instance_shader_parameter("pattern_offset", Vector2(i * 3.17, i * 1.71))
		sheet.extra_cull_margin = 1.0
		_add(sheet)
		var foot := sheet.transform.origin
		if s["upper"]:
			upper = Rect2(foot.x, 0, 0, 0) if n_upper == 0 else upper.expand(Vector2(foot.x, 0))
			upper_foot += foot
			n_upper += 1
		else:
			lower = Rect2(foot.x, 0, 0, 0) if n_lower == 0 else lower.expand(Vector2(foot.x, 0))
			lower_foot += foot
			n_lower += 1
	lower_foot /= n_lower
	upper_foot /= n_upper
	# the sheets' meshes are centred and 8 m wide: the tiers reach 4 m past their outer sheets
	var lower_mid := (lower.position.x + lower.end.x) / 2.0
	var lower_half := (lower.end.x - lower.position.x) / 2.0 + 4.0
	var upper_mid := (upper.position.x + upper.end.x) / 2.0
	var upper_half := (upper.end.x - upper.position.x) / 2.0 + 4.0

	var pool_material := _material("waterfall_pool.gdshader")
	var pool := _mesh_instance("LowerPool", _mesh_from("waterfall_pool.glb", "pool.res"), pool_material)
	var upper_pool := _mesh_instance("UpperPool", _mesh_from("waterfall_upper_pool.glb", "upper_pool.res"), pool_material)
	var lower_water := pool.mesh.get_aabb().position.y
	var upper_water := upper_pool.mesh.get_aabb().position.y

	_froth("LowerFroth", Vector3(lower_mid, lower_water, lower_foot.z + 1.6), lower_half * 2.0 + 1.0)
	_froth("UpperFroth", Vector3(upper_mid, upper_water, upper_foot.z + 1.6), upper_half * 2.0 + 1.0)
	_froth("LipFroth", Vector3(lower_mid, lower_foot.y + 13.3, lower_foot.z - 3.5), lower_half * 2.0, 1.2)

	_stones("LowerStones", Vector3(lower_mid, 0.0, lower_foot.z), lower_half, 11)
	_stones("UpperStones", Vector3(upper_mid, upper_foot.y, upper_foot.z), upper_half, 23)

	_particles("LowerParticles", Vector3(lower_mid, lower_water, lower_foot.z + 1.6), lower_half)
	_particles("UpperParticles", Vector3(upper_mid, upper_water, upper_foot.z + 1.6), upper_half)
	_bubbles(Vector3(lower_mid, lower_water, lower_foot.z + 3.0), lower_half)

	_camera(layout["camera"])
	_environment()
	_own(scene_root)
	var scene := PackedScene.new()
	scene.pack(scene_root)
	ResourceSaver.save(scene, DIR + "waterfall_full.tscn")
	print("saved %swaterfall_full.tscn" % DIR)
	quit()


func _add(p_node: Node) -> void:
	scene_root.add_child(p_node)


# Godot's layout transform: basis columns x, y, z, then the origin.
func _transform(m: Array) -> Transform3D:
	return Transform3D(Basis(Vector3(m[0], m[1], m[2]), Vector3(m[3], m[4], m[5]), Vector3(m[6], m[7], m[8])),
			Vector3(m[9], m[10], m[11]))


# The first mesh in an imported glb, saved as its own .res (so the scene points at it).
func _mesh_from(p_glb: String, p_res: String) -> Mesh:
	var scene: Node = load(DIR + p_glb).instantiate()
	var mesh: Mesh = (scene.find_children("*", "MeshInstance3D", true, false)[0] as MeshInstance3D).mesh
	scene.free()
	ResourceSaver.save(mesh, DIR + "meshes/" + p_res)
	return load(DIR + "meshes/" + p_res)


func _material(p_shader: String) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = load(DIR + p_shader)
	return material


func _mesh_instance(p_name: String, p_mesh: Mesh, p_material: Material) -> MeshInstance3D:
	var node := MeshInstance3D.new()
	node.name = p_name
	node.mesh = p_mesh
	node.material_override = p_material
	_add(node)
	return node


func _froth(p_name: String, p_at: Vector3, p_length: float, p_radius := 1.6) -> void:
	var tube := MeshInstance3D.new()
	tube.name = p_name
	tube.set_script(FrothTube)
	tube.set("length", p_length)
	tube.set("radius", p_radius)
	var material := _material("waterfall_froth.gdshader")
	material.set_shader_parameter("tube_length", p_length)
	material.set_shader_parameter("tube_radius", p_radius)
	tube.material_override = material
	tube.extra_cull_margin = 2.0
	tube.position = p_at
	_add(tube)


func _stones(p_name: String, p_at: Vector3, p_half_width: float, p_seed: int) -> void:
	var stones := Node3D.new()
	stones.name = p_name
	stones.set_script(StoneScatter)
	stones.set("seed", p_seed)
	stones.set("half_width", p_half_width)
	stones.set("along_pool", 10)
	stones.set("material", _material("waterfall_stone.gdshader"))
	stones.position = p_at
	_add(stones)


func _particles(p_name: String, p_at: Vector3, p_half_width: float) -> void:
	var group := Node3D.new()
	group.name = p_name
	group.position = p_at
	_add(group)
	var puffs := GPUParticles3D.new()
	puffs.name = "Puffs"
	puffs.material_override = _material("waterfall_puff.gdshader")
	puffs.amount = int(36 * p_half_width / 4.5)
	puffs.lifetime = 1.8
	puffs.preprocess = 2.0
	puffs.visibility_aabb = AABB(Vector3(-p_half_width - 4, -4, -6), Vector3(p_half_width * 2 + 8, 16, 16))
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	process.emission_box_extents = Vector3(p_half_width, 0.2, 0.5)
	process.emission_shape_offset = Vector3(0, 0.9, 0)
	process.direction = Vector3(0, 1, 0.25)
	process.spread = 20.0
	process.initial_velocity_min = 1.5
	process.initial_velocity_max = 3.0
	process.gravity = Vector3(0, -0.8, 0)
	process.damping_min = 0.5
	process.damping_max = 0.8
	process.scale_min = 1.2
	process.scale_max = 2.2
	puffs.process_material = process
	puffs.draw_pass_1 = QuadMesh.new()
	group.add_child(puffs)
	var spray := GPUParticles3D.new()
	spray.name = "Spray"
	spray.amount = int(120 * p_half_width / 4.5)
	spray.lifetime = 1.4
	spray.preprocess = 2.0
	spray.visibility_aabb = puffs.visibility_aabb
	var spray_process := ParticleProcessMaterial.new()
	spray_process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	spray_process.emission_box_extents = Vector3(p_half_width, 0.2, 0.6)
	spray_process.emission_shape_offset = Vector3(0, 1.0, 0.3)
	spray_process.direction = Vector3(0, 1, 0.35)
	spray_process.spread = 25.0
	spray_process.initial_velocity_min = 5.0
	spray_process.initial_velocity_max = 8.0
	spray_process.gravity = Vector3(0, -9.81, 0)
	spray_process.scale_min = 0.6
	spray_process.scale_max = 1.2
	spray.process_material = spray_process
	var droplet := SphereMesh.new()
	droplet.radius = 0.07
	droplet.height = 0.14
	droplet.radial_segments = 8
	droplet.rings = 4
	var white := StandardMaterial3D.new()
	white.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	droplet.material = white
	spray.draw_pass_1 = droplet
	group.add_child(spray)


func _bubbles(p_at: Vector3, p_half_width: float) -> void:
	var group := Node3D.new()
	group.name = "Bubbles"
	_add(group)
	var square := PlaneMesh.new()
	square.size = Vector2(2, 2)
	square.subdivide_width = 12
	square.subdivide_depth = 12
	square.orientation = PlaneMesh.FACE_Z
	var material := _material("waterfall_bubble.gdshader")
	var rng := RandomNumberGenerator.new()
	rng.seed = 5
	for i in 8:
		var bubble := MeshInstance3D.new()
		bubble.name = "Bubble%d" % i
		bubble.mesh = square
		bubble.material_override = material
		bubble.extra_cull_margin = 1.0
		var size := rng.randf_range(1.2, 1.8)
		bubble.transform = Transform3D(Basis().scaled(Vector3.ONE * size),
				p_at + Vector3(rng.randf_range(-p_half_width, p_half_width), rng.randf_range(1.0, 4.0), rng.randf_range(0.0, 2.0)))
		group.add_child(bubble)


func _camera(c: Dictionary) -> void:
	var camera := Camera3D.new()
	camera.name = "Camera3D"
	# the layout turned the camera's own axes too (as for the meshes, whose glb is in Godot's axes);
	# a camera looks down -z with y up in both programs, so turn its own axes back
	var own_axes := Transform3D(Basis(Vector3(1, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0)), Vector3.ZERO)
	camera.transform = (_transform(c["transform"]) * own_axes).orthonormalized()
	camera.keep_aspect = Camera3D.KEEP_WIDTH
	camera.fov = c["fov_x"]
	camera.far = 500.0
	if c["focus"] != null:
		var focus: float = c["focus"]
		var attributes := CameraAttributesPractical.new()
		attributes.dof_blur_far_enabled = true
		attributes.dof_blur_far_distance = focus + 6.0
		attributes.dof_blur_far_transition = 20.0
		attributes.dof_blur_near_enabled = true
		attributes.dof_blur_near_distance = focus - 6.0
		attributes.dof_blur_near_transition = 15.0
		attributes.dof_blur_amount = 0.25
		camera.attributes = attributes
	_add(camera)


func _environment() -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.25, 0.25, 0.25)
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	var node := WorldEnvironment.new()
	node.name = "WorldEnvironment"
	node.environment = env
	_add(node)


func _own(p_node: Node) -> void:
	for child in p_node.get_children():
		child.owner = scene_root
		# the stones are made by their script when the scene starts: don't save them
		if child.get_script() != StoneScatter:
			_own(child)
