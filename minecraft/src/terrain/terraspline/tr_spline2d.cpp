#include "terraspline.h"
#include <cmath>
#include <godot_cpp/classes/time.hpp> // NEW: For high-precision profiling
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static inline float local_lerp(float a, float b, float t) {
	return a + t * (b - a);
}

// =========================================================
// TerrainHeightmap Implementation
// =========================================================

void TerrainHeightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "width", "height", "default_value"), &TerrainHeightmap::initialize, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("clear", "default_value"), &TerrainHeightmap::clear, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_width"), &TerrainHeightmap::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &TerrainHeightmap::get_height);
	ClassDB::bind_method(D_METHOD("get_image"), &TerrainHeightmap::get_image);
}

TerrainHeightmap::TerrainHeightmap() {}
TerrainHeightmap::~TerrainHeightmap() {}

void TerrainHeightmap::initialize(int p_width, int p_height, float p_default_value) {
	width = p_width;
	height = p_height;
	data.resize(width * height);
	clear(p_default_value);
}

void TerrainHeightmap::clear(float p_default_value) {
	int sz = width * height;
	float *ptr = data.ptrw();
	for (int i = 0; i < sz; ++i) {
		ptr[i] = p_default_value;
	}
}

Ref<Image> TerrainHeightmap::get_image() const {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();

	Ref<Image> img = Image::create_empty(width, height, false, Image::FORMAT_RF);
	if (width > 0 && height > 0) {
		PackedByteArray byte_data;
		byte_data.resize(data.size() * sizeof(float));
		memcpy(byte_data.ptrw(), data.ptr(), byte_data.size());
		img->set_data(width, height, false, Image::FORMAT_RF, byte_data);
	}

	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("    [TerrainHeightmap] get_image() created in: ", (t_end - t_start) / 1000.0, " ms.");
	return img;
}

// =========================================================
// TerrainSpline2D Implementation
// =========================================================

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

	ClassDB::bind_method(D_METHOD("set_interpolation_mode", "mode"), &TerrainSpline2D::set_interpolation_mode);
	ClassDB::bind_method(D_METHOD("get_interpolation_mode"), &TerrainSpline2D::get_interpolation_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interpolation_mode", PROPERTY_HINT_ENUM, "Nearest,IDW Line,IDW Vertex"), "set_interpolation_mode", "get_interpolation_mode");

	ClassDB::bind_method(D_METHOD("set_falloff_curve", "falloff_curve"), &TerrainSpline2D::set_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_falloff_curve"), &TerrainSpline2D::get_falloff_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_falloff_curve", "get_falloff_curve");

	ClassDB::bind_method(D_METHOD("set_use_tile_culling", "use"), &TerrainSpline2D::set_use_tile_culling);
	ClassDB::bind_method(D_METHOD("get_use_tile_culling"), &TerrainSpline2D::get_use_tile_culling);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_tile_culling"), "set_use_tile_culling", "get_use_tile_culling");

	ClassDB::bind_method(D_METHOD("set_tile_size", "size"), &TerrainSpline2D::set_tile_size);
	ClassDB::bind_method(D_METHOD("get_tile_size"), &TerrainSpline2D::get_tile_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_size"), "set_tile_size", "get_tile_size");

	ClassDB::bind_method(D_METHOD("set_bake_interval", "interval"), &TerrainSpline2D::set_bake_interval);
	ClassDB::bind_method(D_METHOD("get_bake_interval"), &TerrainSpline2D::get_bake_interval);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_interval"), "set_bake_interval", "get_bake_interval");

	ClassDB::bind_method(D_METHOD("get_padded_aabb"), &TerrainSpline2D::get_padded_aabb);
	ClassDB::bind_method(D_METHOD("deform_heightmap", "heightmap", "offset"), &TerrainSpline2D::deform_heightmap);
	ClassDB::bind_method(D_METHOD("_deform_heightmap_task", "task_idx"), &TerrainSpline2D::_deform_heightmap_task);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &TerrainSpline2D::_on_curve_changed);

	ADD_SIGNAL(MethodInfo("spline_changed"));

	BIND_ENUM_CONSTANT(BLEND_ADD);
	BIND_ENUM_CONSTANT(BLEND_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_MAX);
	BIND_ENUM_CONSTANT(BLEND_MIN);
	BIND_ENUM_CONSTANT(BLEND_REPLACE);
	BIND_ENUM_CONSTANT(INTERP_NEAREST);
	BIND_ENUM_CONSTANT(INTERP_IDW_LINE);
	BIND_ENUM_CONSTANT(INTERP_IDW_VERTEX);
}

