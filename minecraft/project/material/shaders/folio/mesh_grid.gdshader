shader_type spatial;

// =============================================================================
// Folio "MeshGridMaterial" port
//   folio-2025/sources/Game/Materials/MeshGridMaterial.js
//
// A NodeMaterial with lights=false, normals=false: a pure, UNLIT triplanar grid
// material (background colour with grid lines drawn on top). It is NOT part of
// the lit MeshDefault pipeline, so it does not share folio_material.gdshaderinc.
//
// Two grid line sets are supported (folio uses an array; 1-2 covers major/minor).
// Antialiased grid is folio's default (Ben Golus method); a cheap non-AA path is
// available via `antialiased = false`.
// =============================================================================

render_mode unshaded, cull_back;

// Which coordinates drive the grid (folio `reference`):
//   0 uv · 1 worldTriplanar · 2 worldX · 3 worldY · 4 worldZ
//   5 localTriplanar · 6 localX · 7 localY · 8 localZ
uniform int grid_reference : hint_range(0, 8) = 3; // default worldY (ground)
uniform bool antialiased = true;
uniform vec4 grid_background : source_color = vec4(0.0, 0.0, 0.0, 1.0);
uniform float grid_scale = 1.0; // folio scaleNode (global multiplier)
uniform int line_count : hint_range(1, 2) = 1;

// Line 0
uniform vec4 line0_color : source_color = vec4(1.0);
uniform float line0_line_scale = 1.0;
uniform float line0_thickness = 0.05;
uniform float line0_cross = 1.0;
uniform vec2 line0_offset = vec2(0.0);
// Line 1
uniform vec4 line1_color : source_color = vec4(1.0);
uniform float line1_line_scale = 10.0;
uniform float line1_thickness = 0.03;
uniform float line1_cross = 1.0;
uniform vec2 line1_offset = vec2(0.0);

varying vec3 v_world_pos;
varying vec3 v_local_pos;
varying vec3 v_world_normal;
varying vec2 v_uv;

// folio toMask: pick the dominant axis of the (world) normal for triplanar.
vec3 to_mask(vec3 nrm) {
	float dot_x = abs(nrm.x);
	float dot_y = abs(nrm.y);
	float dot_z = abs(nrm.z);
	vec3 mask = vec3(1.0, 0.0, 0.0);
	if (dot_z > dot_x) {
		mask = vec3(0.0, 0.0, 1.0);
	}
	if (dot_y > dot_x && dot_y > dot_z) {
		mask = vec3(0.0, 1.0, 0.0);
	}
	return mask;
}

// folio toTriplanarUv: choose the projection plane from the mask.
vec2 to_triplanar_uv(vec3 pos, vec3 mask) {
	vec2 uvr = pos.yz;
	uvr = mix(uvr, pos.xz, mask.y);
	uvr = mix(uvr, pos.xy, mask.z);
	return uvr;
}

// folio toGrid (cheap, no antialiasing).
float to_grid(vec2 uvr_in, float scale, float thickness, vec2 offset, float cross) {
	vec2 reference_uv = uvr_in / scale + offset;
	vec2 cross_grid = step(abs(fract(reference_uv) - 0.5), vec2((1.0 - cross) * 0.5));
	float cross_mask = 1.0 - mix(cross_grid.x, 1.0, cross_grid.y);
	vec2 grid = step(abs(fract(reference_uv - 0.5) - 0.5) * 2.0, vec2(thickness)) * cross_mask;
	return mix(grid.x, 1.0, grid.y);
}

// folio toAntialiasedGrid (Ben Golus "best darn grid shader yet").
float to_aa_grid(vec2 uvr_in, float scale, float thickness, vec2 offset,
		float cross, float deriv_mask) {
	float line_width = thickness;
	vec2 reference_uv = uvr_in / scale + offset;
	vec2 uv_deriv = fwidth(reference_uv) * deriv_mask;
	vec2 draw_width = clamp(vec2(line_width), uv_deriv, vec2(1.0));
	vec2 line_aa = uv_deriv * 1.5;

	vec2 cross_grid = step(abs(fract(reference_uv) - 0.5), vec2((1.0 - cross) * 0.5));
	float cross_mask = 1.0 - mix(cross_grid.x, 1.0, cross_grid.y);

	vec2 grid_uv = 1.0 - abs(fract(reference_uv) * 2.0 - 1.0);
	vec2 grid2 = smoothstep(draw_width + line_aa, draw_width - line_aa, grid_uv);
	grid2 *= clamp(vec2(line_width) / draw_width, 0.0, 1.0);
	grid2 = mix(grid2, vec2(line_width), clamp(uv_deriv * 2.0 - 1.0, 0.0, 1.0)) * cross_mask;
	return mix(grid2.x, 1.0, grid2.y);
}

vec2 grid_reference_uv(vec3 mask) {
	// UV mode uses the mesh's UV; every other mode is world/local projected.
	if (grid_reference == 1) {
		return to_triplanar_uv(v_world_pos, mask);
	} else if (grid_reference == 2) {
		return v_world_pos.yz;
	} else if (grid_reference == 3) {
		return v_world_pos.xz;
	} else if (grid_reference == 4) {
		return v_world_pos.xy;
	} else if (grid_reference == 5) {
		return to_triplanar_uv(v_local_pos, mask);
	} else if (grid_reference == 6) {
		return v_local_pos.yz;
	} else if (grid_reference == 7) {
		return v_local_pos.xz;
	} else if (grid_reference == 8) {
		return v_local_pos.xy;
	}
	return v_uv; // 0: mesh uv
}

void vertex() {
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_local_pos = VERTEX;
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
	v_uv = UV;
}

float grid_line(vec2 uvr, float line_scale, float thickness, vec2 offset,
		float cross, float deriv_mask) {
	if (antialiased) {
		return to_aa_grid(uvr, line_scale * grid_scale, thickness, offset, cross, deriv_mask);
	}
	return to_grid(uvr, line_scale * grid_scale, thickness, offset, cross);
}

void fragment() {
	vec3 mask = to_mask(normalize(v_world_normal));
	float deriv_mask = clamp(1.0 - length(fwidth(mask)), 0.0, 1.0);

	vec2 uvr = (grid_reference == 0) ? v_uv : grid_reference_uv(mask);

	vec3 grid_color = grid_background.rgb;

	float g0 = grid_line(uvr, line0_line_scale, line0_thickness, line0_offset,
			line0_cross, deriv_mask);
	grid_color = mix(grid_color, line0_color.rgb, g0);

	if (line_count > 1) {
		float g1 = grid_line(uvr, line1_line_scale, line1_thickness, line1_offset,
				line1_cross, deriv_mask);
		grid_color = mix(grid_color, line1_color.rgb, g1);
	}

	ALBEDO = grid_color;
}
