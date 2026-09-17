#ifndef FOLIO_RESOURCES_H
#define FOLIO_RESOURCES_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

/**
 * Folio port — ResourcesLoader  (registered as `FolioResources`)
 * --------------------------------------------------------------
 * Faithful port of folio-2025 `Game/ResourcesLoader.js`: a keyed batch loader.
 *
 * Folio builds Three loaders (GLTF + Draco + KTX2, TextureLoader) and loads a
 * list of `[ key, path, type, modifier? ]`, caching by path and resolving a
 * `{ key: resource }` object with a progress callback.
 *
 * In Godot most of that machinery is unnecessary: the editor import pipeline
 * already turns `.glb` -> PackedScene and `.png`/`.ktx` -> Texture2D, and
 * `ResourceLoader` selects the importer from the file. So we drop the loader-type
 * system and keep only the *behaviour* that consumers rely on:
 *   - load a batch spec, each entry `[ key, path, (type ignored), modifier? ]`,
 *   - serve repeats from a path cache,
 *   - run an optional `modifier` Callable on each loaded resource,
 *   - report progress `(remaining, total)` after each item,
 *   - return a Dictionary `{ key: resource }`.
 *
 * Deviations (documented):
 *   - `type` (entry[2]) is accepted for spec parity but ignored — Godot infers it.
 *   - Most folio texture modifiers (minFilter/wrap/flipY/colorSpace/mipmaps) are
 *     IMPORT settings in Godot (.import), not runtime mutations — set them there.
 *     The `modifier` Callable remains for genuine runtime post-processing.
 *   - Loading is synchronous here (folio is async/Promise). The threaded upgrade
 *     path is `ResourceLoader::load_threaded_request` + polling, wired to the
 *     loading screen when we port the boot sequence.
 *   - A failed load is logged and skipped (folio rejects the whole batch).
 *
 * Renamed to `FolioResources` to avoid confusion with Godot core `ResourceLoader`.
 */
class FolioResources : public RefCounted {
	GDCLASS(FolioResources,
			RefCounted)

private:
	HashMap<String, Ref<Resource>> cache; // by path

protected:
	static void _bind_methods();

public:
	FolioResources();
	~FolioResources();

	// files: Array of [ key:String, path:String, type?:String, modifier?:Callable ].
	// Returns { key: Resource }. progress_callback(remaining, total) after each.
	Dictionary
	load(const Array &p_files,
		 const Callable &p_progress_callback);

	bool has_cached(const String &p_path) const;
	Ref<Resource> get_cached(const String &p_path) const;
	void clear_cache();
};

} // namespace godot

#endif // FOLIO_RESOURCES_H