TerrainSpline2D::TerrainSpline2D() {
	set_notify_transform(true);
	max_height = 0.0f;
	spline_width = 2.0f;
	falloff_distance = 5.0f;
	is_closed = true;
	blend_mode = BLEND_ADD;
	interpolation_mode = INTERP_IDW_LINE;
	use_tile_culling = true;
	tile_size = 32;
	bake_interval = 2.0f;
}

TerrainSpline2D::~TerrainSpline2D() {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_valid() && curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
}

void TerrainSpline2D::_notification(int p_what) {
	if (p_what == Node3D::NOTIFICATION_TRANSFORM_CHANGED) {
		UtilityFunctions::print("[TerrainSpline2D] Transform changed, marking fully dirty.");
		mark_dirty();
	} else if (p_what == Node::NOTIFICATION_READY) {
		_cache_curve_state();
		last_padded_aabb = get_padded_aabb();
	}
}

void TerrainSpline2D::set_max_height(float p_height) {
	max_height = p_height;
	mark_dirty();
}
float TerrainSpline2D::get_max_height() const { return max_height; }
void TerrainSpline2D::set_spline_width(float p_width) {
	spline_width = MAX(0.0f, p_width);
	mark_dirty();
}
float TerrainSpline2D::get_spline_width() const { return spline_width; }
void TerrainSpline2D::set_interpolation_mode(InterpolationMode p_mode) {
	interpolation_mode = p_mode;
	mark_dirty();
}
TerrainSpline2D::InterpolationMode TerrainSpline2D::get_interpolation_mode() const { return interpolation_mode; }
void TerrainSpline2D::set_falloff_distance(float p_dist) {
	falloff_distance = MAX(0.0f, p_dist);
	mark_dirty();
}
float TerrainSpline2D::get_falloff_distance() const { return falloff_distance; }
void TerrainSpline2D::set_is_closed(bool p_closed) {
	is_closed = p_closed;
	mark_dirty();
}
bool TerrainSpline2D::get_is_closed() const { return is_closed; }
void TerrainSpline2D::set_blend_mode(BlendMode p_mode) {
	blend_mode = p_mode;
	mark_dirty();
}
TerrainSpline2D::BlendMode TerrainSpline2D::get_blend_mode() const { return blend_mode; }
void TerrainSpline2D::set_falloff_curve(const Ref<Curve> &p_curve) {
	falloff_curve = p_curve;
	mark_dirty();
}
Ref<Curve> TerrainSpline2D::get_falloff_curve() const { return falloff_curve; }
void TerrainSpline2D::set_use_tile_culling(bool p_use) {
	use_tile_culling = p_use;
	mark_dirty();
}
bool TerrainSpline2D::get_use_tile_culling() const { return use_tile_culling; }
void TerrainSpline2D::set_tile_size(int p_size) {
	tile_size = MAX(8, p_size);
	mark_dirty();
}
int TerrainSpline2D::get_tile_size() const { return tile_size; }
void TerrainSpline2D::set_bake_interval(float p_interval) {
	bake_interval = MAX(0.1f, p_interval);
	mark_dirty();
}
float TerrainSpline2D::get_bake_interval() const { return bake_interval; }

