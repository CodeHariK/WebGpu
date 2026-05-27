#include "terraspline.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

namespace godot {

static inline float local_lerp(float a, float b, float t) {
	return a + t * (b - a);
}

void TerrainSpline2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_height", "max_height"), &TerrainSpline2D::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &TerrainSpline2D::get_max_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height"), "set_max_height", "get_max_height");

	ClassDB::bind_method(D_METHOD("set_spline_width", "spline_width"), &TerrainSpline2D::set_spline_width);
	ClassDB::bind_method(D_METHOD("get_spline_width"), &TerrainSpline2D::get_spline_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spline_width"), "set_spline_width", "get_spline_width");

	ClassDB::bind_method(D_METHOD("set_falloff_distance", "falloff_distance"), &TerrainSpline2D::set_falloff_distance);
	ClassDB::bind_method(D_METHOD("get_falloff_distance"), &TerrainSpline2D::get_falloff_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "falloff_distance"), "set_falloff_distance", "get_falloff_distance");

	ClassDB::bind_method(D_METHOD("set_is_closed", "is_closed"), &TerrainSpline2D::set_is_closed);
	ClassDB::bind_method(D_METHOD("get_is_closed"), &TerrainSpline2D::get_is_closed);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_closed"), "set_is_closed", "get_is_closed");

	ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &TerrainSpline2D::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &TerrainSpline2D::get_blend_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Add,Subtract,Max,Min,Replace"), "set_blend_mode", "get_blend_mode");

	ClassDB::bind_method(D_METHOD("set_falloff_curve", "falloff_curve"), &TerrainSpline2D::set_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_falloff_curve"), &TerrainSpline2D::get_falloff_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_falloff_curve", "get_falloff_curve");

	ClassDB::bind_method(D_METHOD("get_baked_points_3d"), &TerrainSpline2D::get_baked_points_3d);
	ClassDB::bind_method(D_METHOD("get_baked_points_2d"), &TerrainSpline2D::get_baked_points_2d);
	ClassDB::bind_method(D_METHOD("get_padded_aabb"), &TerrainSpline2D::get_padded_aabb);
	ClassDB::bind_method(D_METHOD("is_point_inside", "point", "polygon"), &TerrainSpline2D::is_point_inside);
	ClassDB::bind_method(D_METHOD("distance_to_spline", "point", "polygon"), &TerrainSpline2D::distance_to_spline);
	ClassDB::bind_method(D_METHOD("get_target_height", "x", "z", "current_h"), &TerrainSpline2D::get_target_height);
	ClassDB::bind_method(D_METHOD("deform_heightmap", "heightmap", "offset"), &TerrainSpline2D::deform_heightmap, DEFVAL(Vector2(0, 0)));
	ClassDB::bind_method(D_METHOD("_deform_heightmap_row", "row_idx"), &TerrainSpline2D::_deform_heightmap_row);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &TerrainSpline2D::_on_curve_changed);
	ClassDB::bind_method(D_METHOD("generate_splatmap", "heightmap"), &TerrainSpline2D::generate_splatmap);
	ClassDB::bind_method(D_METHOD("generate_scatter_transforms", "heightmap", "count", "min_slope", "max_slope"), &TerrainSpline2D::generate_scatter_transforms, DEFVAL(0.0f), DEFVAL(90.0f));

	ADD_SIGNAL(MethodInfo("spline_changed"));

	BIND_ENUM_CONSTANT(BLEND_ADD);
	BIND_ENUM_CONSTANT(BLEND_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_MAX);
	BIND_ENUM_CONSTANT(BLEND_MIN);
	BIND_ENUM_CONSTANT(BLEND_REPLACE);
}

TerrainSpline2D::TerrainSpline2D() {
	max_height = 0.0f; // Default offset is 0, rely on 3D curve height
	spline_width = 2.0f;
	falloff_distance = 5.0f;
	is_closed = true;
	blend_mode = BLEND_ADD;
	is_dirty = true;
}

