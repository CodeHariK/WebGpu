/**
 * @file tr_toon.cpp
 * @brief Shared toon solid + outline materials (shader source lives here).
 */
#include "tr_toon.h"
#include <godot_cpp/classes/shader.hpp>

namespace godot {

namespace {

/// Uniforms, varyings, vertex() and light() shared by the solid and road variants.
const char *TOON_COMMON = R"(
shader_type spatial;
render_mode cull_back, depth_draw_opaque, specular_disabled;

uniform int bands : hint_range(1, 4) = 2;                // Diffuse steps (1 = lit / shadow only)
uniform float band_softness : hint_range(0.0, 0.5) = 0.04; // Width of each step's edge
uniform float shadow_level : hint_range(0.0, 1.0) = 0.62;  // Brightness of the darkest step
uniform vec3 shadow_tint : source_color = vec3(0.72, 0.78, 1.0); // Cool shadows
uniform float rim_strength : hint_range(0.0, 1.0) = 0.25;
uniform float rim_width : hint_range(0.05, 1.0) = 0.45;
uniform vec3 rim_color : source_color = vec3(1.0, 0.97, 0.9);
uniform float saturation : hint_range(0.0, 2.0) = 1.0;
// F3 debug views (GameManager cycles the global): 0 off, 1 UV, 2 UV2, 3 raw vertex colour, 4 marking mask
global uniform int ts_debug_view;

varying vec3 v_albedo;
varying vec3 v_raw_color;

// False-colour debug output for views 1..3; returns false when off (view 4 is per shader).
bool toon_debug(vec2 uv, vec2 uv2, out vec3 c) {
	c = vec3(0.0);
	if (ts_debug_view == 1) {
		c = vec3(uv.x, fract(uv.y), 0.0);
	} else if (ts_debug_view == 2) {
		c = vec3(uv2.x, uv2.y, 0.0);
	} else if (ts_debug_view == 3) {
		c = v_raw_color;
	} else {
		return false;
	}
	return true;
}

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + vec3(0.055)) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

void vertex() {
	vec3 c = srgb_to_linear(COLOR.rgb);
	float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
	v_albedo = clamp(mix(vec3(l), c, saturation), 0.0, 1.0);
	v_raw_color = COLOR.rgb;
}

// Diffuse: NdotL * shadow attenuation, quantized into `bands` steps between shadow_level and 1.
void light() {
	float ndl = clamp(dot(NORMAL, LIGHT), 0.0, 1.0) * ATTENUATION;
	float n = float(bands);
	float stepped = floor(ndl * n) / n;
	float edge = fract(ndl * n);
	stepped += smoothstep(1.0 - band_softness * n, 1.0, edge) / n; // soften the step edges
	float lit = mix(shadow_level, 1.0, clamp(stepped, 0.0, 1.0));
	vec3 tint = mix(shadow_tint, vec3(1.0), clamp(stepped, 0.0, 1.0));
	DIFFUSE_LIGHT += ALBEDO * (LIGHT_COLOR / PI) * lit * tint; // LIGHT_COLOR carries energy * PI
}
)";

const char *TOON_SOLID_FRAGMENT = R"(
void fragment() {
	vec3 dbg;
	if (toon_debug(UV, UV2, dbg) || ts_debug_view == 4) {
		ALBEDO = vec3(0.0);
		EMISSION = dbg; // Unlit false colour
		ROUGHNESS = 1.0;
		SPECULAR = 0.0;
	} else {
		ALBEDO = v_albedo;
		ROUGHNESS = 1.0;
		SPECULAR = 0.0;
		float rim = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 1.0 / rim_width);
		EMISSION = rim_color * v_albedo * rim * rim_strength;
	}
}
)";

/// Lane markings from UV2 (x = 0..1 across the deck, y = on-deck flag) and UV.y (track metres / texture_length).
const char *TOON_ROAD_FRAGMENT = R"(
uniform float deck_width = 8.0;      // metres, set by the road
uniform float texture_length = 10.0; // metres per V repeat, set by the road
uniform int lanes : hint_range(0, 8) = 0;
uniform bool dashed = true;
uniform bool edge_lines = false;
uniform float line_width = 0.25;     // metres
uniform float dash_length = 3.0;     // metres (gap equal)
uniform float edge_inset = 0.4;      // metres from the deck edge
uniform vec3 line_color : source_color = vec3(0.96, 0.93, 0.8);


void fragment() {
	vec3 albedo = v_albedo;
	float mask = 0.0;
	if (UV2.y > 0.5 && deck_width > 0.0 && (lanes > 1 || edge_lines)) {
		float lat = (UV2.x - 0.5) * deck_width;          // metres from the centre line
		float along = UV.y * texture_length;
		float aa = fwidth(lat) + 1e-4;
		float half_w = line_width * 0.5;
		float line = 0.0;
		if (lanes > 1) {
			float lane_w = deck_width / float(lanes);
			float d = 1e9;
			for (int i = 1; i < lanes; i++) {
				d = min(d, abs(lat - (-0.5 * deck_width + lane_w * float(i))));
			}
			float on = 1.0 - smoothstep(half_w - aa, half_w + aa, d);
			if (dashed) {
				on *= step(fract(along / (2.0 * dash_length)), 0.5);
			}
			line = on;
		}
		if (edge_lines) {
			float e = abs(abs(lat) - (0.5 * deck_width - edge_inset));
			line = max(line, 1.0 - smoothstep(half_w - aa, half_w + aa, e));
		}
		albedo = mix(albedo, srgb_to_linear(line_color), line);
		mask = line;
	}
	vec3 dbg;
	if (toon_debug(UV, UV2, dbg) || ts_debug_view == 4) {
		ALBEDO = vec3(0.0);
		EMISSION = ts_debug_view == 4 ? vec3(mask) : dbg; // Unlit false colour
		ROUGHNESS = 1.0;
		SPECULAR = 0.0;
	} else {
		ALBEDO = albedo;
		ROUGHNESS = 1.0;
		SPECULAR = 0.0;
		float rim = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 1.0 / rim_width);
		EMISSION = rim_color * albedo * rim * rim_strength;
	}
}
)";

Ref<ShaderMaterial> make_material(const String &p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	return m;
}

} // namespace

Ref<ShaderMaterial> make_toon_solid_material() { return make_material(String(TOON_COMMON) + TOON_SOLID_FRAGMENT); }

Ref<ShaderMaterial> make_toon_road_material() { return make_material(String(TOON_COMMON) + TOON_ROAD_FRAGMENT); }

Ref<ShaderMaterial> make_toon_outline_material(
		float p_width,
		const Color &p_color
) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(R"(
shader_type spatial;
render_mode cull_front, unshaded, depth_draw_opaque;

uniform float width = 0.08;   // metres
uniform vec3 color : source_color = vec3(0.12, 0.08, 0.1);

void vertex() {
	VERTEX += NORMAL * width;
}

void fragment() {
	ALBEDO = color;
}
)");
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	m->set_shader_parameter("width", p_width);
	m->set_shader_parameter("color", p_color);
	return m;
}

} // namespace godot
