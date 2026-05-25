#include "procedural_lofter.h"
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// ==========================================
// ProceduralLofter Implementation
// ==========================================

void ProceduralLofter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_slices_array", "slices"), &ProceduralLofter::set_slices_array);
	ClassDB::bind_method(D_METHOD("get_slices_array"), &ProceduralLofter::get_slices_array);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &ProceduralLofter::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &ProceduralLofter::get_material);
	ClassDB::bind_method(D_METHOD("set_flat_shaded", "flat_shaded"), &ProceduralLofter::set_flat_shaded);
	ClassDB::bind_method(D_METHOD("get_flat_shaded"), &ProceduralLofter::get_flat_shaded);
	ClassDB::bind_method(D_METHOD("add_slice", "slice", "position"), &ProceduralLofter::add_slice);
	ClassDB::bind_method(D_METHOD("set_blend_factor", "factor"), &ProceduralLofter::set_blend_factor);
	ClassDB::bind_method(D_METHOD("get_blend_factor"), &ProceduralLofter::get_blend_factor);
	ClassDB::bind_method(D_METHOD("set_segments", "segments"), &ProceduralLofter::set_segments);
	ClassDB::bind_method(D_METHOD("get_segments"), &ProceduralLofter::get_segments);

	ClassDB::bind_method(D_METHOD("generate_lofted_mesh", "slices", "transforms"), &ProceduralLofter::generate_lofted_mesh);
	ClassDB::bind_method(D_METHOD("bake"), &ProceduralLofter::bake);
	ClassDB::bind_method(D_METHOD("update_loft"), &ProceduralLofter::update_loft);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "slices_array", PROPERTY_HINT_ARRAY_TYPE, "Curve3D"), "set_slices_array", "get_slices_array");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat_shaded"), "set_flat_shaded", "get_flat_shaded");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blend_factor"), "set_blend_factor", "get_blend_factor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "segments"), "set_segments", "get_segments");
}

ProceduralLofter::ProceduralLofter() {
	blend_factor = 0.0f;
	segments = 16;
	flat_shaded = false;
}

ProceduralLofter::~ProceduralLofter() {
	if (last_connected_curve.is_valid()) {
		last_connected_curve->disconnect("changed", Callable(this, "update_loft"));
	}
	for (auto &slice : last_connected_slices) {
		if (slice.is_valid()) {
			slice->disconnect("changed", Callable(this, "update_loft"));
		}
	}
}

void ProceduralLofter::set_slices_array(const TypedArray<Curve3D> &p_slices) {
	slices_array = p_slices;
	is_dirty = true;
}

TypedArray<Curve3D> ProceduralLofter::get_slices_array() const {
	return slices_array;
}

void ProceduralLofter::set_material(const Ref<Material> &p_material) {
	material = p_material;
	if (mesh_instance) {
		mesh_instance->set_material_override(material);
	}
}

Ref<Material> ProceduralLofter::get_material() const {
	return material;
}

void ProceduralLofter::set_flat_shaded(bool p_flat) {
	flat_shaded = p_flat;
	is_dirty = true;
}

bool ProceduralLofter::get_flat_shaded() const {
	return flat_shaded;
}

void ProceduralLofter::add_slice(Ref<Curve3D> p_slice, float p_position) {
	if (p_slice.is_valid()) {
		slices_array.push_back(p_slice);
		is_dirty = true;
	}
}

void ProceduralLofter::set_blend_factor(float p_factor) {
	blend_factor = p_factor;
	is_dirty = true;
}
float ProceduralLofter::get_blend_factor() const { return blend_factor; }

void ProceduralLofter::set_segments(int p_segments) {
	segments = p_segments;
	is_dirty = true;
}
int ProceduralLofter::get_segments() const { return segments; }