TerrainSpline2D::~TerrainSpline2D() {
	if (last_connected_curve.is_valid() && last_connected_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		last_connected_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
}

void TerrainSpline2D::set_max_height(float p_height) {
	max_height = p_height;
	mark_dirty();
}

float TerrainSpline2D::get_max_height() const {
	return max_height;
}

void TerrainSpline2D::set_spline_width(float p_width) {
	spline_width = MAX(0.0f, p_width);
	mark_dirty();
}

float TerrainSpline2D::get_spline_width() const {
	return spline_width;
}

void TerrainSpline2D::set_falloff_distance(float p_dist) {
	falloff_distance = MAX(0.0f, p_dist);
	mark_dirty();
}

float TerrainSpline2D::get_falloff_distance() const {
	return falloff_distance;
}

void TerrainSpline2D::set_is_closed(bool p_closed) {
	is_closed = p_closed;
	mark_dirty();
}

bool TerrainSpline2D::get_is_closed() const {
	return is_closed;
}

void TerrainSpline2D::set_blend_mode(BlendMode p_mode) {
	blend_mode = p_mode;
	mark_dirty();
}

TerrainSpline2D::BlendMode TerrainSpline2D::get_blend_mode() const {
	return blend_mode;
}

void TerrainSpline2D::set_falloff_curve(const Ref<Curve> &p_curve) {
	falloff_curve = p_curve;
	mark_dirty();
}

Ref<Curve> TerrainSpline2D::get_falloff_curve() const {
	return falloff_curve;
}

PackedVector3Array TerrainSpline2D::get_baked_points_3d() const {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_null()) {
		return PackedVector3Array();
	}

	TerrainSpline2D *mutable_this = const_cast<TerrainSpline2D *>(this);
	if (!curve->is_connected("changed", Callable(mutable_this, "_on_curve_changed"))) {
		curve->connect("changed", Callable(mutable_this, "_on_curve_changed"));
	}

	return curve->get_baked_points();
}

PackedVector2Array TerrainSpline2D::get_baked_points_2d() const {
	PackedVector3Array p3d = get_baked_points_3d();
	PackedVector2Array p2d;
	int size = p3d.size();
	p2d.resize(size);
	for (int i = 0; i < size; ++i) {
		p2d.set(i, Vector2(p3d[i].x, p3d[i].z));
	}
	return p2d;
}

Rect2 TerrainSpline2D::get_padded_aabb() const {
	PackedVector3Array points_3d = get_baked_points_3d();
	int size = points_3d.size();
	if (size == 0) {
		return Rect2();
	}

	Vector2 min_pt(points_3d[0].x, points_3d[0].z);
	Vector2 max_pt(points_3d[0].x, points_3d[0].z);

	for (int i = 1; i < size; ++i) {
		Vector2 p(points_3d[i].x, points_3d[i].z);
		if (p.x < min_pt.x) min_pt.x = p.x;
		if (p.y < min_pt.y) min_pt.y = p.y;
		if (p.x > max_pt.x) max_pt.x = p.x;
		if (p.y > max_pt.y) max_pt.y = p.y;
	}

	Rect2 rect(min_pt, max_pt - min_pt);
	return rect.grow(spline_width + falloff_distance);
}

void TerrainSpline2D::mark_dirty() {
	is_dirty = true;
	emit_signal("spline_changed");
}

void TerrainSpline2D::_on_curve_changed() {
	mark_dirty();
}

bool TerrainSpline2D::is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const {
	int num_pts = polygon.size();
	if (num_pts < 3) return false;
	
	bool inside = false;
	for (int i = 0, j = num_pts - 1; i < num_pts; j = i++) {
		Vector2 vi = polygon[i];
		Vector2 vj = polygon[j];

		bool intersect = ((vi.y > p.y) != (vj.y > p.y)) &&
				(p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x);
		if (intersect) {
			inside = !inside;
		}
	}
	return inside;
}

float TerrainSpline2D::get_distance_to_segment(const Vector2 &p, const Vector2 &a, const Vector2 &b) const {
	Vector2 ab = b - a;
	float l2 = ab.length_squared();
	if (l2 == 0.0f) return p.distance_to(a);
	
	float t = Math::clamp((p - a).dot(ab) / l2, 0.0f, 1.0f);
	Vector2 projection = a + t * ab;
	return p.distance_to(projection);
}

float TerrainSpline2D::distance_to_spline(const Vector2 &p, const PackedVector2Array &polygon) const {
	int num_pts = polygon.size();
	if (num_pts == 0) return 0.0f;
	if (num_pts == 1) return p.distance_to(polygon[0]);

	float min_dist = 1e20f;
	int limit = is_closed ? num_pts : num_pts - 1;
	for (int i = 0; i < limit; ++i) {
		Vector2 a = polygon[i];
		Vector2 b = polygon[(i + 1) % num_pts];
		float dist = get_distance_to_segment(p, a, b);
		if (dist < min_dist) {
			min_dist = dist;
		}
	}
	return min_dist;
}

