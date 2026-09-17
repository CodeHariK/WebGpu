extends SceneTree

# Throwaway smoke test for FolioResources.load() — safe to delete.
func _initialize():
	var r = FolioResources.new()
	var hits := {"count": 0}
	var cb := func(remaining, total): hits.count += 1; print("progress ", remaining, "/", total)
	var res = r.load([["core", "res://scene/folio/core.tscn"]], cb)
	print("keys=", res.keys())
	print("is_packedscene=", res.get("core") is PackedScene)
	print("progress_calls=", hits.count)
	print("cached=", r.has_cached("res://scene/folio/core.tscn"))
	var res2 = r.load([["core2", "res://scene/folio/core.tscn"]])  # cache hit path
	print("cache_hit_same_obj=", res2.get("core2") == res.get("core"))
	quit()
