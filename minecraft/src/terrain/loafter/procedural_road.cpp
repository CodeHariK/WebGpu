#include "procedural_road.h"
#include "utils/curve/curve_baker.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void ProceduralRoad::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cross_section", "cross_section"), &ProceduralRoad::set_cross_section);
	ClassDB::bind_method(D_METHOD("get_cross_section"), &ProceduralRoad::get_cross_section);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &ProceduralRoad::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &ProceduralRoad::get_material);
	ClassDB::bind_method(D_METHOD("set_custom_padding", "padding"), &ProceduralRoad::set_custom_padding);
	ClassDB::bind_method(D_METHOD("get_custom_padding"), &ProceduralRoad::get_custom_padding);

	ClassDB::bind_method(D_METHOD("set_use_adaptive", "use_adaptive"), &ProceduralRoad::set_use_adaptive);
	ClassDB::bind_method(D_METHOD("get_use_adaptive"), &ProceduralRoad::get_use_adaptive);
	ClassDB::bind_method(D_METHOD("set_chunk_start_distance", "chunk_start_distance"), &ProceduralRoad::set_chunk_start_distance);
	ClassDB::bind_method(D_METHOD("get_chunk_start_distance"), &ProceduralRoad::get_chunk_start_distance);
	ClassDB::bind_method(D_METHOD("set_chunk_end_distance", "chunk_end_distance"), &ProceduralRoad::set_chunk_end_distance);
	ClassDB::bind_method(D_METHOD("get_chunk_end_distance"), &ProceduralRoad::get_chunk_end_distance);
	ClassDB::bind_method(D_METHOD("set_adaptive_max_step", "max_step"), &ProceduralRoad::set_adaptive_max_step);
	ClassDB::bind_method(D_METHOD("get_adaptive_max_step"), &ProceduralRoad::get_adaptive_max_step);
	ClassDB::bind_method(D_METHOD("set_adaptive_min_step", "min_step"), &ProceduralRoad::set_adaptive_min_step);
	ClassDB::bind_method(D_METHOD("get_adaptive_min_step"), &ProceduralRoad::get_adaptive_min_step);
	ClassDB::bind_method(D_METHOD("set_adaptive_angle_tol", "angle_tol"), &ProceduralRoad::set_adaptive_angle_tol);
	ClassDB::bind_method(D_METHOD("get_adaptive_angle_tol"), &ProceduralRoad::get_adaptive_angle_tol);
	ClassDB::bind_method(D_METHOD("set_texture_length", "texture_length"), &ProceduralRoad::set_texture_length);
	ClassDB::bind_method(D_METHOD("get_texture_length"), &ProceduralRoad::get_texture_length);

	ClassDB::bind_method(D_METHOD("_on_parent_spline_changed"), &ProceduralRoad::_on_parent_spline_changed);
	ClassDB::bind_method(D_METHOD("_update_mesh"), &ProceduralRoad::_update_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "cross_section", PROPERTY_HINT_RESOURCE_TYPE, "Curve2D"), "set_cross_section", "get_cross_section");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "custom_padding"), "set_custom_padding", "get_custom_padding");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "texture_length"), "set_texture_length", "get_texture_length");

	ADD_GROUP("Adaptive & Chunking", "chunk_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_adaptive"), "set_use_adaptive", "get_use_adaptive");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chunk_start_distance"), "set_chunk_start_distance", "get_chunk_start_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chunk_end_distance"), "set_chunk_end_distance", "get_chunk_end_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adaptive_max_step"), "set_adaptive_max_step", "get_adaptive_max_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adaptive_min_step"), "set_adaptive_min_step", "get_adaptive_min_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adaptive_angle_tol"), "set_adaptive_angle_tol", "get_adaptive_angle_tol");
}

ProceduralRoad::ProceduralRoad() {}

void ProceduralRoad::_ready() {
	SplineComponent::_ready();

	parent_spline = Object::cast_to<ProceduralSpline3D>(get_parent());

	mesh_instance = Object::cast_to<MeshInstance3D>(get_node_or_null("MeshInstance3D"));
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("MeshInstance3D");
		add_child(mesh_instance);
		if (get_owner())
			mesh_instance->set_owner(get_owner());
	}
	if (mesh_instance)
		mesh_instance->set_material_override(material);

	if (parent_spline && !parent_spline->is_connected("spline_changed", Callable(this, "_on_parent_spline_changed"))) {
		parent_spline->connect("spline_changed", Callable(this, "_on_parent_spline_changed"));
	}

	mesh_dirty = false;
	queue_update();
}

