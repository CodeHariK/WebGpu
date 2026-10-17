// Fire for the blade trail ribbon (fire_trail.gd), built up in 4 steps — `fire_step` picks how
// many are on (V in the demo cycles 1 → 4), so each idea can be seen on its own.
//
// What the ribbon gives us per pixel:
//   UV.x = age     0 right behind the blade … 1 at the tail, about to vanish
//   UV.y = across  0 at the blade's base … 1 at its tip
//
// It is unshaded (no lighting: fire makes its own light) and blended normally over what's behind
// (additive blending — colour ADDED to the background — washed out to white over the grey ground,
// so the cartoon colours stay solid instead). Colours above 1.0 trigger glow.
shader_type spatial;
render_mode unshaded, blend_mix, cull_disabled, depth_draw_never, shadows_disabled;

uniform int fire_step : hint_range(1, 4) = 4;
uniform sampler2D noise : repeat_enable, filter_linear; // a NoiseTexture2D, made in the demo
uniform float brightness = 1.3; // > 1: glows (only the hottest part, so the colours still show)
uniform float bands = 4.0; // step 4: how many flat colour bands

// Step 2's colours, hottest to coolest.
const vec3 WHITE = vec3(1.0, 0.95, 0.8);
const vec3 YELLOW = vec3(1.0, 0.8, 0.2);
const vec3 ORANGE = vec3(1.0, 0.4, 0.05);
const vec3 RED = vec3(0.6, 0.06, 0.02);

// heat 1 = hottest (white) … 0 = coolest (dark red): three blends, one after the other.
vec3 fire_colour(float heat) {
	if (heat > 0.66) return mix(YELLOW, WHITE, (heat - 0.66) / 0.34);
	if (heat > 0.33) return mix(ORANGE, YELLOW, (heat - 0.33) / 0.33);
	return mix(RED, ORANGE, heat / 0.33);
}

void fragment() {
	float age = UV.x;
	float across = UV.y;

	// STEP 1 — fade: one flat orange; the older, the more transparent.
	vec3 colour = ORANGE;
	float alpha = 1.0 - age;

	// STEP 2 — colour by age: fresh flame is white-hot, the tail cools to dark red. The power
	// makes it cool fast, so most of the trail is orange and red and only the very front is white.
	float heat = pow(1.0 - age, 2.2);
	if (fire_step >= 2) {
		colour = fire_colour(heat);
	}

	// STEP 3 — flicker: a noise pattern, sliding along the trail over time, eats into the flame.
	// Where the noise is weaker than the flame's age, the flame is gone — so the tail breaks up
	// into tongues while the fresh part stays solid. The edges along the blade fade too.
	if (fire_step >= 3) {
		float n = texture(noise, vec2(age * 1.5 - TIME * 1.2, across * 0.8 + TIME * 0.25)).r;
		heat = clamp(heat * 0.7 + n * 0.6 - age * 0.3, 0.0, 1.0);
		float edges = sin(across * PI); // 0 at base and tip, 1 in the middle
		alpha = smoothstep(0.05, 0.25, heat * mix(0.6, 1.0, edges));
		colour = fire_colour(heat);
	}

	// STEP 4 — cartoon: snap the heat to a few flat bands (hard steps, no smooth blend) and cut
	// the alpha hard too: toon fire, crisp edges, fits a cute style.
	if (fire_step >= 4) {
		float banded = floor(heat * bands) / bands;
		colour = fire_colour(banded + 0.5 / bands);
		alpha = step(0.12, heat);
	}

	ALBEDO = colour * brightness;
	ALPHA = alpha;
}