void TerrainSpline2D::_cache_curve_state() {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_null())
		return;
	int count = curve->get_point_count();
	cached_curve_state.resize(count);
	for (int i = 0; i < count; ++i) {
		cached_curve_state[i].pos = curve->get_point_position(i);
		cached_curve_state[i].in = curve->get_point_in(i);
		cached_curve_state[i].out = curve->get_point_out(i);
	}
}

void TerrainSpline2D::mark_dirty() {
	if (!is_dirty) {
		accumulated_dirty_rect = last_padded_aabb;
	} else if (last_padded_aabb.has_area()) {
		accumulated_dirty_rect = accumulated_dirty_rect.merge(last_padded_aabb);
	}
	is_full_rebuild = true;
	is_dirty = true;
	emit_signal("spline_changed");
}

void TerrainSpline2D::_on_curve_changed() {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_null()) {
		UtilityFunctions::print("[TerrainSpline2D] Curve became null. Triggering full dirty.");
		mark_dirty();
		return;
	}

	int count = curve->get_point_count();
	if (count != cached_curve_state.size() || is_full_rebuild) {
		UtilityFunctions::print("[TerrainSpline2D] Point count changed or full rebuild flagged. Triggering full dirty.");
		mark_dirty();
		return;
	}

	Transform3D gt = get_global_transform();
	Rect2 local_changes;
	bool has_local_changes = false;

	for (int i = 0; i < count; ++i) {
		Vector3 pos = curve->get_point_position(i);
		Vector3 in = curve->get_point_in(i);
		Vector3 out = curve->get_point_out(i);

		// If this specific control point was moved by the level designer
		if (pos != cached_curve_state[i].pos || in != cached_curve_state[i].in || out != cached_curve_state[i].out) {
			UtilityFunctions::print("[TerrainSpline2D] Point ID ", i, " moved! Calculating dirty rect...");
			int start_idx = MAX(0, i - 1);
			int end_idx = MIN(count - 1, i + 1);

			for (int j = start_idx; j <= end_idx; ++j) {
				Vector3 old_g_pos = gt.xform(cached_curve_state[j].pos);
				Vector2 old_p2(old_g_pos.x, old_g_pos.z);
				Vector3 new_g_pos = gt.xform(curve->get_point_position(j));
				Vector2 new_p2(new_g_pos.x, new_g_pos.z);

				if (!has_local_changes) {
					local_changes = Rect2(old_p2, Vector2()).expand(new_p2);
					has_local_changes = true;
				} else {
					local_changes = local_changes.expand(old_p2).expand(new_p2);
				}
			}
			cached_curve_state[i].pos = pos;
			cached_curve_state[i].in = in;
			cached_curve_state[i].out = out;
		}
	}

	if (has_local_changes) {
		local_changes = local_changes.grow(spline_width + falloff_distance);
		UtilityFunctions::print("[TerrainSpline2D] Local Dirty Rect Extents: Pos(", local_changes.position.x, ", ", local_changes.position.y, ") Size(", local_changes.size.x, ", ", local_changes.size.y, ")");

		if (!is_dirty) {
			accumulated_dirty_rect = local_changes;
		} else if (accumulated_dirty_rect.has_area()) {
			accumulated_dirty_rect = accumulated_dirty_rect.merge(local_changes);
		} else {
			accumulated_dirty_rect = local_changes;
		}
		is_dirty = true;
		emit_signal("spline_changed");
	}
}

Rect2 TerrainSpline2D::consume_dirty_rect() {
	Rect2 current_bounds = get_padded_aabb();
	Rect2 result;

	if (is_full_rebuild) {
		UtilityFunctions::print("[TerrainSpline2D] Consuming FULL REBUILD Dirty Rect.");
		if (is_dirty && accumulated_dirty_rect.has_area()) {
			result = accumulated_dirty_rect.merge(current_bounds);
		} else {
			if (last_padded_aabb.has_area()) {
				result = last_padded_aabb.merge(current_bounds);
			} else {
				result = current_bounds;
			}
		}
	} else {
		UtilityFunctions::print("[TerrainSpline2D] Consuming LOCAL Dirty Rect.");
		result = accumulated_dirty_rect;
	}

	last_padded_aabb = current_bounds;
	is_dirty = false;
	is_full_rebuild = false;
	accumulated_dirty_rect = Rect2();
	_cache_curve_state();

	return result;
}