void ProceduralRoad::queue_update() {
	if (!mesh_dirty) {
		mesh_dirty = true;
		call_deferred("_update_mesh");
	}
}

void ProceduralRoad::_on_parent_spline_changed() { queue_update(); }

void ProceduralRoad::set_cross_section(const Ref<Curve2D> &p_profile) {
	if (cross_section.is_valid() && cross_section->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
		cross_section->disconnect("changed", Callable(this, "_on_parent_spline_changed"));
	}
	cross_section = p_profile;
	if (cross_section.is_valid() && !cross_section->is_connected("changed", Callable(this, "_on_parent_spline_changed"))) {
		cross_section->connect("changed", Callable(this, "_on_parent_spline_changed"));
	}
	queue_update();
}

void ProceduralRoad::set_material(const Ref<Material> &p_material) {
	material = p_material;
	if (mesh_instance)
		mesh_instance->set_material_override(material);
}

void ProceduralRoad::set_custom_padding(float p_pad) {
	custom_padding = MAX(0.0f, p_pad);
	if (parent_spline)
		parent_spline->mark_dirty();
}


// Bakes the transforms along the path curve based on adaptive or standard chunk settings.
std::vector<Transform3D> ProceduralRoad::_bake_road_transforms(ProceduralSpline3D *p_spline, const Ref<Curve3D> &p_curve, float p_start, float p_end) {
	std::vector<Transform3D> transforms;
	if (use_adaptive) {
		transforms = CurveBaker::bake_transforms_adaptive(
				p_curve,
				p_start,
				p_end,
				adaptive_max_step,
				adaptive_min_step,
				adaptive_angle_tol,
				p_spline->get_global_transform());
	} else {
		if (chunk_end_distance > chunk_start_distance) {
			float interval = p_spline->get_bake_interval();
			if (interval <= 0.0f) {
				interval = 2.0f;
			}
			Transform3D gt = p_spline->get_global_transform();
			for (float d = p_start; d < p_end; d += interval) {
				transforms.push_back(gt * p_curve->sample_baked_with_rotation(d));
			}
			transforms.push_back(gt * p_curve->sample_baked_with_rotation(p_end));
		} else {
			p_spline->ensure_transform_cache();
			transforms = p_spline->baked_transforms;
		}
	}
	return transforms;
}

// Computes U coordinates based on the 2D cross-section points.
std::vector<float> ProceduralRoad::_calculate_profile_u(const PackedVector2Array &p_profile, float &r_total_length) {
	int count = p_profile.size();
	std::vector<float> profile_u(count, 0.0f);
	r_total_length = 0.0f;
	std::vector<float> profile_distances(count, 0.0f);

	for (int j = 1; j < count; j++) {
		r_total_length += p_profile[j].distance_to(p_profile[j - 1]);
		profile_distances[j] = r_total_length;
	}
	for (int j = 0; j < count; j++) {
		profile_u[j] = (r_total_length > 0.0f) ? (profile_distances[j] / r_total_length) : 0.0f;
	}
	return profile_u;
}

// Projects the 2D road profile onto 3D space using baked transforms.
std::vector<PackedVector3Array> ProceduralRoad::_project_road_rings(const std::vector<Transform3D> &p_transforms, const PackedVector2Array &p_profile, const Transform3D &p_inv_global) {
	int num_transforms = p_transforms.size();
	int verts_per_ring = p_profile.size();
	std::vector<PackedVector3Array> rings(num_transforms);

	for (int i = 0; i < num_transforms; i++) {
		Transform3D local_t = p_inv_global * p_transforms[i];
		rings[i].resize(verts_per_ring);

		for (int j = 0; j < verts_per_ring; j++) {
			Vector3 local_pt(p_profile[j].x, p_profile[j].y, 0.0f);
			rings[i].set(j, local_t.xform(local_pt));
		}
	}
	return rings;
}

