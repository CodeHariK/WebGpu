#include "procedural_spline3d.h"
#include "utils/curve/curve_baker.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ProceduralSpline3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &ProceduralSpline3D::set_curve);
	ClassDB::bind_method(D_METHOD("get_is_closed"), &ProceduralSpline3D::get_is_closed);

	ClassDB::bind_method(D_METHOD("set_interpolation_mode", "mode"), &ProceduralSpline3D::set_interpolation_mode);
	ClassDB::bind_method(D_METHOD("get_interpolation_mode"), &ProceduralSpline3D::get_interpolation_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interpolation_mode", PROPERTY_HINT_ENUM, "Nearest,IDW Line,IDW Vertex"), "set_interpolation_mode", "get_interpolation_mode");

	ClassDB::bind_method(D_METHOD("set_bake_interval", "interval"), &ProceduralSpline3D::set_bake_interval);
	ClassDB::bind_method(D_METHOD("get_bake_interval"), &ProceduralSpline3D::get_bake_interval);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_interval"), "set_bake_interval", "get_bake_interval");

	ClassDB::bind_method(D_METHOD("get_padded_aabb"), &ProceduralSpline3D::get_padded_aabb);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &ProceduralSpline3D::_on_curve_changed);

	ADD_SIGNAL(MethodInfo("spline_changed"));

	BIND_ENUM_CONSTANT(INTERP_NEAREST);
	BIND_ENUM_CONSTANT(INTERP_IDW_LINE);
	BIND_ENUM_CONSTANT(INTERP_IDW_VERTEX);
}

ProceduralSpline3D::ProceduralSpline3D() {
	set_notify_transform(true);
	interpolation_mode = INTERP_IDW_LINE;
	bake_interval = 2.0f;
}

ProceduralSpline3D::~ProceduralSpline3D() {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_valid() && curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
}

void ProceduralSpline3D::_notification(int p_what) {
	if (p_what == Node3D::NOTIFICATION_TRANSFORM_CHANGED) {
		mark_dirty();
	} else if (p_what == Node::NOTIFICATION_READY) {
		_update_max_padding();
		_cache_curve_state();
		last_padded_aabb = get_padded_aabb();
	} else if (p_what == Node::NOTIFICATION_CHILD_ORDER_CHANGED) {
		// MUST check this so padding updates when a child is added/removed!
		_update_max_padding();
		mark_dirty();
	}
}

bool ProceduralSpline3D::get_is_closed() const {
	Ref<Curve3D> curve = get_curve();
	return curve.is_valid() && curve->is_closed();
}

void ProceduralSpline3D::set_interpolation_mode(InterpolationMode p_mode) {
	interpolation_mode = p_mode;
	mark_dirty();
}
ProceduralSpline3D::InterpolationMode ProceduralSpline3D::get_interpolation_mode() const { return interpolation_mode; }

void ProceduralSpline3D::set_bake_interval(float p_interval) {
	bake_interval = MAX(0.1f, p_interval);
	mark_dirty();
}
float ProceduralSpline3D::get_bake_interval() const { return bake_interval; }

void ProceduralSpline3D::set_curve(const Ref<Curve3D> &p_curve) {
	// 1. Disconnect from the OLD curve if it exists
	Ref<Curve3D> old_curve = get_curve();
	if (old_curve.is_valid() && old_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		old_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}

	// 2. Call the parent class's standard implementation to actually set it
	Path3D::set_curve(p_curve);

	// 3. Connect to the NEW curve
	if (p_curve.is_valid() && !p_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		p_curve->connect("changed", Callable(this, "_on_curve_changed"));
	}

	// 4. A completely new curve means the whole spline needs rebuilding
	mark_dirty();
}

