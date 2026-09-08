/**
 * @file spline_rocks.cpp
 * @brief SplineRocks: variants, placement along the spline, MultiMeshes and colliders.
 */
#include "spline_rocks.h"
#include "prop_geometry.h"
#include "prop_material.h"
#include "terrain/terraspline/tr_compositor.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define SR_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &SplineRocks::set_##m_name);                               \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &SplineRocks::get_##m_name);                                        \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void SplineRocks::_bind_methods() {
	ADD_GROUP("Placement", "");
	SR_BIND(FLOAT, spacing, PROPERTY_HINT_RANGE, "0.1,100,0.1,suffix:m");
	SR_BIND(FLOAT, spacing_jitter, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	SR_BIND(FLOAT, start_offset, PROPERTY_HINT_RANGE, "0,1000,0.5,suffix:m");
	SR_BIND(FLOAT, section_start, PROPERTY_HINT_RANGE, "0,10000,1,suffix:m");
	SR_BIND(FLOAT, section_end, PROPERTY_HINT_RANGE, "0,10000,1,suffix:m");
	SR_BIND(INT, rows, PROPERTY_HINT_RANGE, "1,6,1");
	SR_BIND(FLOAT, row_offset, PROPERTY_HINT_RANGE, "0,20,0.1,suffix:m");
	SR_BIND(FLOAT, lateral_offset, PROPERTY_HINT_RANGE, "-100,100,0.1,suffix:m");
	SR_BIND(FLOAT, lateral_jitter, PROPERTY_HINT_RANGE, "0,20,0.1,suffix:m");
	SR_BIND(FLOAT, vertical_offset, PROPERTY_HINT_RANGE, "-20,20,0.05,suffix:m");
	SR_BIND(BOOL, snap_to_ground, PROPERTY_HINT_NONE, "");
	SR_BIND(BOOL, align_to_tangent, PROPERTY_HINT_NONE, "");
	SR_BIND(INT, seed, PROPERTY_HINT_NONE, "");
	ADD_GROUP("Rock", "");
	SR_BIND(INT, variants, PROPERTY_HINT_RANGE, "1,12,1");
	SR_BIND(VECTOR3, size, PROPERTY_HINT_NONE, "suffix:m");
	SR_BIND(FLOAT, size_variation, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	SR_BIND(INT, detail, PROPERTY_HINT_RANGE, "0,3,1");
	ClassDB::bind_method(D_METHOD("set_base_shape", "value"), &SplineRocks::set_base_shape);
	ClassDB::bind_method(D_METHOD("get_base_shape"), &SplineRocks::get_base_shape);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "base_shape", PROPERTY_HINT_ENUM, "Sphere,Cube"), "set_base_shape", "get_base_shape");
	SR_BIND(FLOAT, roundness, PROPERTY_HINT_RANGE, "0,1,0.01");
	SR_BIND(FLOAT, tilt, PROPERTY_HINT_RANGE, "0,60,0.5,suffix:°");
	SR_BIND(FLOAT, noise_amplitude, PROPERTY_HINT_RANGE, "0,1,0.01");
	SR_BIND(BOOL, ridged, PROPERTY_HINT_NONE, "");
	SR_BIND(FLOAT, facet_snap, PROPERTY_HINT_RANGE, "0,1,0.01");
	SR_BIND(BOOL, flat_shaded, PROPERTY_HINT_NONE, "");
	SR_BIND(FLOAT, flatten_bottom, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	SR_BIND(COLOR, base_color, PROPERTY_HINT_NONE, "");
	SR_BIND(COLOR, top_color, PROPERTY_HINT_NONE, "");
	SR_BIND(INT, strata, PROPERTY_HINT_RANGE, "0,32,1");
	SR_BIND(FLOAT, moss_amount, PROPERTY_HINT_RANGE, "0,1,0.01");
	ClassDB::bind_method(D_METHOD("set_material", "material"), &SplineRocks::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &SplineRocks::get_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
	SR_BIND(BOOL, collision_enabled, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("rebuild"), &SplineRocks::rebuild);
	ClassDB::bind_method(D_METHOD("queue_rebuild"), &SplineRocks::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &SplineRocks::_on_spline_changed);
}
#undef SR_BIND
// clang-format on

SplineRocks::SplineRocks() {}
SplineRocks::~SplineRocks() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void SplineRocks::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			_connect_spline();
			queue_rebuild();
			break;
		case NOTIFICATION_PARENTED:
			if (is_inside_tree()) {
				_connect_spline();
				queue_rebuild();
			}
			break;
		case NOTIFICATION_UNPARENTED:
		case NOTIFICATION_EXIT_TREE:
			_disconnect_spline();
			break;
		case NOTIFICATION_PROCESS: // Only while waiting for terrain to stream in under the rocks
			_retry_timer -= get_process_delta_time();
			if (_retry_timer <= 0.0) {
				set_process(false);
				rebuild();
			}
			break;
		default:
			break;
	}
}