// Internal 3D Math Evaluator
TerrainSpline2D::SplineEval TerrainSpline2D::_evaluate_spline_point(const Vector2 &p, const PackedVector3Array &poly3d, const PackedVector2Array &poly2d) const {
	SplineEval res;
	res.distance = 1e20f;
	res.spline_y = 0.0f;
	res.is_inside = false;

	int num_pts = poly3d.size();
	if (num_pts == 0) return res;

	if (is_closed) {
		res.is_inside = is_point_inside(p, poly2d);
	}

	if (num_pts == 1) {
		res.distance = p.distance_to(Vector2(poly3d[0].x, poly3d[0].z));
		res.spline_y = poly3d[0].y;
		return res;
	}

	int limit = is_closed ? num_pts : num_pts - 1;
	for (int i = 0; i < limit; ++i) {
		Vector3 a = poly3d[i];
		Vector3 b = poly3d[(i + 1) % num_pts];

		Vector2 a2(a.x, a.z);
		Vector2 b2(b.x, b.z);

		Vector2 ab = b2 - a2;
		float l2 = ab.length_squared();
		float t = 0.0f;
		if (l2 > 0.0f) {
			t = Math::clamp((p - a2).dot(ab) / l2, 0.0f, 1.0f);
		}

		Vector2 proj = a2 + t * ab;
		float dist = p.distance_to(proj);

		if (dist < res.distance) {
			res.distance = dist;
			// Interpolate the exact Y height of the 3D spline curve at this projection
			res.spline_y = a.y + t * (b.y - a.y); 
		}
	}
	return res;
}

float TerrainSpline2D::get_target_height(float x, float z, float current_h) const {
	PackedVector3Array poly3d = get_baked_points_3d();
	PackedVector2Array poly2d = get_baked_points_2d();

	SplineEval eval = _evaluate_spline_point(Vector2(x, z), poly3d, poly2d);
	
	// max_height now acts as an additional offset, allowing users to lift the whole plateau
	float target_spline_h = eval.spline_y + max_height; 
	float weight = 0.0f;

	if (eval.is_inside || eval.distance <= spline_width) {
		weight = 1.0f;
	} else if (eval.distance < (spline_width + falloff_distance) && falloff_distance > 0.0001f) {
		float t = (eval.distance - spline_width) / falloff_distance;
		weight = falloff_curve.is_valid() ? falloff_curve->sample(1.0f - t) : 1.0f - t;
	}

	if (weight <= 0.0f) return current_h;

	switch (blend_mode) {
		case BLEND_ADD: return current_h + (target_spline_h * weight);
		case BLEND_SUBTRACT: return current_h - (target_spline_h * weight);
		case BLEND_MAX: return Math::max(current_h, (float)local_lerp(current_h, target_spline_h, weight));
		case BLEND_MIN: return Math::min(current_h, (float)local_lerp(current_h, target_spline_h, weight));
		case BLEND_REPLACE: return local_lerp(current_h, target_spline_h, weight);
	}
	return current_h;
}