PackedVector3Array TerrainSpline2D::get_baked_points_3d() const {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_null())
		return PackedVector3Array();

	TerrainSpline2D *mutable_this = const_cast<TerrainSpline2D *>(this);
	if (!curve->is_connected("changed", Callable(mutable_this, "_on_curve_changed"))) {
		curve->connect("changed", Callable(mutable_this, "_on_curve_changed"));
	}

	PackedVector3Array global_pts;
	Transform3D gt = get_global_transform();

	float total_len = curve->get_baked_length();
	for (float d = 0.0f; d < total_len; d += bake_interval) {
		global_pts.push_back(gt.xform(curve->sample_baked(d)));
	}
	global_pts.push_back(gt.xform(curve->sample_baked(total_len)));
	return global_pts;
}

PackedVector2Array TerrainSpline2D::get_baked_points_2d() const {
	PackedVector3Array p3d = get_baked_points_3d();
	PackedVector2Array p2d;
	int size = p3d.size();
	p2d.resize(size);
	for (int i = 0; i < size; ++i)
		p2d.set(i, Vector2(p3d[i].x, p3d[i].z));
	return p2d;
}

Rect2 TerrainSpline2D::get_padded_aabb() const {
	PackedVector3Array points_3d = get_baked_points_3d();
	if (points_3d.size() == 0)
		return Rect2();

	Vector2 min_pt(points_3d[0].x, points_3d[0].z);
	Vector2 max_pt(points_3d[0].x, points_3d[0].z);

	for (int i = 1; i < points_3d.size(); ++i) {
		Vector2 p(points_3d[i].x, points_3d[i].z);
		if (p.x < min_pt.x)
			min_pt.x = p.x;
		if (p.y < min_pt.y)
			min_pt.y = p.y;
		if (p.x > max_pt.x)
			max_pt.x = p.x;
		if (p.y > max_pt.y)
			max_pt.y = p.y;
	}
	return Rect2(min_pt, max_pt - min_pt).grow(spline_width + falloff_distance);
}

bool TerrainSpline2D::is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const {
	int num_pts = polygon.size();
	if (num_pts < 3)
		return false;
	bool inside = false;
	for (int i = 0, j = num_pts - 1; i < num_pts; j = i++) {
		Vector2 vi = polygon[i];
		Vector2 vj = polygon[j];
		if (((vi.y > p.y) != (vj.y > p.y)) && (p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x)) {
			inside = !inside;
		}
	}
	return inside;
}

TerrainSpline2D::SplineEval TerrainSpline2D::_evaluate_spline_point(const Vector2 &p) const {
	SplineEval res;
	res.distance = 1e20f;
	res.spline_y = 0.0f;
	res.is_inside = false;

	if (thread_poly3d.size() == 0)
		return res;
	if (is_closed)
		res.is_inside = is_point_inside(p, thread_poly2d);

	if (thread_poly3d.size() == 1) {
		res.distance = p.distance_to(Vector2(thread_poly3d[0].x, thread_poly3d[0].z));
		res.spline_y = thread_poly3d[0].y;
		return res;
	}

	float min_dist = 1e20f;
	float closest_y = 0.0f;
	float total_weight_line = 0.0f;
	float blended_y_line = 0.0f;

	for (const BakedSegment &seg : thread_segments) {
		if (p.x < seg.min_x || p.x > seg.max_x || p.y < seg.min_z || p.y > seg.max_z)
			continue;

		float t = (seg.l2 > 0.0f) ? Math::clamp((p - seg.a).dot(seg.ab) / seg.l2, 0.0f, 1.0f) : 0.0f;
		Vector2 proj = seg.a + t * seg.ab;
		float dist = p.distance_to(proj);
		float seg_y = seg.y_start + t * seg.y_diff;

		if (dist < min_dist) {
			min_dist = dist;
			closest_y = seg_y;
		}

		if (interpolation_mode == INTERP_IDW_LINE) {
			float weight = 1.0f / (dist * dist + 0.001f);
			blended_y_line += seg_y * weight;
			total_weight_line += weight;
		}
	}

	res.distance = min_dist;

	if (interpolation_mode == INTERP_NEAREST) {
		res.spline_y = closest_y;
	} else if (interpolation_mode == INTERP_IDW_LINE) {
		res.spline_y = (total_weight_line > 0.0f) ? (blended_y_line / total_weight_line) : closest_y;
	} else if (interpolation_mode == INTERP_IDW_VERTEX) {
		float total_weight_vert = 0.0f;
		float blended_y_vert = 0.0f;
		for (int i = 0; i < thread_poly3d.size(); ++i) {
			Vector2 pt_2d(thread_poly3d[i].x, thread_poly3d[i].z);
			float dist = p.distance_to(pt_2d);
			float weight = 1.0f / (dist * dist + 0.001f);
			blended_y_vert += thread_poly3d[i].y * weight;
			total_weight_vert += weight;
		}
		res.spline_y = (total_weight_vert > 0.0f) ? (blended_y_vert / total_weight_vert) : closest_y;
	}

	return res;
}

