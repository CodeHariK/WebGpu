#include "resources.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

FolioResources::FolioResources() {}

FolioResources::~FolioResources() {}

Dictionary FolioResources::load(
		const Array &p_files,
		const Callable &p_progress_callback
) {
	Dictionary loaded;
	const int total = p_files.size();
	int remaining = total;

	ResourceLoader *rl = ResourceLoader::get_singleton();

	for (int i = 0; i < total; i++) {
		const Array file = p_files[i];
		if (file.size() < 2) {
			UtilityFunctions::printerr("FolioResources: entry ", i, " needs at least [ key, path ].");
			remaining--;
			if (p_progress_callback.is_valid()) {
				p_progress_callback.call(remaining, total);
			}
			continue;
		}

		const String key = file[0];
		const String path = file[1];

		Ref<Resource> resource;

		// Serve from cache, otherwise load (Godot infers the type from the file).
		if (cache.has(path)) {
			resource = cache[path];
		} else if (rl) {
			resource = rl->load(path);
			if (resource.is_valid()) {
				cache.insert(path, resource);
			}
		}

		if (resource.is_null()) {
			UtilityFunctions::printerr("FolioResources: couldn't load '", path, "'");
			remaining--;
			if (p_progress_callback.is_valid()) {
				p_progress_callback.call(remaining, total);
			}
			continue;
		}

		// Optional per-item modifier Callable (entry[3]); entry[2] type is ignored.
		if (file.size() >= 4) {
			const Variant modifier_v = file[3];
			if (modifier_v.get_type() == Variant::CALLABLE) {
				const Callable modifier = modifier_v;
				if (modifier.is_valid()) {
					modifier.call(resource);
				}
			}
		}

		loaded[key] = resource;

		remaining--;
		if (p_progress_callback.is_valid()) {
			p_progress_callback.call(remaining, total);
		}
	}

	return loaded;
}

bool FolioResources::has_cached(const String &p_path) const { return cache.has(p_path); }

Ref<Resource> FolioResources::get_cached(const String &p_path) const {
	if (cache.has(p_path)) {
		return cache[p_path];
	}
	return Ref<Resource>();
}

void FolioResources::clear_cache() { cache.clear(); }

void FolioResources::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load", "files", "progress_callback"), &FolioResources::load, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("has_cached", "path"), &FolioResources::has_cached);
	ClassDB::bind_method(D_METHOD("get_cached", "path"), &FolioResources::get_cached);
	ClassDB::bind_method(D_METHOD("clear_cache"), &FolioResources::clear_cache);
}

} // namespace godot