void ProceduralSpline3D::_cache_curve_state() {
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

void ProceduralSpline3D::mark_dirty() {
	has_baked_cache = false;
	has_transform_cache = false;

	if (!is_dirty) {
		accumulated_dirty_rect = last_padded_aabb;
	} else if (last_padded_aabb.has_area()) {
		accumulated_dirty_rect = accumulated_dirty_rect.merge(last_padded_aabb);
	}
	is_full_rebuild = true;
	is_dirty = true;
	emit_signal("spline_changed");
}

void ProceduralSpline3D::_on_curve_changed() {
	Ref<Curve3D> curve = get_curve();
	if (curve.is_null()) {
#if DEBUG
		UtilityFunctions::print("[ProceduralSpline3D] Curve became null. Triggering full dirty.");
#endif
		mark_dirty();
		return;
	}

	int count = curve->get_point_count();
	if (count != cached_curve_state.size() || is_full_rebuild) {
#if DEBUG
		UtilityFunctions::print("[ProceduralSpline3D] Point count changed or full rebuild flagged. Triggering full dirty.");
#endif
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

		if (pos != cached_curve_state[i].pos || in != cached_curve_state[i].in || out != cached_curve_state[i].out) {
#if DEBUG
			UtilityFunctions::print("[ProceduralSpline3D] Point ID ", i, " moved! Calculating dirty rect...");
#endif
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
		has_baked_cache = false;
		local_changes = local_changes.grow(cached_max_padding);
#if DEBUG
		UtilityFunctions::print("[ProceduralSpline3D] Local Dirty Rect Extents: Pos(", local_changes.position.x, ", ", local_changes.position.y, ") Size(", local_changes.size.x, ", ", local_changes.size.y, ")");
#endif

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

Rect2 ProceduralSpline3D::consume_dirty_rect() {
	Rect2 current_bounds = get_padded_aabb();
	Rect2 result;

	if (is_full_rebuild) {
#if DEBUG
		UtilityFunctions::print("[ProceduralSpline3D] Consuming FULL REBUILD Dirty Rect.");
#endif
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
#if DEBUG
		UtilityFunctions::print("[ProceduralSpline3D] Consuming LOCAL Dirty Rect.");
#endif
		result = accumulated_dirty_rect;
	}

	last_padded_aabb = current_bounds;
	is_dirty = false;
	is_full_rebuild = false;
	accumulated_dirty_rect = Rect2();
	_cache_curve_state();

	return result;
}

PackedVector3Array ProceduralSpline3D::get_baked_points_3d() const {
	return CurveBaker::bake_curve(get_curve(), bake_interval, get_global_transform());
}

PackedVector2Array ProceduralSpline3D::get_baked_points_2d() const {
	PackedVector3Array p3d = get_baked_points_3d();
	PackedVector2Array p2d;
	int size = p3d.size();
	p2d.resize(size);
	for (int i = 0; i < size; ++i)
		p2d.set(i, Vector2(p3d[i].x, p3d[i].z));
	return p2d;
}

// 1. THE LOOP (Main Thread Only)
// Call this from _notification() when the node is ready or children change!
void ProceduralSpline3D::_update_max_padding() {
	float max_pad = 0.0f;
	TypedArray<Node> children = get_children();

	for (int i = 0; i < children.size(); ++i) {
		SplineComponent *comp = Object::cast_to<SplineComponent>(children[i]);
		if (comp) {
			max_pad = Math::max(max_pad, comp->get_spline_padding());
		}
	}

	// Save it to memory
	cached_max_padding = max_pad;
}

Rect2 ProceduralSpline3D::get_padded_aabb() const {
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
	return Rect2(min_pt, max_pt - min_pt).grow(cached_max_padding);
}

bool ProceduralSpline3D::is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const {
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

void ProceduralSpline3D::ensure_baked_cache() {
	if (has_baked_cache) {
		return;
	}

	baked_poly3d = get_baked_points_3d();
	baked_poly2d = get_baked_points_2d();

	baked_segments.clear();
	int limit = get_is_closed() ? baked_poly3d.size() : baked_poly3d.size() - 1;
	baked_segments.reserve(limit);

	for (int i = 0; i < limit; ++i) {
		Vector3 a = baked_poly3d[i];
		Vector3 b = baked_poly3d[(i + 1) % baked_poly3d.size()];

		BakedSegment seg;
		seg.a = Vector2(a.x, a.z);
		seg.b = Vector2(b.x, b.z);
		seg.ab = seg.b - seg.a;
		seg.l2 = seg.ab.length_squared();
		seg.y_start = a.y;
		seg.y_diff = b.y - a.y;
		seg.min_x = Math::min(seg.a.x, seg.b.x) - cached_max_padding;
		seg.max_x = Math::max(seg.a.x, seg.b.x) + cached_max_padding;
		seg.min_z = Math::min(seg.a.y, seg.b.y) - cached_max_padding;
		seg.max_z = Math::max(seg.a.y, seg.b.y) + cached_max_padding;

		baked_segments.push_back(seg);
	}

	has_baked_cache = true;
}

void ProceduralSpline3D::ensure_transform_cache() {
	if (has_transform_cache) {
		return;
	}

	baked_transforms = CurveBaker::bake_transforms(get_curve(), bake_interval, get_global_transform());
	has_transform_cache = true;
}

ProceduralSpline3D::SplineEval ProceduralSpline3D::evaluate_spline_point_segmented(const Vector2 &p, const std::vector<int> &p_segment_indices) const {
	const_cast<ProceduralSpline3D *>(this)->ensure_baked_cache();

	SplineEval res;
	res.distance = 1e20f;
	res.spline_y = 0.0f;
	res.is_inside = false;

	if (baked_poly3d.size() == 0)
		return res;
	if (get_is_closed())
		res.is_inside = is_point_inside(p, baked_poly2d);

	if (baked_poly3d.size() == 1) {
		res.distance = p.distance_to(Vector2(baked_poly3d[0].x, baked_poly3d[0].z));
		res.spline_y = baked_poly3d[0].y;
		return res;
	}

	float min_dist_sq = 1e20f;
	float closest_y = 0.0f;
	float total_weight_line = 0.0f;
	float blended_y_line = 0.0f;

	for (int seg_idx : p_segment_indices) {
		const BakedSegment &seg = baked_segments[seg_idx];
		if (p.x < seg.min_x || p.x > seg.max_x || p.y < seg.min_z || p.y > seg.max_z)
			continue;

		float t = (seg.l2 > 0.0f) ? Math::clamp((p - seg.a).dot(seg.ab) / seg.l2, 0.0f, 1.0f) : 0.0f;
		Vector2 proj = seg.a + t * seg.ab;

		float dist_sq = p.distance_squared_to(proj);
		float seg_y = seg.y_start + t * seg.y_diff;

		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest_y = seg_y;
		}

		if (interpolation_mode == INTERP_IDW_LINE) {
			float weight = 1.0f / (dist_sq + 0.001f);
			blended_y_line += seg_y * weight;
			total_weight_line += weight;
		}
	}

	res.distance = Math::sqrt(min_dist_sq);

	if (interpolation_mode == INTERP_NEAREST) {
		res.spline_y = closest_y;
	} else if (interpolation_mode == INTERP_IDW_LINE) {
		res.spline_y = (total_weight_line > 0.0f) ? (blended_y_line / total_weight_line) : closest_y;
	} else if (interpolation_mode == INTERP_IDW_VERTEX) {
		float total_weight_vert = 0.0f;
		float blended_y_vert = 0.0f;

		float search_radius = MAX(cached_max_padding, bake_interval * 2.0f);

		for (int i = 0; i < baked_poly3d.size(); ++i) {
			Vector2 pt_2d(baked_poly3d[i].x, baked_poly3d[i].z);

			if (Math::abs(p.x - pt_2d.x) > search_radius ||
				Math::abs(p.y - pt_2d.y) > search_radius) {
				continue;
			}

			float dist_sq = p.distance_squared_to(pt_2d);
			float weight = 1.0f / (dist_sq + 0.001f);
			blended_y_vert += baked_poly3d[i].y * weight;
			total_weight_vert += weight;
		}
		res.spline_y = (total_weight_vert > 0.0f) ? (blended_y_vert / total_weight_vert) : closest_y;
	}

	return res;
}

} //namespace godot