void TerrainSpline2D::_deform_heightmap_task(int p_task_idx) {
	if (thread_heightmap.is_null() || thread_data_ptr == nullptr)
		return;

	Rect2i tile = thread_active_tiles[p_task_idx];
	int w = thread_heightmap->get_width();

	for (int z = tile.position.y; z <= tile.position.y + tile.size.y - 1; ++z) {
		for (int x = tile.position.x; x <= tile.position.x + tile.size.x - 1; ++x) {
			Vector2 p((float)x + thread_offset.x, (float)z + thread_offset.y);
			SplineEval eval = _evaluate_spline_point(p);

			float target_spline_h = eval.spline_y + max_height;
			float weight = 0.0f;

			if (eval.is_inside || eval.distance <= spline_width) {
				weight = 1.0f;
			} else if (eval.distance < (spline_width + falloff_distance) && falloff_distance > 0.0001f) {
				float t = (eval.distance - spline_width) / falloff_distance;
				if (thread_has_curve) {
					int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
					weight = thread_baked_curve[c_idx];
				} else {
					weight = 1.0f - t;
				}
			}

			if (weight <= 0.0f)
				continue;

			int idx = z * w + x;
			float current_h = thread_data_ptr[idx];
			float new_h = current_h;

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
			thread_data_ptr[idx] = new_h;
		}
	}
}

