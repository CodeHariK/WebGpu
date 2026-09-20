extends Node3D

# Demo of FolioInstancedGroup: scatter bushes on grassy terrain (samples the
# terrain data map's G channel on the CPU) and instance them in one draw call.
# `make run_folio_scatter`. `--phase=<0..1>` day phase, `--shot=<name>`, `--fps=N`.
const SUBDIV := 128.0

var _img: Image

func _g_at(x: float, z: float) -> float:
	var u := x / SUBDIV / 1.5 + 0.5
	var v := z / SUBDIV / 1.5 + 0.5
	if u < 0.0 or u > 1.0 or v < 0.0 or v > 1.0:
		return 0.0
	var px := int(clamp(u * float(_img.get_width() - 1), 0, _img.get_width() - 1))
	var py := int(clamp(v * float(_img.get_height() - 1), 0, _img.get_height() - 1))
	return _img.get_pixel(px, py).g

func _scatter() -> void:
	_img = Image.load_from_file("res://material/textures/folio/terrain_data.png")
	if _img == null:
		return
	var transforms: Array[Transform3D] = []
	var rng := RandomNumberGenerator.new()
	rng.seed = 12345
	var tries := 0
	while transforms.size() < 350 and tries < 6000:
		tries += 1
		var x := rng.randf_range(-88.0, 88.0)
		var z := rng.randf_range(-88.0, 88.0)
		if _g_at(x, z) < 0.6:
			continue
		var s := rng.randf_range(0.7, 1.5)
		var b := Basis(Vector3.UP, rng.randf() * TAU).scaled(Vector3(s, s * rng.randf_range(0.8, 1.3), s))
		transforms.append(Transform3D(b, Vector3(x, 0.35 * s, z)))
	$Bushes.scatter(transforms)

	# Trees: fewer, larger, min-spaced on grass.
	var tree_t: Array[Transform3D] = []
	var placed: Array[Vector2] = []
	var t2 := 0
	while tree_t.size() < 60 and t2 < 6000:
		t2 += 1
		var tx := rng.randf_range(-85.0, 85.0)
		var tz := rng.randf_range(-85.0, 85.0)
		if _g_at(tx, tz) < 0.7:
			continue
		var too_close := false
		for q in placed:
			if Vector2(tx, tz).distance_to(q) < 7.0:
				too_close = true
				break
		if too_close:
			continue
		placed.append(Vector2(tx, tz))
		var ts := rng.randf_range(0.9, 1.4)
		var tb := Basis(Vector3.UP, rng.randf() * TAU).scaled(Vector3(ts, ts, ts))
		tree_t.append(Transform3D(tb, Vector3(tx, 0.0, tz)))
	$Trees.scatter(tree_t)

func _ready() -> void:
	var fps := 60
	var phase := -1.0
	var shot := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--fps="):
			fps = int(a.substr(6))
		elif a.begins_with("--phase="):
			phase = a.substr(8).to_float()
		elif a.begins_with("--shot="):
			shot = a.substr(7)
	Engine.max_fps = fps
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
		$Cam.look_at(Vector3(0, 0, 0), Vector3(0, 1, 0))
	var g := get_node_or_null("Game")
	if g and g.has_method("get_day_cycles") and phase >= 0.0:
		g.get_day_cycles().set_progress_override(phase)
	_scatter()
	if shot != "":
		await get_tree().create_timer(0.8).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/" + shot + ".png")
		get_tree().quit()
