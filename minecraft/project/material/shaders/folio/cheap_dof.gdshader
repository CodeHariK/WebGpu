shader_type canvas_item;

// =============================================================================
// Folio "cheapDOF" post-process port
//   folio-2025/sources/Game/Passes/cheapDOF.js
//
// A fake tilt-shift, NOT depth-of-field: a sharp horizontal band across the
// vertical centre of the screen, hash-blurred toward the top and bottom. Gives
// the toy/miniature look. Runs as a fullscreen ColorRect on a CanvasLayer,
// reading the rendered frame via the screen texture. FolioRendering shows/hides
// this layer by quality level (on at level 0, off at level 1).
//
//   strength = smoothstep(start, end, abs(SCREEN_UV.y - 0.5))
//   blurred  = hashBlur(screen, strength * amount, repeats)
//   COLOR    = mix(screen, blurred, strength)
// =============================================================================

uniform sampler2D screen_tex : hint_screen_texture, filter_linear;

uniform float dof_start = 0.2;
uniform float dof_end = 0.5;
uniform int dof_repeats : hint_range(1, 100) = 25;
uniform float dof_amount = 0.003;

const float DOF_TAU = 6.28318530718;

float hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void fragment() {
	vec2 uv = SCREEN_UV;
	float strength = smoothstep(dof_start, dof_end, abs(uv.y - 0.5));

	vec4 original = texture(screen_tex, uv);
	if (strength <= 0.0) {
		COLOR = original;
	} else {
		float radius = strength * dof_amount;
		float rot = hash12(uv) * DOF_TAU; // per-pixel random rotation
		vec4 acc = vec4(0.0);
		// Golden-angle disk samples (hash blur), sqrt() for even area coverage.
		for (int i = 0; i < dof_repeats; i++) {
			float t = (float(i) + 0.5) / float(dof_repeats);
			float ang = rot + float(i) * 2.399963; // golden angle (rad)
			float rad = radius * sqrt(t);
			vec2 off = vec2(cos(ang), sin(ang)) * rad;
			acc += texture(screen_tex, uv + off);
		}
		vec4 blurred = acc / float(dof_repeats);
		COLOR = mix(original, blurred, strength);
	}
}