Ref<ArrayMesh> ProceduralLofter::generate_lofted_mesh(TypedArray<Curve3D> slices, TypedArray<Transform3D> transforms) const {
	int num_slices = slices.size();
	int num_transforms = transforms.size();
	if (num_slices < 2 || num_transforms < num_slices)
		return Ref<ArrayMesh>();

	MeshData mesh_data;

	// STEP 1: Transform 2D profiles into 3D space and store vertices
	std::vector<PackedVector3Array> all_slice_vertices(num_slices);
	for (int i = 0; i < num_slices; i++) {
		Ref<Curve3D> slice = slices[i];
		if (slice.is_null())
			continue;

		Transform3D transform = transforms[i];
		int pt_count = slice->get_point_count();
		all_slice_vertices[i].resize(pt_count);

		for (int j = 0; j < pt_count; j++) {
			Vector3 p3d = slice->get_point_position(j);
			// Ignore Y axis of Curve3D and map X/Z coordinates to local cross-section 2D plane (local Z=0)
			Vector3 local_3d = Vector3(p3d.x, p3d.z, 0.0);
			Vector3 world_3d = transform.xform(local_3d);
			all_slice_vertices[i][j] = world_3d;
		}
	}

	if (flat_shaded) {
		// STEP 2 (Flat Shading): Add each triangle independently (no sharing)
		int current_vertex_index = 0;
		for (int i = 0; i < num_slices - 1; i++) {
			Ref<Curve3D> slice_A = slices[i];
			Ref<Curve3D> slice_B = slices[i + 1];

			if (slice_A.is_null() || slice_B.is_null())
				continue;

			int verts_A = all_slice_vertices[i].size();
			int verts_B = all_slice_vertices[i + 1].size();

			if (verts_A < 3 || verts_B < 3)
				continue;

			bool a_is_larger = (verts_A >= verts_B);
			int max_verts = a_is_larger ? verts_A : verts_B;
			int min_verts = a_is_larger ? verts_B : verts_A;

			for (int j = 0; j < max_verts; j++) {
				int current_major = j;
				int next_major = (j + 1) % max_verts;

				int current_minor = (int)(((float)current_major / max_verts) * min_verts);
				int next_minor = (int)(((float)next_major / max_verts) * min_verts);

				int current_A = a_is_larger ? current_major : current_minor;
				int next_A = a_is_larger ? next_major : next_minor;
				int current_B = a_is_larger ? current_minor : current_major;
				int next_B = a_is_larger ? next_minor : next_major;

				Vector3 v_A_curr = all_slice_vertices[i][current_A];
				Vector3 v_A_next = all_slice_vertices[i][next_A];
				Vector3 v_B_curr = all_slice_vertices[i + 1][current_B];
				Vector3 v_B_next = all_slice_vertices[i + 1][next_B];

				// Build Primary Triangle (CCW Winding Order)
				Vector3 n_primary = (v_B_curr - v_A_curr).cross(v_A_next - v_A_curr);
				if (!n_primary.is_zero_approx()) {
					n_primary = n_primary.normalized();
				} else {
					n_primary = Vector3(0, 1, 0);
				}

				mesh_data.vertices.push_back(v_A_curr);
				mesh_data.vertices.push_back(v_A_next);
				mesh_data.vertices.push_back(v_B_curr);

				mesh_data.normals.push_back(n_primary);
				mesh_data.normals.push_back(n_primary);
				mesh_data.normals.push_back(n_primary);

				mesh_data.indices.push_back(current_vertex_index++);
				mesh_data.indices.push_back(current_vertex_index++);
				mesh_data.indices.push_back(current_vertex_index++);

				// Build Secondary Triangle if the minor ring advanced
				if (current_minor != next_minor) {
					Vector3 n_secondary = (v_B_curr - v_A_next).cross(v_B_next - v_A_next);
					if (!n_secondary.is_zero_approx()) {
						n_secondary = n_secondary.normalized();
					} else {
						n_secondary = Vector3(0, 1, 0);
					}

					mesh_data.vertices.push_back(v_A_next);
					mesh_data.vertices.push_back(v_B_next);
					mesh_data.vertices.push_back(v_B_curr);

					mesh_data.normals.push_back(n_secondary);
					mesh_data.normals.push_back(n_secondary);
					mesh_data.normals.push_back(n_secondary);

					mesh_data.indices.push_back(current_vertex_index++);
					mesh_data.indices.push_back(current_vertex_index++);
					mesh_data.indices.push_back(current_vertex_index++);
				}
			}
		}
	} else {
		// STEP 2 (Smooth Shading): Add all vertices and index them to share normals
		std::vector<std::vector<int>> vertex_indices(num_slices);
		int global_vertex_index = 0;
		for (int i = 0; i < num_slices; i++) {
			vertex_indices[i].resize(all_slice_vertices[i].size());
			for (int j = 0; j < all_slice_vertices[i].size(); j++) {
				mesh_data.vertices.push_back(all_slice_vertices[i][j]);
				mesh_data.normals.push_back(Vector3(0, 0, 0));
				vertex_indices[i][j] = global_vertex_index++;
			}
		}

		for (int i = 0; i < num_slices - 1; i++) {
			Ref<Curve3D> slice_A = slices[i];
			Ref<Curve3D> slice_B = slices[i + 1];

			if (slice_A.is_null() || slice_B.is_null())
				continue;

			int verts_A = all_slice_vertices[i].size();
			int verts_B = all_slice_vertices[i + 1].size();

			if (verts_A < 3 || verts_B < 3)
				continue;

			bool a_is_larger = (verts_A >= verts_B);
			int max_verts = a_is_larger ? verts_A : verts_B;
			int min_verts = a_is_larger ? verts_B : verts_A;

			for (int j = 0; j < max_verts; j++) {
				int current_major = j;
				int next_major = (j + 1) % max_verts;

				int current_minor = (int)(((float)current_major / max_verts) * min_verts);
				int next_minor = (int)(((float)next_major / max_verts) * min_verts);

				int current_A = a_is_larger ? current_major : current_minor;
				int next_A = a_is_larger ? next_major : next_minor;
				int current_B = a_is_larger ? current_minor : current_major;
				int next_B = a_is_larger ? next_minor : next_major;

				int idx_A_curr = vertex_indices[i][current_A];
				int idx_A_next = vertex_indices[i][next_A];
				int idx_B_curr = vertex_indices[i + 1][current_B];
				int idx_B_next = vertex_indices[i + 1][next_B];

				Vector3 v_A_curr = mesh_data.vertices[idx_A_curr];
				Vector3 v_A_next = mesh_data.vertices[idx_A_next];
				Vector3 v_B_curr = mesh_data.vertices[idx_B_curr];

				// Build Primary Triangle (CCW Winding Order)
				Vector3 n_primary = (v_B_curr - v_A_curr).cross(v_A_next - v_A_curr);
				mesh_data.normals[idx_A_curr] += n_primary;
				mesh_data.normals[idx_A_next] += n_primary;
				mesh_data.normals[idx_B_curr] += n_primary;

				mesh_data.indices.push_back(idx_A_curr);
				mesh_data.indices.push_back(idx_A_next);
				mesh_data.indices.push_back(idx_B_curr);

				// Build Secondary Triangle if the minor ring advanced
				if (current_minor != next_minor) {
					Vector3 v_B_next = mesh_data.vertices[idx_B_next];
					Vector3 n_secondary = (v_B_curr - v_A_next).cross(v_B_next - v_A_next);
					mesh_data.normals[idx_A_next] += n_secondary;
					mesh_data.normals[idx_B_next] += n_secondary;
					mesh_data.normals[idx_B_curr] += n_secondary;

					mesh_data.indices.push_back(idx_A_next);
					mesh_data.indices.push_back(idx_B_next);
					mesh_data.indices.push_back(idx_B_curr);
				}
			}
		}

		// Normalize accumulated normals
		for (int k = 0; k < mesh_data.normals.size(); k++) {
			if (!mesh_data.normals[k].is_zero_approx()) {
				mesh_data.normals[k] = mesh_data.normals[k].normalized();
			} else {
				mesh_data.normals[k] = Vector3(0, 1, 0);
			}
		}
	}

	// Construct ArrayMesh
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = mesh_data.vertices;
	arrays[Mesh::ARRAY_NORMAL] = mesh_data.normals;
	arrays[Mesh::ARRAY_INDEX] = mesh_data.indices;

	Ref<ArrayMesh> final_mesh;
	final_mesh.instantiate();
	if (mesh_data.vertices.size() > 0 && mesh_data.indices.size() > 0) {
		final_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}

	return final_mesh;
}