// Generates vertices, normals, UVs, and indices by stitching adjacent rings together.
void ProceduralRoad::_stitch_road_mesh(
		const std::vector<Transform3D> &p_transforms,
		const std::vector<PackedVector3Array> &p_rings,
		const std::vector<float> &p_profile_u,
		float p_start_v,
		float p_total_profile_length,
		bool p_is_closed,
		float p_texture_length,
		PackedVector3Array &r_vertices,
		PackedVector3Array &r_normals,
		PackedVector2Array &r_uvs,
		PackedInt32Array &r_indices) {
	int num_transforms = p_transforms.size();
	int verts_per_ring = p_profile_u.size();
	int current_vertex = r_vertices.size();
	int limit = p_is_closed ? num_transforms : num_transforms - 1;

	float current_v = p_start_v;
	float texture_scale = MAX(0.01f, p_texture_length); // How many meters before the texture repeats

	for (int i = 0; i < limit; i++) {
		int next_i = (i + 1) % num_transforms;
		float step_dist = p_transforms[i].origin.distance_to(p_transforms[next_i].origin);
		float next_v = current_v + step_dist;

		for (int j = 0; j < verts_per_ring - 1; j++) {
			Vector3 v0 = p_rings[i][j];
			Vector3 v1 = p_rings[i][j + 1];
			Vector3 v2 = p_rings[next_i][j];
			Vector3 v3 = p_rings[next_i][j + 1];

			float u0 = p_profile_u[j];
			float u1 = p_profile_u[j + 1];
			float vA = current_v / texture_scale;
			float vB = next_v / texture_scale;

			// Primary Triangle
			Vector3 n1 = (v2 - v0).cross(v1 - v0).normalized();
			r_vertices.push_back(v0); r_normals.push_back(n1); r_uvs.push_back(Vector2(u0, vA)); r_indices.push_back(current_vertex++);
			r_vertices.push_back(v1); r_normals.push_back(n1); r_uvs.push_back(Vector2(u1, vA)); r_indices.push_back(current_vertex++);
			r_vertices.push_back(v2); r_normals.push_back(n1); r_uvs.push_back(Vector2(u0, vB)); r_indices.push_back(current_vertex++);

			// Secondary Triangle
			Vector3 n2 = (v2 - v1).cross(v3 - v1).normalized();
			r_vertices.push_back(v1); r_normals.push_back(n2); r_uvs.push_back(Vector2(u1, vA)); r_indices.push_back(current_vertex++);
			r_vertices.push_back(v3); r_normals.push_back(n2); r_uvs.push_back(Vector2(u1, vB)); r_indices.push_back(current_vertex++);
			r_vertices.push_back(v2); r_normals.push_back(n2); r_uvs.push_back(Vector2(u0, vB)); r_indices.push_back(current_vertex++);
		}
		current_v = next_v;
	}
}

// Updates the road mesh by calling helper functions to generate vertices, normals, and UVs.
void ProceduralRoad::_update_mesh() {
	if (!is_inside_tree() || is_queued_for_deletion()) {
		return;
	}

	ProceduralSpline3D *master_spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!master_spline || !mesh_instance) {
		return;
	}

	mesh_instance->set_material_override(material);

	if (cross_section.is_null() || cross_section->get_point_count() < 2) {
		mesh_instance->set_mesh(Ref<Mesh>());
		return;
	}

	Ref<Curve3D> path_curve = master_spline->get_curve();
	if (path_curve.is_null()) {
		mesh_instance->set_mesh(Ref<Mesh>());
		return;
	}

	float start_dist = chunk_start_distance;
	float end_dist = chunk_end_distance;
	if (end_dist <= start_dist) {
		end_dist = path_curve->get_baked_length();
	}

	std::vector<Transform3D> transforms = _bake_road_transforms(master_spline, path_curve, start_dist, end_dist);
	if (transforms.size() < 2) {
		mesh_instance->set_mesh(Ref<Mesh>());
		return;
	}

	PackedVector2Array baked_profile = cross_section->get_baked_points();
	if (baked_profile.size() < 2) {
		return;
	}

	float total_profile_length = 0.0f;
	std::vector<float> profile_u = _calculate_profile_u(baked_profile, total_profile_length);

	Transform3D inv_global = master_spline->get_global_transform().affine_inverse();
	std::vector<PackedVector3Array> rings = _project_road_rings(transforms, baked_profile, inv_global);

	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	bool is_closed = master_spline->get_is_closed() && transforms.size() > 2 && (chunk_end_distance <= chunk_start_distance);

	_stitch_road_mesh(transforms, rings, profile_u, start_dist, total_profile_length, is_closed, texture_length, vertices, normals, uvs, indices);

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> final_mesh;
	final_mesh.instantiate();
	final_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	mesh_instance->set_mesh(final_mesh);
	mesh_dirty = false;
}

} // namespace godot