void TerrainSpline2D::deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, const Vector2 &p_offset) {
	if (p_heightmap.is_null()) return;
	Rect2 aabb = get_padded_aabb();

	float chunk_x = p_offset.x;
	float chunk_z = p_offset.y;
	int w = p_heightmap->get_width();
	int h = p_heightmap->get_height();

	Rect2 chunk_rect(Vector2(chunk_x, chunk_z), Vector2(w, h));
	if (!aabb.intersects(chunk_rect)) return;

	int min_x = Math::max(0, (int)Math::floor(aabb.position.x - chunk_x));
	int max_x = Math::min(w - 1, (int)Math::ceil(aabb.position.x + aabb.size.x - chunk_x));
	int min_z = Math::max(0, (int)Math::floor(aabb.position.y - chunk_z));
	int max_z = Math::min(h - 1, (int)Math::ceil(aabb.position.y + aabb.size.y - chunk_z));

	int num_rows = max_z - min_z + 1;
	if (num_rows <= 0) return;

	// Cache the arrays so we aren't allocating memory on 10,000 voxels inside the thread pool
	temp_polygon_3d = get_baked_points_3d();
	temp_polygon_2d = get_baked_points_2d();

	temp_has_curve = falloff_curve.is_valid();
	if (temp_has_curve) {
		temp_baked_curve.resize(256);
		for (int i = 0; i < 256; ++i) {
			float sample_pos = (float)i / 255.0f;
			temp_baked_curve[i] = falloff_curve->sample(sample_pos);
		}
	} else {
		temp_baked_curve.clear();
	}

	temp_heightmap = p_heightmap;
	temp_data_ptr = temp_heightmap->get_data_ptrw();
	temp_min_x = min_x;
	temp_max_x = max_x;
	temp_min_z = min_z;
	temp_max_z = max_z;
	temp_offset = p_offset;

	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	if (wtp) {
		int group_id = wtp->add_group_task(
				Callable(this, "_deform_heightmap_row"),
				num_rows, -1, true, "TerraSpline Deformation");
		wtp->wait_for_group_task_completion(group_id);
	} else {
		for (int r = 0; r < num_rows; ++r) _deform_heightmap_row(r);
	}

	// Clean up caches
	temp_heightmap.unref();
	temp_data_ptr = nullptr;
	temp_baked_curve.clear();
	temp_polygon_3d.clear();
	temp_polygon_2d.clear();
	temp_has_curve = false;
	temp_offset = Vector2(0, 0);
}

void TerrainSpline2D::_process(double delta) {
	Ref<Curve3D> current_curve = get_curve();
	if (current_curve != last_connected_curve) {
		if (last_connected_curve.is_valid() && last_connected_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
			last_connected_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
		}
		last_connected_curve = current_curve;
		if (last_connected_curve.is_valid() && !last_connected_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
			last_connected_curve->connect("changed", Callable(this, "_on_curve_changed"));
		}
		mark_dirty();
	}
}

void TerrainSpline2D::_deform_heightmap_row(int p_row_idx) {
	if (temp_heightmap.is_null() || temp_data_ptr == nullptr) return;
	int z = temp_min_z + p_row_idx;
	if (z < 0 || z >= temp_heightmap->get_height()) return;

	int map_w = temp_heightmap->get_width();

	for (int x = temp_min_x; x <= temp_max_x; ++x) {
		Vector2 p((float)x + temp_offset.x, (float)z + temp_offset.y);
		SplineEval eval = _evaluate_spline_point(p, temp_polygon_3d, temp_polygon_2d);

		float target_spline_h = eval.spline_y + max_height;
		float weight = 0.0f;

		if (eval.is_inside || eval.distance <= spline_width) {
			weight = 1.0f;
		} else if (eval.distance < (spline_width + falloff_distance) && falloff_distance > 0.0001f) {
			float t = (eval.distance - spline_width) / falloff_distance;
			if (temp_has_curve) {
				int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
				weight = temp_baked_curve[c_idx];
			} else if (falloff_curve.is_valid()) {
				weight = falloff_curve->sample(1.0f - t);
			} else {
				weight = 1.0f - t;
			}
		}

		// If the point is outside the spline's influence entirely, do nothing
		if (weight <= 0.0f && blend_mode != BLEND_MIN) continue;

		int idx = z * map_w + x;
		float current_h = temp_data_ptr[idx];
		float new_h = current_h;

		// The Smooth LERP fix ensures exact ground tracking without math artifacts
		switch (blend_mode) {
			case BLEND_ADD:
				new_h = current_h + (target_spline_h * weight);
				break;
			case BLEND_SUBTRACT:
				new_h = current_h - (target_spline_h * weight);
				break;
			case BLEND_MAX:
				new_h = Math::max(current_h, (float)local_lerp(current_h, target_spline_h, weight));
				break;
			case BLEND_MIN:
				new_h = Math::min(current_h, (float)local_lerp(current_h, target_spline_h, weight));
				break;
			case BLEND_REPLACE:
				new_h = local_lerp(current_h, target_spline_h, weight);
				break;
		}

		temp_data_ptr[idx] = new_h;
	}
}

