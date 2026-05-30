#include "procedural_lofter.h"
#include "utils/curve/curve_baker.h"
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ProceduralLofter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_slice_resolution", "resolution"), &ProceduralLofter::set_slice_resolution);
	ClassDB::bind_method(D_METHOD("get_slice_resolution"), &ProceduralLofter::get_slice_resolution);
	ClassDB::bind_method(D_METHOD("set_slices_array", "slices"), &ProceduralLofter::set_slices_array);
	ClassDB::bind_method(D_METHOD("get_slices_array"), &ProceduralLofter::get_slices_array);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &ProceduralLofter::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &ProceduralLofter::get_material);
	ClassDB::bind_method(D_METHOD("set_flat_shaded", "flat_shaded"), &ProceduralLofter::set_flat_shaded);
	ClassDB::bind_method(D_METHOD("get_flat_shaded"), &ProceduralLofter::get_flat_shaded);
	ClassDB::bind_method(D_METHOD("add_slice", "slice", "position"), &ProceduralLofter::add_slice);
	ClassDB::bind_method(D_METHOD("set_blend_factor", "factor"), &ProceduralLofter::set_blend_factor);
	ClassDB::bind_method(D_METHOD("get_blend_factor"), &ProceduralLofter::get_blend_factor);

	// DEFORMATION BINDINGS
	ClassDB::bind_method(D_METHOD("set_scale_curve", "curve"), &ProceduralLofter::set_scale_curve);
	ClassDB::bind_method(D_METHOD("get_scale_curve"), &ProceduralLofter::get_scale_curve);
	ClassDB::bind_method(D_METHOD("set_wave_amplitude", "amplitude"), &ProceduralLofter::set_wave_amplitude);
	ClassDB::bind_method(D_METHOD("get_wave_amplitude"), &ProceduralLofter::get_wave_amplitude);
	ClassDB::bind_method(D_METHOD("set_wave_frequency", "frequency"), &ProceduralLofter::set_wave_frequency);
	ClassDB::bind_method(D_METHOD("get_wave_frequency"), &ProceduralLofter::get_wave_frequency);

	ClassDB::bind_method(D_METHOD("bake"), &ProceduralLofter::bake);
	ClassDB::bind_method(D_METHOD("update_loft"), &ProceduralLofter::update_loft);
	ClassDB::bind_method(D_METHOD("_on_parent_spline_changed"), &ProceduralLofter::_on_parent_spline_changed);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "slice_resolution"), "set_slice_resolution", "get_slice_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "slices_array", PROPERTY_HINT_ARRAY_TYPE, "Curve3D"), "set_slices_array", "get_slices_array");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat_shaded"), "set_flat_shaded", "get_flat_shaded");

	ADD_GROUP("Deformation", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scale_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_scale_curve", "get_scale_curve");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wave_amplitude"), "set_wave_amplitude", "get_wave_amplitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wave_frequency"), "set_wave_frequency", "get_wave_frequency");
}

ProceduralLofter::ProceduralLofter() {
	blend_factor = 0.0f;
	flat_shaded = false;
}

ProceduralLofter::~ProceduralLofter() {
	for (auto &slice : last_connected_slices) {
		if (slice.is_valid())
			slice->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
	}
	if (scale_curve.is_valid())
		scale_curve->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
	if (last_connected_curve.is_valid() && last_connected_curve->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
		last_connected_curve->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
	}
}

void ProceduralLofter::ensure_curve_connection() {
	Ref<Curve3D> current_curve = get_curve();
	if (current_curve != last_connected_curve) {
		if (last_connected_curve.is_valid() && last_connected_curve->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
			last_connected_curve->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
		}
		last_connected_curve = current_curve;
		if (current_curve.is_valid() && !current_curve->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
			current_curve->connect("changed", Callable(this, "_on_parent_spline_changed"));
		}
		mesh_dirty = true;
	}
}

void ProceduralLofter::_on_parent_spline_changed() {
	mesh_dirty = true;
	update_loft();
}

void ProceduralLofter::set_slices_array(const TypedArray<Curve3D> &p_slices) {
	slices_array = p_slices;
	mesh_dirty = true;
	update_loft();
}
TypedArray<Curve3D> ProceduralLofter::get_slices_array() const { return slices_array; }

void ProceduralLofter::set_material(const Ref<Material> &p_material) {
	material = p_material;
	if (mesh_instance)
		mesh_instance->set_material_override(material);
}
Ref<Material> ProceduralLofter::get_material() const { return material; }

