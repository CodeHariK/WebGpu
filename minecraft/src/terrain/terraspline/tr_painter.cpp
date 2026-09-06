/**
 * @file tr_painter.cpp
 * @brief TerrainSplinePainter: bindings, the private shape deformer, and the control-map paint.
 */
#include "tr_painter.h"
#include "tr_deformer.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define TR_PAINTER_BIND(m_variant, m_name, m_hint, m_hint_str)                                                         \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &TerrainSplinePainter::set_##m_name);                      \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &TerrainSplinePainter::get_##m_name);                               \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void TerrainSplinePainter::_bind_methods() {
	ADD_GROUP("Paint", "");
	TR_PAINTER_BIND(INT, texture_id, PROPERTY_HINT_RANGE, "0,31,1");
	TR_PAINTER_BIND(FLOAT, strength, PROPERTY_HINT_RANGE, "0,1,0.01");
	ClassDB::bind_method(D_METHOD("set_paint_curve", "curve"), &TerrainSplinePainter::set_paint_curve);
	ClassDB::bind_method(D_METHOD("get_paint_curve"), &TerrainSplinePainter::get_paint_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "paint_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_paint_curve", "get_paint_curve");

	ADD_GROUP("Shape", "");
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &TerrainSplinePainter::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &TerrainSplinePainter::get_shape);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape", PROPERTY_HINT_ENUM, "From Deformer (sibling footprint),Custom"), "set_shape", "get_shape");
	TR_PAINTER_BIND(FLOAT, spline_width, PROPERTY_HINT_RANGE, "0,200,0.1,suffix:m");
	TR_PAINTER_BIND(FLOAT, falloff_distance, PROPERTY_HINT_RANGE, "0,200,0.1,suffix:m");
	TR_PAINTER_BIND(BOOL, fill_interior, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("set_falloff_curve", "curve"), &TerrainSplinePainter::set_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_falloff_curve"), &TerrainSplinePainter::get_falloff_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_falloff_curve", "get_falloff_curve");

	ClassDB::bind_method(D_METHOD("mark_dirty"), &TerrainSplinePainter::mark_dirty);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &TerrainSplinePainter::_on_curve_changed);

	BIND_ENUM_CONSTANT(SHAPE_FROM_DEFORMER);
	BIND_ENUM_CONSTANT(SHAPE_CUSTOM);
}
#undef TR_PAINTER_BIND
// clang-format on

TerrainSplinePainter::TerrainSplinePainter() {}

TerrainSplinePainter::~TerrainSplinePainter() {
	if (_shape) {
		memdelete(_shape);
		_shape = nullptr;
	}
}

// ---------------------------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------------------------

void TerrainSplinePainter::_validate_property(PropertyInfo &p_property) const {
	if (shape != SHAPE_CUSTOM &&
		(p_property.name == StringName("spline_width") || p_property.name == StringName("falloff_distance") ||
		 p_property.name == StringName("fill_interior") || p_property.name == StringName("falloff_curve"))) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void TerrainSplinePainter::set_falloff_curve(const Ref<Curve> &p_curve) {
	if (falloff_curve.is_valid()) {
		falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
	falloff_curve = p_curve;
	if (falloff_curve.is_valid()) {
		falloff_curve->connect("changed", Callable(this, "_on_curve_changed"));
	}
	mark_dirty();
}

void TerrainSplinePainter::set_paint_curve(const Ref<Curve> &p_curve) {
	if (paint_curve.is_valid()) {
		paint_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
	paint_curve = p_curve;
	if (paint_curve.is_valid()) {
		paint_curve->connect("changed", Callable(this, "_on_curve_changed"));
	}
	mark_dirty();
}

void TerrainSplinePainter::mark_dirty() {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (spline) {
		spline->mark_dirty();
	}
}

void TerrainSplinePainter::_on_curve_changed() { mark_dirty(); }

// ---------------------------------------------------------------------------------------------
// Preparation (main thread)
// ---------------------------------------------------------------------------------------------

/// The private deformer never enters the tree; its setters' mark_dirty finds no parent and is a no-op.
void TerrainSplinePainter::_sync_shape() {
	if (!_shape) {
		_shape = memnew(TerrainSplineDeformer);
	}
	_shape->set_spline_width(spline_width);
	_shape->set_falloff_distance(falloff_distance);
	_shape->set_inner_falloff_distance(falloff_distance);
	_shape->set_fill_interior(fill_interior);
	_shape->set_falloff_curve(falloff_curve);
}

void TerrainSplinePainter::prepare() {
	if (shape == SHAPE_CUSTOM) {
		_sync_shape();
	}
	_has_paint_curve = paint_curve.is_valid();
	if (_has_paint_curve) {
		_baked_paint_curve.resize(256);
		for (int i = 0; i < 256; ++i) {
			_baked_paint_curve[i] = paint_curve->sample((float)i / 255.0f);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Paint (any thread)
// ---------------------------------------------------------------------------------------------

uint32_t TerrainSplinePainter::paint_control(
		uint32_t p_old,
		int p_tex,
		float p_weight
) {
	const uint32_t old_base = (p_old >> 27) & 0x1Fu;
	const uint32_t old_over = (p_old >> 22) & 0x1Fu;
	const uint32_t old_blend = (p_old >> 14) & 0xFFu;
	const uint32_t dominant = old_blend < 128u ? old_base : old_over;
	const uint32_t tex = (uint32_t)p_tex & 0x1Fu;
	const float w = CLAMP(p_weight, 0.0f, 1.0f);

	uint32_t base, over, blend;
	if (w >= 0.5f) { // New texture dominates: it is the base, the old one fades in over 1 - w
		base = tex;
		over = dominant;
		blend = (uint32_t)Math::round((1.0f - w) * 255.0f);
	} else {
		base = dominant;
		over = tex;
		blend = (uint32_t)Math::round(w * 255.0f);
	}
	const uint32_t keep = p_old & ((1u << 14) - 1u) & ~1u; // uv rotation/scale, hole, nav; autoshader off
	return (base << 27) | (over << 22) | (blend << 14) | keep;
}

void TerrainSplinePainter::paint_prepared(
		std::vector<uint32_t> &r_control,
		ProceduralSpline3D *p_spline,
		const Vector2 &p_offset,
		int p_size,
		const Rect2 &p_padded_aabb,
		TerrainSplineDeformer *p_sibling
) {
	if (strength <= 0.0f || p_spline == nullptr || p_size <= 0) {
		return;
	}
	TerrainSplineDeformer *field_owner = shape == SHAPE_CUSTOM ? _shape : p_sibling;
	if (field_owner == nullptr) {
		return;
	}
	std::vector<float> weights;
	if (!field_owner->compute_weight_field(p_spline, p_offset, p_size, p_padded_aabb, weights)) {
		return;
	}
	if (r_control.empty()) {
		r_control.assign((size_t)p_size * p_size, (uint32_t)CONTROL_DEFAULT);
	}
	const size_t n = (size_t)p_size * p_size;
	for (size_t i = 0; i < n; ++i) {
		float w = weights[i];
		if (w <= 0.0f) {
			continue;
		}
		if (_has_paint_curve) {
			w = _baked_paint_curve[CLAMP((int)(w * 255.0f), 0, 255)];
		}
		w *= strength;
		if (w <= 0.001f) {
			continue;
		}
		r_control[i] = paint_control(r_control[i], texture_id, w);
	}
}

} // namespace godot
