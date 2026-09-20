shader_type spatial;

// =============================================================================
// Folio "RainLines"  (folio World/RainLines.js)
//
// A tiled field of falling line-quads around the camera. Each quad's base XZ sits
// in [0,1]; the vertex stage tiles it across a `size`-wide area centred on the
// view, then drops it from `elevation` to the ground on a per-line looping
// progress (offset by a per-line random). Per-vertex `offset` (UV) picks the
// side (x) for thickness and the top/bottom (y) for the streak length; `random`
// (UV2.x) decorrelates timing + gates visibility.
//
// The same system renders SNOW: when weather.snow raises, the manager shortens
// `line_length` and slows `local_time`, turning streaks into slow flecks.
//
// Uniforms are driven per-tick by FolioRainLines from the weather globals.
// Unshaded white; alpha for softness. folio uses hasCoreShadows — omitted here to
// keep the moving-vertex pass cheap (streaks are thin + bright).
// =============================================================================

render_mode unshaded, cull_disabled, shadows_disabled;

uniform float thickness = 0.015;
uniform float elevation = 20.0;
uniform float incline = 0.2;
uniform float size = 80.0;
uniform vec2 center = vec2(0.0);
uniform float line_length = 2.0; // folio `length` (renamed; GLSL builtin)
uniform float local_time = 0.0;
uniform float visible_ratio = 0.0;
uniform float alpha : hint_range(0.0, 1.0) = 0.6;

void vertex() {
	vec3 p = VERTEX; // base: x,z in [0,1], y = 0
	vec2 off = UV; // (side, top/bottom)
	float rnd = UV2.x; // per-line random
	vec2 tangent = vec2(0.707, -0.707);

	// Tile the field around the camera centre (folio loop).
	p.xz *= size;
	p.xz -= center;
	float half_size = size * 0.5;
	p.x = mod(p.x + half_size, size) - half_size;
	p.z = mod(p.z + half_size, size) - half_size;
	p.xz += center;

	// Thickness (side offset along the fixed tangent).
	p.xz += tangent * (off.x * thickness);

	// Fall: top = elevation+length; bottom verts drop by length; slide down with
	// a looping progress; clamp to [0, elevation].
	float progress = mod(local_time + rnd, 1.0);
	p.y = elevation + line_length;
	p.y -= line_length * (1.0 - off.y);
	p.y -= progress * (elevation + line_length);
	p.y = clamp(p.y, 0.0, elevation);

	// Visibility gate: hide a fraction of lines by shoving them far up.
	float visible = step(visible_ratio, fract(rnd * 99.0));
	p.y += visible * 99.0;

	// Wind incline: slant more the higher the line.
	p.xz += tangent * (p.y * incline * -1.0);

	VERTEX = p;
}

void fragment() {
	ALBEDO = vec3(1.0);
	ALPHA = alpha;
}