void ProceduralLofter::set_flat_shaded(bool p_flat) {
	flat_shaded = p_flat;
	mesh_dirty = true;
	update_loft();
}
bool ProceduralLofter::get_flat_shaded() const { return flat_shaded; }

void ProceduralLofter::add_slice(Ref<Curve3D> p_slice, float p_position) {
	if (p_slice.is_valid()) {
		slices_array.push_back(p_slice);
		mesh_dirty = true;
		update_loft();
	}
}

void ProceduralLofter::set_blend_factor(float p_factor) {
	blend_factor = p_factor;
	mesh_dirty = true;
	update_loft();
}
float ProceduralLofter::get_blend_factor() const { return blend_factor; }

// --- DEFORMATION SETTERS ---
void ProceduralLofter::set_scale_curve(const Ref<Curve> &p_curve) {
	if (scale_curve.is_valid() && scale_curve->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
		scale_curve->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
	}
	scale_curve = p_curve;
	if (scale_curve.is_valid() && !scale_curve->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
		scale_curve->connect("changed", Callable(this, "_on_parent_spline_changed"));
	}
	mesh_dirty = true;
	update_loft();
}
Ref<Curve> ProceduralLofter::get_scale_curve() const { return scale_curve; }

void ProceduralLofter::set_wave_amplitude(float p_amp) {
	wave_amplitude = p_amp;
	mesh_dirty = true;
	update_loft();
}
float ProceduralLofter::get_wave_amplitude() const { return wave_amplitude; }

void ProceduralLofter::set_wave_frequency(float p_freq) {
	wave_frequency = p_freq;
	mesh_dirty = true;
	update_loft();
}
float ProceduralLofter::get_wave_frequency() const { return wave_frequency; }

