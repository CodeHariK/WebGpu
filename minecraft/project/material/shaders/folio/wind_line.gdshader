shader_type spatial;

// =============================================================================
// Folio "WindLine"  (folio World/WindLines.js + Geometries/WindLineGeometry.js)
//
// One wavy ribbon strip laid along its local Z, sampled from a Catmull-Rom curve.
// The ribbon has zero width at rest (both edge verts sit on the centreline); a
// travelling "bulge" widens a section of it as `progress` sweeps 0->1, so it
// reads as a gust of wind streaking across the sky. Width is pushed along a fixed
// world tangent (folio `_tangent = (0,1,-1)`), same as folio's vertexNode.
//
// Per-vertex UV carries (ratio, side): ratio = 0..1 along the length, side = ±0.5
// picks the ribbon edge (folio derives both from attributes / vertexIndex).
//
// Unshaded white, transparent; alpha fades with the bulge so ends are soft.
// =============================================================================

render_mode unshaded, cull_disabled, shadows_disabled;

uniform float thickness = 0.1;
uniform float progress = 0.0;

varying float v_fade;

void vertex() {
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;

	float ratio = UV.x; // 0..1 along the ribbon
	float side = UV.y; // -0.5 / +0.5 ribbon edge

	// folio: taper to the two ends + a travelling bulge at `progress`.
	float base_thickness = smoothstep(0.0, 1.0, 1.0 - abs(ratio - 0.5) * 2.0);
	float remapped_progress = progress * 3.0 - 1.0;
	float progress_thickness = smoothstep(0.0, 1.0, 1.0 - abs(ratio - remapped_progress));
	float final_thickness = thickness * base_thickness * progress_thickness;

	vec3 tangent = normalize(vec3(0.0, 1.0, -1.0)); // folio fixed world tangent
	world += tangent * (side * final_thickness);

	v_fade = base_thickness * progress_thickness;

	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * vec4(world, 1.0);
}

void fragment() {
	ALBEDO = vec3(1.0);
	ALPHA = v_fade;
}
