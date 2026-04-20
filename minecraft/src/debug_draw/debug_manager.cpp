#include "debug_manager.h"
#include "debug_quad.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

DebugManager *DebugManager::singleton = nullptr;

void DebugManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("draw_line", "id", "start", "end", "thickness", "color", "duration"), &DebugManager::draw_line, DEFVAL(0.05f), DEFVAL(Color(1, 1, 1)), DEFVAL(-1.0f));
	ClassDB::bind_method(D_METHOD("clear_line", "id"), &DebugManager::clear_line);
	ClassDB::bind_method(D_METHOD("clear_all"), &DebugManager::clear_all);
}

DebugManager::DebugManager() {
	if (singleton == nullptr) {
		singleton = this;
	}
}

DebugManager::~DebugManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

DebugManager *DebugManager::get_singleton() {
	return singleton;
}

void DebugManager::_enter_tree() {
	if (singleton != nullptr && singleton != this) {
		UtilityFunctions::printerr("DebugManager Error: Multiple instances detected! Only one is allowed.");
		queue_free();
		return;
	}
	singleton = this;
	UtilityFunctions::print("DebugManager: Singleton initialized.");
}

void DebugManager::_exit_tree() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void DebugManager::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	float f_delta = (float)delta;
	std::vector<String> to_remove;

	for (auto &E : lines) {
		if (E.value.duration > 0) {
			E.value.duration -= f_delta;
			if (E.value.duration <= 0) {
				to_remove.push_back(E.key);
			}
		}
	}

	for (const String &id : to_remove) {
		clear_line(id);
	}
}

void DebugManager::draw_line(const String &p_id, const Vector3 &p_start, const Vector3 &p_end, float p_thickness, const Color &p_color, float p_duration) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!lines.has(p_id)) {
		DebugLine dl;
		dl.quad = memnew(DebugLineQuad);
		dl.quad->set_name("DebugLine_" + p_id);
		add_child(dl.quad);
		dl.quad->set_as_top_level(true);
		
		dl.duration = p_duration;
		lines[p_id] = dl;
	}

	DebugLine &dl = lines[p_id];
	dl.quad->set_line(p_start, p_end, p_thickness);
	dl.quad->set_color(p_color);
	dl.quad->set_visible(true);
	
	// Update duration if a positive one is provided, otherwise keep current (could be -1 for persistent)
	if (p_duration > 0) {
		dl.duration = p_duration;
	}
}

void DebugManager::clear_line(const String &p_id) {
	if (lines.has(p_id)) {
		DebugLine &dl = lines[p_id];
		if (dl.quad) {
			dl.quad->queue_free();
		}
		lines.erase(p_id);
	}
}

void DebugManager::clear_all() {
	for (auto &E : lines) {
		if (E.value.quad) {
			E.value.quad->queue_free();
		}
	}
	lines.clear();
}

} // namespace godot