void ProceduralLofter::_ready() {
	mesh_instance = Object::cast_to<MeshInstance3D>(get_node_or_null("MeshInstance3D"));
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("MeshInstance3D");
		add_child(mesh_instance);
		if (get_owner()) {
			mesh_instance->set_owner(get_owner());
		}
	}
	if (mesh_instance) {
		mesh_instance->set_material_override(material);
	}
	is_dirty = true;
}

void ProceduralLofter::_process(double delta) {
	Ref<Curve3D> current_curve = get_curve();
	if (current_curve != last_connected_curve) {
		if (last_connected_curve.is_valid()) {
			last_connected_curve->disconnect("changed", Callable(this, "update_loft"));
		}
		last_connected_curve = current_curve;
		if (last_connected_curve.is_valid()) {
			last_connected_curve->connect("changed", Callable(this, "update_loft"));
		}
		is_dirty = true;
	}

	bool slices_changed = false;
	if (slices_array.size() != last_connected_slices.size()) {
		slices_changed = true;
	} else {
		for (int i = 0; i < slices_array.size(); i++) {
			Ref<Curve3D> slice = slices_array[i];
			if (slice != last_connected_slices[i]) {
				slices_changed = true;
				break;
			}
		}
	}

	if (slices_changed) {
		for (auto &slice : last_connected_slices) {
			if (slice.is_valid()) {
				slice->disconnect("changed", Callable(this, "update_loft"));
			}
		}
		last_connected_slices.clear();
		for (int i = 0; i < slices_array.size(); i++) {
			Ref<Curve3D> slice = slices_array[i];
			last_connected_slices.push_back(slice);
			if (slice.is_valid()) {
				slice->connect("changed", Callable(this, "update_loft"));
			}
		}
		is_dirty = true;
	}

	if (Engine::get_singleton()->is_editor_hint() || is_dirty) {
		if (is_dirty) {
			update_loft();
			is_dirty = false;
		}
	}
}

