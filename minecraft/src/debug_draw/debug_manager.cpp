#include "debug_manager.h"
#include "debug_quad.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

DebugManager *DebugManager::singleton = nullptr;

void DebugManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("draw_line", "id", "start", "end", "thickness", "color", "duration"), &DebugManager::draw_line, DEFVAL(0.05f), DEFVAL(Color(1, 1, 1)), DEFVAL(-1.0f));
	ClassDB::bind_method(D_METHOD("clear_line", "id"), &DebugManager::clear_line);

	ClassDB::bind_method(D_METHOD("draw_text", "id", "text", "pos", "size", "color", "duration"), &DebugManager::draw_text, DEFVAL(0.05f), DEFVAL(Color(1, 1, 1)), DEFVAL(-1.0f));
	ClassDB::bind_method(D_METHOD("clear_text", "id"), &DebugManager::clear_text);

	ClassDB::bind_method(D_METHOD("draw_sphere", "id", "pos", "radius", "color", "duration"), &DebugManager::draw_sphere, DEFVAL(0.5f), DEFVAL(Color(1, 1, 1)), DEFVAL(-1.0f));
	ClassDB::bind_method(D_METHOD("clear_sphere", "id"), &DebugManager::clear_sphere);

	ClassDB::bind_method(D_METHOD("clear_all"), &DebugManager::clear_all);
}

DebugManager::DebugManager() {
	// Let _enter_tree handle singleton assignment
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
	std::vector<String> lines_to_remove;
	std::vector<String> texts_to_remove;
	std::vector<String> spheres_to_remove;

	for (auto &E : lines) {
		if (E.value.duration > 0) {
			E.value.duration -= f_delta;
			if (E.value.duration <= 0) {
				lines_to_remove.push_back(E.key);
			}
		}
	}

	for (auto &E : texts) {
		if (E.value.duration > 0) {
			E.value.duration -= f_delta;
			if (E.value.duration <= 0) {
				texts_to_remove.push_back(E.key);
			}
		}
	}

	for (auto &E : spheres) {
		if (E.value.duration > 0) {
			E.value.duration -= f_delta;
			if (E.value.duration <= 0) {
				spheres_to_remove.push_back(E.key);
			}
		}
	}

	for (const String &id : lines_to_remove) {
		clear_line(id);
	}
	for (const String &id : texts_to_remove) {
		clear_text(id);
	}
	for (const String &id : spheres_to_remove) {
		clear_sphere(id);
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

void DebugManager::draw_text(const String &p_id, const String &p_text, const Vector3 &p_pos, float p_size, const Color &p_color, float p_duration) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!texts.has(p_id)) {
		DebugText dt;
		dt.label = memnew(Label3D);
		dt.label->set_name("DebugText_" + p_id);
		add_child(dt.label);
		dt.label->set_as_top_level(true);

		// Setup default look
		dt.label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		dt.label->set_draw_flag(Label3D::FLAG_FIXED_SIZE, true);

		dt.duration = p_duration;
		texts[p_id] = dt;
	}

	DebugText &dt = texts[p_id];
	dt.label->set_text(p_text);
	dt.label->set_global_position(p_pos);
	dt.label->set_pixel_size(p_size);
	dt.label->set_modulate(p_color);
	dt.label->set_visible(true);

	if (p_duration > 0) {
		dt.duration = p_duration;
	}
}

void DebugManager::clear_text(const String &p_id) {
	if (texts.has(p_id)) {
		DebugText &dt = texts[p_id];
		if (dt.label) {
			dt.label->queue_free();
		}
		texts.erase(p_id);
	}
}

void DebugManager::clear_all() {
	for (auto &E : lines) {
		if (E.value.quad) {
			E.value.quad->queue_free();
		}
	}
	lines.clear();

	for (auto &E : texts) {
		if (E.value.label) {
			E.value.label->queue_free();
		}
	}
	texts.clear();

	for (auto &E : spheres) {
		if (E.value.mesh_instance) {
			E.value.mesh_instance->queue_free();
		}
	}
	spheres.clear();

	for (auto &E : trajectories) {
		Trajectory &traj = E.value;
		for (size_t i = 0; i < traj.points.size(); ++i) {
			clear_line(E.key + String("_") + String::num_int64(i));
		}
	}
	trajectories.clear();
}

void DebugManager::draw_trajectory(const String &p_id, const Vector3 &p_point, float p_delta, float p_interval, int p_max_points, const Color &p_color) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!trajectories.has(p_id)) {
		trajectories[p_id] = Trajectory();
	}

	Trajectory &traj = trajectories[p_id];
	traj.timer += p_delta;

	if (traj.timer >= p_interval) {
		traj.timer = 0.0f;
		traj.points.push_back(p_point);
		if (traj.points.size() > (size_t)p_max_points) {
			traj.points.erase(traj.points.begin());
		}
	}

	if (traj.points.size() > 1) {
		for (size_t i = 0; i < traj.points.size() - 1; ++i) {
			draw_line(p_id + String("_") + String::num_int64(i), traj.points[i], traj.points[i + 1], 0.1f, p_color, 0.15f);
		}
	}
}

void DebugManager::clear_trajectory(const String &p_id) {
	if (trajectories.has(p_id)) {
		Trajectory &traj = trajectories[p_id];
		for (size_t i = 0; i < traj.points.size(); ++i) {
			clear_line(p_id + String("_") + String::num_int64(i));
		}
		trajectories.erase(p_id);
	}
}

void DebugManager::draw_sphere(const String &p_id, const Vector3 &p_pos, float p_radius, const Color &p_color, float p_duration) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!spheres.has(p_id)) {
		DebugSphere ds;
		ds.mesh_instance = memnew(MeshInstance3D);
		ds.mesh_instance->set_name("DebugSphere_" + p_id);
		add_child(ds.mesh_instance);
		ds.mesh_instance->set_as_top_level(true);

		Ref<SphereMesh> sphere_mesh;
		sphere_mesh.instantiate();
		sphere_mesh->set_radius(0.5f);
		sphere_mesh->set_height(1.0f);
		ds.mesh_instance->set_mesh(sphere_mesh);

		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		ds.mesh_instance->set_material_override(mat);

		ds.duration = p_duration;
		spheres[p_id] = ds;
	}

	DebugSphere &ds = spheres[p_id];
	ds.mesh_instance->set_global_position(p_pos);
	ds.mesh_instance->set_scale(Vector3(p_radius * 2.0f, p_radius * 2.0f, p_radius * 2.0f));

	Ref<StandardMaterial3D> mat = ds.mesh_instance->get_material_override();
	if (mat.is_valid()) {
		mat->set_albedo(p_color);
		if (p_color.a < 1.0f) {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		} else {
			mat->set_transparency(BaseMaterial3D::TRANSPARENCY_DISABLED);
		}
	}

	if (p_duration > 0) {
		ds.duration = p_duration;
	}
}

void DebugManager::clear_sphere(const String &p_id) {
	if (spheres.has(p_id)) {
		DebugSphere &ds = spheres[p_id];
		if (ds.mesh_instance) {
			ds.mesh_instance->queue_free();
		}
		spheres.erase(p_id);
	}
}

} // namespace godot