void SplineRocks::_connect_spline() {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (spline == _watched_spline) {
		return;
	}
	_disconnect_spline();
	if (spline) {
		spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
		_watched_spline = spline;
	}
}

void SplineRocks::_disconnect_spline() {
	if (_watched_spline) {
		const Callable cb(this, "_on_spline_changed");
		if (_watched_spline->is_connected("spline_changed", cb)) {
			_watched_spline->disconnect("spline_changed", cb);
		}
		_watched_spline = nullptr;
	}
}

void SplineRocks::_on_spline_changed() {
	_ground_retries = 0;
	queue_rebuild();
}

void SplineRocks::queue_rebuild() {
	if (!_rebuild_queued && is_inside_tree()) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

void SplineRocks::set_material(const Ref<Material> &p_material) {
	material = p_material;
	for (MultiMeshInstance3D *mm : _mm) {
		mm->set_material_override(material.is_valid() ? material : Ref<Material>(_stone));
	}
}

// ---------------------------------------------------------------------------------------------
// Variants and placement
// ---------------------------------------------------------------------------------------------

/// `variants` RockMeshes sharing this node's style, seeded seed + i; built synchronously.
void SplineRocks::_make_variants() {
	_variants.clear();
	for (int i = 0; i < variants; ++i) {
		Ref<RockMesh> m;
		m.instantiate();
		m->set_seed(seed * 31 + i);
		m->set_size(size);
		m->set_detail(detail);
		m->set_base_shape(base_shape);
		m->set_roundness(roundness);
		m->set_tilt(tilt);
		m->set_noise_amplitude(noise_amplitude);
		m->set_ridged(ridged);
		m->set_facet_snap(facet_snap);
		m->set_flat_shaded(flat_shaded);
		m->set_flatten_bottom(flatten_bottom);
		m->set_base_color(base_color);
		m->set_top_color(top_color);
		m->set_strata(strata);
		m->set_moss_amount(moss_amount);
		m->rebuild();
		_variants.push_back(m);
	}
}

/// Terrain3D height at a global XZ through the compositor two levels up; false while not generated.
bool SplineRocks::_ground_height(
		const Vector3 &p_global,
		float &r_height
) const {
	const Node *spline = get_parent();
	const TerrainSplineCompositor *comp =
			spline ? Object::cast_to<TerrainSplineCompositor>(spline->get_parent()) : nullptr;
	if (!comp || !comp->get_terrain()) {
		return false;
	}
	Object *data = Object::cast_to<Object>(comp->get_terrain()->get("data"));
	if (!data) {
		return false;
	}
	const Variant h = data->call("get_height", p_global);
	if (h.get_type() != Variant::FLOAT || Math::is_nan((float)h)) {
		return false;
	}
	r_height = (float)h;
	return true;
}

/**
 * @brief Walks the section every spacing (± jitter) metres; per station, per row: origin = spline point
 * + lateral (row and formation offsets, jitter) + vertical; basis elongated along the tangent or randomly
 * yawed, scaled by size_variation; optionally dropped to the ground. Returns instance transforms
 * (local space) grouped by variant.
 */
bool SplineRocks::_place(
		std::vector<std::vector<Transform3D>> &r_per_variant,
		bool &r_missing_ground
) const {
	r_per_variant.assign(_variants.size(), {});
	r_missing_ground = false;
	const ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline || _variants.empty()) {
		return false;
	}
	Ref<Curve3D> curve = spline->get_curve();
	if (curve.is_null() || curve->get_point_count() < 2) {
		return false;
	}
	const float total = curve->get_baked_length();
	const float start = CLAMP(section_start, 0.0f, total);
	const float end = section_end > section_start ? MIN(section_end, total) : total;
	if (end <= start) {
		return false;
	}
	const Transform3D to_local = get_global_transform().affine_inverse() * spline->get_global_transform();
	const Transform3D to_global = get_global_transform();
	prop::RNG rng((uint64_t)(uint32_t)seed);

	for (float d = start + start_offset; d <= end + 1e-3f;
		 d += spacing * (1.0f + spacing_jitter * rng.range(-1.0f, 1.0f))) {
		const Transform3D frame = to_local * curve->sample_baked_with_rotation(MIN(d, total), false, false);
		Vector3 along = frame.basis.get_column(2);
		along.y = 0.0f;
		along = along.length_squared() > 1e-8f ? along.normalized() : Vector3(0, 0, 1);
		const Vector3 lateral = Vector3(0, 1, 0).cross(along).normalized();

		for (int r = 0; r < rows; ++r) {
			const float row_lat = (r - (rows - 1) * 0.5f) * row_offset;
			Vector3 origin = frame.origin +
					lateral * (lateral_offset + row_lat + lateral_jitter * rng.range(-1.0f, 1.0f)) +
					Vector3(0, vertical_offset, 0);
			if (snap_to_ground) {
				float h;
				if (_ground_height(to_global.xform(origin), h)) {
					origin.y = to_global.affine_inverse().xform(Vector3(0, h, 0)).y + vertical_offset;
				} else {
					r_missing_ground = true;
				}
			}
			Basis basis;
			if (align_to_tangent) {
				basis = Basis::looking_at(-along, Vector3(0, 1, 0)).rotated(Vector3(0, 1, 0), (float)Math::PI * 0.5f);
				basis = basis.rotated(Vector3(0, 1, 0), rng.range(-0.25f, 0.25f));
			} else {
				basis = Basis(Vector3(0, 1, 0), rng.range(0.0f, (float)Math::TAU));
			}
			const float s = 1.0f + size_variation * rng.range(-1.0f, 1.0f);
			basis = basis.scaled(Vector3(s, s * rng.range(0.85f, 1.15f), s));
			const size_t v = rng.next() % _variants.size();
			r_per_variant[v].push_back(Transform3D(basis, origin));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Rebuild
// ---------------------------------------------------------------------------------------------

void SplineRocks::_clear_children() {
	for (MultiMeshInstance3D *mm : _mm) {
		mm->queue_free();
	}
	_mm.clear();
	if (static_body) {
		static_body->queue_free();
		static_body = nullptr;
	}
}

void SplineRocks::rebuild() {
	_rebuild_queued = false;
	_clear_children();
	_make_variants();
	if (_stone.is_null()) {
		_stone = make_stone_material();
	}

	std::vector<std::vector<Transform3D>> per_variant;
	bool missing_ground = false;
	if (!_place(per_variant, missing_ground)) {
		return;
	}
	if (collision_enabled) {
		static_body = memnew(StaticBody3D);
		static_body->set_name("RocksBody");
		add_child(static_body, false, INTERNAL_MODE_BACK);
	}
	for (size_t v = 0; v < _variants.size(); ++v) {
		const std::vector<Transform3D> &xf = per_variant[v];
		if (xf.empty()) {
			continue;
		}
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_mesh(_variants[v]);
		mm->set_instance_count((int)xf.size());
		for (int i = 0; i < (int)xf.size(); ++i) {
			mm->set_instance_transform(i, xf[i]);
		}
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		mmi->set_name(String("Rocks") + String::num_int64((int64_t)v));
		mmi->set_multimesh(mm);
		mmi->set_material_override(material.is_valid() ? material : Ref<Material>(_stone));
		add_child(mmi, false, INTERNAL_MODE_BACK);
		_mm.push_back(mmi);

		if (static_body) {
			for (int b = 0; b < _variants[v]->get_collision_shape_count(); ++b) {
				Ref<Shape3D> shape = _variants[v]->create_collision_shape(b);
				if (shape.is_null()) {
					continue;
				}
				for (const Transform3D &t : xf) {
					CollisionShape3D *cs = memnew(CollisionShape3D);
					cs->set_shape(shape);
					static_body->add_child(cs, false, INTERNAL_MODE_BACK);
					cs->set_transform(t);
				}
			}
		}
	}

	// Terrain not there yet under some rocks: try again shortly (streaming), up to 20 times.
	if (missing_ground && snap_to_ground && !Engine::get_singleton()->is_editor_hint() && _ground_retries < 20) {
		_ground_retries++;
		_retry_timer = 0.5;
		set_process(true);
	}
}

} // namespace godot