Ref<Image> TerrainSpline2D::generate_splatmap(const Ref<TerrainHeightmap> &p_heightmap) const {
	Ref<Image> splatmap;
	splatmap.instantiate();
	if (p_heightmap.is_null()) {
		return splatmap;
	}
	int w = p_heightmap->get_width();
	int h = p_heightmap->get_height();
	if (w <= 0 || h <= 0) {
		return splatmap;
	}

	splatmap->create_empty(w, h, false, Image::FORMAT_RGBA8);
	PackedVector2Array polygon = get_baked_points_2d();

	for (int z = 0; z < h; ++z) {
		for (int x = 0; x < w; ++x) {
			Vector2 grid_p((float)x, (float)z);
			float r = 0.0f;
			float g = 0.0f;

			// Inside check
			if (is_closed && is_point_inside(grid_p, polygon)) {
				r = 1.0f;
			}

			// Steepness check based on normal estimation
			float hl = p_heightmap->get_height_at(x - 1, z);
			float hr = p_heightmap->get_height_at(x + 1, z);
			float hd = p_heightmap->get_height_at(x, z - 1);
			float hu = p_heightmap->get_height_at(x, z + 1);
			Vector3 normal(hl - hr, 2.0f, hd - hu);
			normal = normal.normalized();

			// Steepness ranges from 0 (flat) to 1 (vertical)
			float steepness = 1.0f - normal.y;
			g = Math::clamp(steepness, 0.0f, 1.0f);

			splatmap->set_pixel(x, z, Color(r, g, 0.0f, 1.0f));
		}
	}

	return splatmap;
}

TypedArray<Transform3D> TerrainSpline2D::generate_scatter_transforms(const Ref<TerrainHeightmap> &p_heightmap, int p_count, float p_min_slope, float p_max_slope) const {
	TypedArray<Transform3D> transforms;
	if (p_heightmap.is_null() || p_count <= 0) {
		return transforms;
	}

	PackedVector2Array polygon = get_baked_points_2d();
	int num_pts = polygon.size();
	if (num_pts < 3) {
		return transforms;
	}

	// Calculate 2D bounding box
	Vector2 min_pt = polygon[0];
	Vector2 max_pt = polygon[0];
	for (int i = 1; i < num_pts; ++i) {
		Vector2 p = polygon[i];
		if (p.x < min_pt.x)
			min_pt.x = p.x;
		if (p.y < min_pt.y)
			min_pt.y = p.y;
		if (p.x > max_pt.x)
			max_pt.x = p.x;
		if (p.y > max_pt.y)
			max_pt.y = p.y;
	}

	float width_range = max_pt.x - min_pt.x;
	float height_range = max_pt.y - min_pt.y;
	if (width_range <= 0.001f || height_range <= 0.001f) {
		return transforms;
	}

	float min_cos = Math::cos(Math::deg_to_rad(p_max_slope));
	float max_cos = Math::cos(Math::deg_to_rad(p_min_slope));

	// Seed LCG for deterministic results
	uint32_t seed = 12345;
	auto rand_float = [&seed]() -> float {
		seed = seed * 1664525 + 1013904223;
		return (float)(seed & 0xFFFFFF) / (float)0xFFFFFF;
	};

	int attempts = 0;
	int spawned = 0;
	int max_attempts = p_count * 20;

	while (spawned < p_count && attempts < max_attempts) {
		attempts++;
		float rx = min_pt.x + rand_float() * width_range;
		float rz = min_pt.y + rand_float() * height_range;
		Vector2 p(rx, rz);

		if (is_closed && !is_point_inside(p, polygon)) {
			continue;
		}

		int grid_x = (int)Math::round(rx);
		int grid_z = (int)Math::round(rz);

		if (grid_x < 0 || grid_x >= p_heightmap->get_width() || grid_z < 0 || grid_z >= p_heightmap->get_height()) {
			continue;
		}

		float hl = p_heightmap->get_height_at(grid_x - 1, grid_z);
		float hr = p_heightmap->get_height_at(grid_x + 1, grid_z);
		float hd = p_heightmap->get_height_at(grid_x, grid_z - 1);
		float hu = p_heightmap->get_height_at(grid_x, grid_z + 1);
		Vector3 normal(hl - hr, 2.0f, hd - hu);
		normal = normal.normalized();

		if (normal.y < min_cos || normal.y > max_cos) {
			continue;
		}

		float ry = p_heightmap->get_height_at(grid_x, grid_z);

		float random_rot_y = rand_float() * Math_PI * 2.0f;
		Basis basis(Vector3(0, 1, 0), random_rot_y);
		Transform3D t(basis, Vector3(rx, ry, rz));

		transforms.push_back(t);
		spawned++;
	}

	return transforms;
}

} // namespace godot