void ProceduralLofter::update_loft() {
	if (!mesh_instance) {
		mesh_instance = Object::cast_to<MeshInstance3D>(get_node_or_null("MeshInstance3D"));
		if (!mesh_instance) {
			mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_name("MeshInstance3D");
			add_child(mesh_instance);
			if (get_owner()) {
				mesh_instance->set_owner(get_owner());
			}
		}
	}

	if (mesh_instance) {
		mesh_instance->set_material_override(material);

		TypedArray<Curve3D> processing_slices;
		TypedArray<Transform3D> processing_transforms;

		Ref<Curve3D> curve = get_curve();
		if (curve.is_null() || slices_array.size() == 0) {
			mesh_instance->set_mesh(Ref<Mesh>());
			return;
		}

		float total_length = curve->get_baked_length();
		if (total_length <= 0.0f) {
			mesh_instance->set_mesh(Ref<Mesh>());
			return;
		}

		int num_slices = slices_array.size();

		for (int i = 0; i < segments; i++) {
			float percent = (segments > 1) ? (float)i / (float)(segments - 1) : 0.0f;
			float distance = percent * total_length;

			Transform3D path_transform = curve->sample_baked_with_rotation(distance);
			processing_transforms.push_back(path_transform);

			Ref<Curve3D> generated_slice;
			generated_slice.instantiate();

			if (num_slices == 1) {
				Ref<Curve3D> base_slice = slices_array[0];
				if (base_slice.is_valid()) {
					int pt_count = base_slice->get_point_count();
					for (int j = 0; j < pt_count; j++) {
						generated_slice->add_point(base_slice->get_point_position(j));
					}
				}
			} else {
				float float_idx = percent * (num_slices - 1);
				int idx_low = (int)float_idx;
				int idx_high = idx_low + 1;
				if (idx_high >= num_slices) {
					idx_high = num_slices - 1;
				}
				Ref<Curve3D> slice_low = slices_array[idx_low];
				Ref<Curve3D> slice_high = slices_array[idx_high];
				if (slice_low.is_valid() && slice_high.is_valid()) {
					float t = float_idx - idx_low;
					int count_low = slice_low->get_point_count();
					int count_high = slice_high->get_point_count();
					if (count_low == count_high) {
						for (int j = 0; j < count_low; j++) {
							generated_slice->add_point(slice_low->get_point_position(j).lerp(slice_high->get_point_position(j), t));
						}
					} else {
						Ref<Curve3D> chosen = (t < 0.5f) ? slice_low : slice_high;
						int count_chosen = chosen->get_point_count();
						for (int j = 0; j < count_chosen; j++) {
							generated_slice->add_point(chosen->get_point_position(j));
						}
					}
				} else if (slice_low.is_valid()) {
					int pt_count = slice_low->get_point_count();
					for (int j = 0; j < pt_count; j++) {
						generated_slice->add_point(slice_low->get_point_position(j));
					}
				} else if (slice_high.is_valid()) {
					int pt_count = slice_high->get_point_count();
					for (int j = 0; j < pt_count; j++) {
						generated_slice->add_point(slice_high->get_point_position(j));
					}
				}
			}

			processing_slices.push_back(generated_slice);
		}

		Ref<ArrayMesh> mesh = generate_lofted_mesh(processing_slices, processing_transforms);
		mesh_instance->set_mesh(mesh);
	}
}

MeshInstance3D *ProceduralLofter::bake() const {
	const_cast<ProceduralLofter *>(this)->update_loft();
	return mesh_instance;
}

} // namespace godot
