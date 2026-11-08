extends SceneTree
## Runs INSIDE the minecraft game project (it needs the game's GDExtension to build the meshes):
## `make liquid-sample` (../../Makefile) copies it there, runs it and deletes the copy.
##
## Loads an inspect record's map (the game's user://inspect/<NAME>.json / .mct; NAME defaults to
## "last"), builds it, and saves into the folder given after `--`:
##   water.res    the MC water mesh, exactly as the liquid shaders get it (COLOR flow, UV, UV2,
##                CUSTOM0 depth), no material
##   terrain.res  every terrain chunk merged into one mesh, with copies of its two materials
##                (grass, vertex-coloured cliff), so nothing points back into the game project
## Both in grid space (the grid sits at the origin). Resources saved compressed.

func _init() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		push_error("export_liquid_sample: give the output folder after --")
		quit(1)
		return
	var out_dir: String = args[0]
	var name: String = OS.get_environment("NAME") if OS.get_environment("NAME") != "" else "last"
	var record = JSON.parse_string(FileAccess.get_file_as_string("user://inspect/%s.json" % name))
	var scene: Node = load(record.scene).instantiate()
	root.add_child(scene)
	for i in 10:
		await process_frame
	var grid = scene.get_node("MCGrid")
	grid.load_grid("user://inspect/%s.mct" % name)
	grid.flush_dirty_chunks()

	# the water mesh, without the game's material
	var water: ArrayMesh = grid.get_node("Water").mesh.duplicate()
	for s in water.get_surface_count():
		water.surface_set_material(s, null)
	_save(water, out_dir.path_join("water.res"))

	# the terrain: one SurfaceTool per material, every chunk appended (chunks are in grid space)
	var tools := {} # material -> SurfaceTool
	for child in grid.get_children():
		if not (child is MeshInstance3D and String(child.name).begins_with("Chunk")):
			continue
		var mesh: ArrayMesh = child.mesh
		for s in mesh.get_surface_count():
			var material: Material = mesh.surface_get_material(s)
			if not tools.has(material):
				var st := SurfaceTool.new()
				st.begin(Mesh.PRIMITIVE_TRIANGLES)
				tools[material] = st
			tools[material].append_from(mesh, s, Transform3D())
	var terrain := ArrayMesh.new()
	for material in tools:
		tools[material].commit(terrain)
		var copy: Material = material.duplicate() if material else null # standalone: no game paths
		terrain.surface_set_material(terrain.get_surface_count() - 1, copy)
	_save(terrain, out_dir.path_join("terrain.res"))
	print("DONE")
	quit()


func _save(p_mesh: ArrayMesh, p_path: String) -> void:
	var error := ResourceSaver.save(p_mesh, p_path, ResourceSaver.FLAG_COMPRESS)
	var size := FileAccess.get_file_as_bytes(p_path).size() if error == OK else 0
	print("saved %s: %d surfaces, %d KB (error %d)" % [p_path, p_mesh.get_surface_count(), size / 1024, error])