Ref<ArrayMesh> ProceduralLofter::generate_lofted_mesh(const std::vector<PackedVector3Array> &rings, const std::vector<Transform3D> &transforms) const {
	int num_slices = rings.size();
	if (num_slices < 2 || transforms.size() < num_slices)
		return Ref<ArrayMesh>();

	bool is_closed = get_is_closed() && num_slices > 2;

	MeshData mesh_data;

	std::vector<PackedVector3Array> all_slice_vertices(num_slices);
	for (int i = 0; i < num_slices; i++) {
		Transform3D transform = transforms[i];
		int pt_count = rings[i].size();
		all_slice_vertices[i].resize(pt_count);

		for (int j = 0; j < pt_count; j++) {
			Vector3 p3d = rings[i][j];
			Vector3 local_3d = Vector3(p3d.x, p3d.z, 0.0);
			Vector3 world_3d = transform.xform(local_3d);
			all_slice_vertices[i][j] = world_3d;
		}
	}

	if (is_closed) {
		int verts_0 = all_slice_vertices[0].size();
		int verts_end = all_slice_vertices[num_slices - 1].size();
		int min_verts = MIN(verts_0, verts_end);
		for (int j = 0; j < min_verts; j++) {
			Vector3 avg = (all_slice_vertices[0][j] + all_slice_vertices[num_slices - 1][j]) * 0.5f;
			all_slice_vertices[0][j] = avg;
			all_slice_vertices[num_slices - 1][j] = avg;
		}
	}

	if (flat_shaded) {
		int current_vertex_index = 0;
		int loop_limit = num_slices - 1;
		for (int i = 0; i < loop_limit; i++) {
			int next_i = i + 1;
			int verts_A = all_slice_vertices[i].size();
			int verts_B = all_slice_vertices[next_i].size();

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
				Vector3 v_B_curr = all_slice_vertices[next_i][current_B];
				Vector3 v_B_next = all_slice_vertices[next_i][next_B];

				Vector3 n_primary = (v_B_curr - v_A_curr).cross(v_A_next - v_A_curr);
				if (!n_primary.is_zero_approx())
					n_primary = n_primary.normalized();
				else
					n_primary = Vector3(0, 1, 0);

				mesh_data.vertices.push_back(v_A_curr);
				mesh_data.vertices.push_back(v_A_next);
				mesh_data.vertices.push_back(v_B_curr);
				mesh_data.normals.push_back(n_primary);
				mesh_data.normals.push_back(n_primary);
				mesh_data.normals.push_back(n_primary);
				mesh_data.indices.push_back(current_vertex_index++);
				mesh_data.indices.push_back(current_vertex_index++);
				mesh_data.indices.push_back(current_vertex_index++);

				if (current_minor != next_minor) {
					Vector3 n_secondary = (v_B_curr - v_A_next).cross(v_B_next - v_A_next);
					if (!n_secondary.is_zero_approx())
						n_secondary = n_secondary.normalized();
					else
						n_secondary = Vector3(0, 1, 0);

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
		std::vector<std::vector<int>> vertex_indices(num_slices);
		int global_vertex_index = 0;
		for (int i = 0; i < num_slices; i++) {
			if (is_closed && i == num_slices - 1) {
				vertex_indices[i] = vertex_indices[0];
			} else {
				vertex_indices[i].resize(all_slice_vertices[i].size());
				for (int j = 0; j < all_slice_vertices[i].size(); j++) {
					mesh_data.vertices.push_back(all_slice_vertices[i][j]);
					mesh_data.normals.push_back(Vector3(0, 0, 0));
					vertex_indices[i][j] = global_vertex_index++;
				}
			}
		}

		int loop_limit = num_slices - 1;
		for (int i = 0; i < loop_limit; i++) {
			int next_i = i + 1;
			int verts_A = all_slice_vertices[i].size();
			int verts_B = all_slice_vertices[next_i].size();

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
				int idx_B_curr = vertex_indices[next_i][current_B];
				int idx_B_next = vertex_indices[next_i][next_B];

				Vector3 v_A_curr = mesh_data.vertices[idx_A_curr];
				Vector3 v_A_next = mesh_data.vertices[idx_A_next];
				Vector3 v_B_curr = mesh_data.vertices[idx_B_curr];

				Vector3 n_primary = (v_B_curr - v_A_curr).cross(v_A_next - v_A_curr);
				mesh_data.normals[idx_A_curr] += n_primary;
				mesh_data.normals[idx_A_next] += n_primary;
				mesh_data.normals[idx_B_curr] += n_primary;

				mesh_data.indices.push_back(idx_A_curr);
				mesh_data.indices.push_back(idx_A_next);
				mesh_data.indices.push_back(idx_B_curr);

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

		for (int k = 0; k < mesh_data.normals.size(); k++) {
			if (!mesh_data.normals[k].is_zero_approx())
				mesh_data.normals[k] = mesh_data.normals[k].normalized();
			else
				mesh_data.normals[k] = Vector3(0, 1, 0);
		}
	}

	// --- CAPS GENERATION (If NOT closed) ---
	if (!is_closed) {
		// Start Cap (slice 0)
		int M_start = all_slice_vertices[0].size();
		if (M_start >= 3) {
			Vector3 v0 = all_slice_vertices[0][0];
			Vector3 v1 = all_slice_vertices[0][1];
			Vector3 v2 = all_slice_vertices[0][2];
			Vector3 n_start = (v2 - v0).cross(v1 - v0);
			if (n_start.is_zero_approx()) {
				n_start = Vector3(0, 0, -1);
			} else {
				n_start = n_start.normalized();
			}

			int cap_start_idx = mesh_data.vertices.size();
			for (int j = 0; j < M_start; j++) {
				mesh_data.vertices.push_back(all_slice_vertices[0][j]);
				mesh_data.normals.push_back(n_start);
			}

			for (int j = 1; j < M_start - 1; j++) {
				mesh_data.indices.push_back(cap_start_idx);
				mesh_data.indices.push_back(cap_start_idx + j + 1);
				mesh_data.indices.push_back(cap_start_idx + j);
			}
		}

		// End Cap (slice num_slices - 1)
		int M_end = all_slice_vertices[num_slices - 1].size();
		if (M_end >= 3) {
			Vector3 v0 = all_slice_vertices[num_slices - 1][0];
			Vector3 v1 = all_slice_vertices[num_slices - 1][1];
			Vector3 v2 = all_slice_vertices[num_slices - 1][2];
			Vector3 n_end = (v1 - v0).cross(v2 - v0);
			if (n_end.is_zero_approx()) {
				n_end = Vector3(0, 0, 1);
			} else {
				n_end = n_end.normalized();
			}

			int cap_end_idx = mesh_data.vertices.size();
			for (int j = 0; j < M_end; j++) {
				mesh_data.vertices.push_back(all_slice_vertices[num_slices - 1][j]);
				mesh_data.normals.push_back(n_end);
			}

			for (int j = 1; j < M_end - 1; j++) {
				mesh_data.indices.push_back(cap_end_idx);
				mesh_data.indices.push_back(cap_end_idx + j);
				mesh_data.indices.push_back(cap_end_idx + j + 1);
			}
		}
	}

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
	ProceduralSpline3D::_ready();
	ensure_curve_connection();

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

	if (!is_connected("spline_changed", Callable(this, "_on_parent_spline_changed"))) {
		connect("spline_changed", Callable(this, "_on_parent_spline_changed"));
	}

	set_process(true);
	mesh_dirty = true;
}

void ProceduralLofter::_process(double delta) {
	ensure_curve_connection();
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
			if (slice.is_valid())
				slice->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
		}
		last_connected_slices.clear();
		for (int i = 0; i < slices_array.size(); i++) {
			Ref<Curve3D> slice = slices_array[i];
			last_connected_slices.push_back(slice);
			if (slice.is_valid())
				slice->connect("changed", Callable(this, "_on_parent_spline_changed"));
		}
		mesh_dirty = true;
	}

	if (Engine::get_singleton()->is_editor_hint() || mesh_dirty) {
		if (mesh_dirty) {
			update_loft();
			mesh_dirty = false;
		}
	}
}

void ProceduralLofter::update_loft() {
	ensure_curve_connection();
	if (!is_inside_tree()) {
		return;
	}

	if (!mesh_instance) {
		mesh_instance = Object::cast_to<MeshInstance3D>(get_node_or_null("MeshInstance3D"));
		if (!mesh_instance) {
			mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_name("MeshInstance3D");
			add_child(mesh_instance);
			if (get_owner())
				mesh_instance->set_owner(get_owner());
		}
	}

	if (mesh_instance) {
		mesh_instance->set_material_override(material);
		std::vector<Transform3D> transforms = CurveBaker::bake_transforms(get_curve(), get_bake_interval(), get_global_transform());

		if (transforms.empty() || slices_array.size() == 0) {
			mesh_instance->set_mesh(Ref<Mesh>());
			return;
		}

		std::vector<PackedVector3Array> all_rings;
		std::vector<Transform3D> processing_transforms;

		int num_slices = slices_array.size();
		int num_baked_points = transforms.size();

		Transform3D inv_global = get_global_transform().affine_inverse();
		bool is_closed = get_is_closed() && num_baked_points > 2;

		for (int i = 0; i < num_baked_points; i++) {
			Transform3D path_transform = inv_global * transforms[i];
			processing_transforms.push_back(path_transform);

			float percent = (num_baked_points > 1) ? (float)i / (float)(num_baked_points - 1) : 0.0f;

			float float_idx = 0.0f;
			int idx_low = 0;
			int idx_high = 0;
			float t = 0.0f;

			if (is_closed && num_slices > 1) {
				float_idx = percent * num_slices;
				idx_low = (int)float_idx % num_slices;
				idx_high = (idx_low + 1) % num_slices;
				t = float_idx - (int)float_idx;
			} else {
				float_idx = percent * (num_slices - 1);
				idx_low = (int)float_idx;
				idx_high = MIN(idx_low + 1, num_slices - 1);
				t = float_idx - idx_low;
			}

			Ref<Curve3D> slice_low = slices_array[idx_low];
			Ref<Curve3D> slice_high = slices_array[idx_high];

			PackedVector3Array current_ring = CurveBaker::bake_blended_ring(slice_low, slice_high, t, slice_resolution);

			// --- DEFORMATION MATH (BASED ON PERCENT 't') ---
			float current_scale = scale_curve.is_valid() ? scale_curve->sample_baked(percent) : 1.0f;
			float current_wave = Math::sin(percent * wave_frequency * Math_TAU) * wave_amplitude;

			for (int k = 0; k < current_ring.size(); ++k) {
				Vector3 pt = current_ring[k];

				// 1. SINE WAVE OFFSET (Pushes points outward/inward based on radius)
				if (current_wave != 0.0f) {
					Vector2 pt2d(pt.x, pt.z);
					if (pt2d.length_squared() > 0.0001f) {
						pt2d += pt2d.normalized() * current_wave;
						pt.x = pt2d.x;
						pt.z = pt2d.y;
					}
				}

				// 2. SCALE
				pt.x *= current_scale;
				pt.z *= current_scale;

				current_ring.set(k, pt);
			}

			all_rings.push_back(current_ring);
		}

		Ref<ArrayMesh> mesh = generate_lofted_mesh(all_rings, processing_transforms);
		mesh_instance->set_mesh(mesh);
	}
}

MeshInstance3D *ProceduralLofter::bake() const {
	const_cast<ProceduralLofter *>(this)->update_loft();
	return mesh_instance;
}

} // namespace godot