void TerrainSpline2D::deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, const Vector2 &p_offset) {
	uint64_t step1_start = Time::get_singleton()->get_ticks_usec();

	if (p_heightmap.is_null())
		return;

	Rect2 aabb = get_padded_aabb();
	int w = p_heightmap->get_width();
	int h = p_heightmap->get_height();
	Rect2 chunk_rect(p_offset, Vector2(w, h));

	if (!aabb.intersects(chunk_rect))
		return;

	thread_min_x = Math::max(0, (int)Math::floor(aabb.position.x - p_offset.x));
	thread_max_x = Math::min(w - 1, (int)Math::ceil(aabb.position.x + aabb.size.x - p_offset.x));
	thread_min_z = Math::max(0, (int)Math::floor(aabb.position.y - p_offset.y));
	thread_max_z = Math::min(h - 1, (int)Math::ceil(aabb.position.y + aabb.size.y - p_offset.y));

	thread_heightmap = p_heightmap;
	thread_offset = p_offset;
	thread_data_ptr = p_heightmap->get_data_ptrw();

	thread_poly3d = get_baked_points_3d();
	thread_poly2d = get_baked_points_2d();

	UtilityFunctions::print("    [TerrainSpline2D] Pre-baked Points: ", (int)thread_poly3d.size());

	// PRECOMPUTE SEGMENTS (The ultimate speed optimization)
	thread_segments.clear();
	int limit = is_closed ? thread_poly3d.size() : thread_poly3d.size() - 1;
	float search_radius = spline_width + falloff_distance;

	for (int i = 0; i < limit; ++i) {
		Vector3 a = thread_poly3d[i];
		Vector3 b = thread_poly3d[(i + 1) % thread_poly3d.size()];

		BakedSegment seg;
		seg.a = Vector2(a.x, a.z);
		seg.b = Vector2(b.x, b.z);
		seg.ab = seg.b - seg.a;
		seg.l2 = seg.ab.length_squared();
		seg.y_start = a.y;
		seg.y_diff = b.y - a.y;
		seg.min_x = Math::min(seg.a.x, seg.b.x) - search_radius;
		seg.max_x = Math::max(seg.a.x, seg.b.x) + search_radius;
		seg.min_z = Math::min(seg.a.y, seg.b.y) - search_radius;
		seg.max_z = Math::max(seg.a.y, seg.b.y) + search_radius;

		thread_segments.push_back(seg);
	}

	thread_has_curve = falloff_curve.is_valid();
	if (thread_has_curve) {
		thread_baked_curve.resize(256);
		for (int i = 0; i < 256; ++i) {
			thread_baked_curve[i] = falloff_curve->sample((float)i / 255.0f);
		}
	}

	thread_active_tiles.clear();
	uint64_t step2_culling = Time::get_singleton()->get_ticks_usec();

	if (use_tile_culling && tile_size > 0) {
		float tile_radius = (tile_size * 1.41421356f) / 2.0f;

		for (int tz = thread_min_z; tz <= thread_max_z; tz += tile_size) {
			for (int tx = thread_min_x; tx <= thread_max_x; tx += tile_size) {
				int t_max_x = Math::min(tx + tile_size - 1, thread_max_x);
				int t_max_z = Math::min(tz + tile_size - 1, thread_max_z);

				Vector2 center((tx + t_max_x) / 2.0f + p_offset.x, (tz + t_max_z) / 2.0f + p_offset.y);

				float min_dist = 1e20f;
				for (const BakedSegment &seg : thread_segments) {
					float t = (seg.l2 > 0.0f) ? Math::clamp((center - seg.a).dot(seg.ab) / seg.l2, 0.0f, 1.0f) : 0.0f;
					Vector2 proj = seg.a + t * seg.ab;
					float d = center.distance_to(proj);
					if (d < min_dist)
						min_dist = d;
				}

				if (min_dist <= (search_radius + tile_radius)) {
					thread_active_tiles.push_back(Rect2i(tx, tz, t_max_x - tx + 1, t_max_z - tz + 1));
				}
			}
		}
	} else {
		for (int tz = thread_min_z; tz <= thread_max_z; ++tz) {
			thread_active_tiles.push_back(Rect2i(thread_min_x, tz, thread_max_x - thread_min_x + 1, 1));
		}
	}

	int num_tasks = thread_active_tiles.size();
	uint64_t step3_threads = Time::get_singleton()->get_ticks_usec();

	if (num_tasks > 0) {
		WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
		if (wtp) {
			int group_id = wtp->add_group_task(Callable(this, "_deform_heightmap_task"), num_tasks, -1, true, "TerraSpline_Unified_Deform");
			wtp->wait_for_group_task_completion(group_id);
		} else {
			for (int r = 0; r < num_tasks; ++r)
				_deform_heightmap_task(r);
		}
	}

	uint64_t step4_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("    [TerrainSpline2D] Precompute: ", (step2_culling - step1_start) / 1000.0, "ms | Culling: ", (step3_threads - step2_culling) / 1000.0, "ms | Math Threads: ", (step4_end - step3_threads) / 1000.0, "ms");

	thread_heightmap.unref();
	thread_data_ptr = nullptr;
	thread_poly3d.clear();
	thread_poly2d.clear();
	thread_segments.clear();
	thread_baked_curve.clear();
	thread_active_tiles.clear();
}
} //namespace godot
