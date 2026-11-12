// STEP 2 — lumpy: each puff its own bumpy outline, like a little cloud.
// The circle's edge is moved in and out by a smooth noise going round it: 7 random heights spaced
// round the circle, blended between (smoothstep), the 7th blending back into the 1st so it closes.
// Each puff gets its own heights from its particle number (INSTANCE_ID, sent from the vertex
// shader in a varying), so no two puffs have the same outline.
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 puff_color = vec3(2.2, 2.2, 2.3);  // linear, past white like the froth
uniform float lumps = 0.25;  // NEW: step 2: how far the edge goes in and out

varying float seed;  // NEW: each puff its own number (its particle index)

// NEW: a random number for a whole number and a seed.
float hash(float n, float s) {  // NEW
	return fract(sin(n * 12.9898 + s * 78.233) * 43758.5453);
}

// NEW: smooth noise round a circle: 7 random heights spaced round it, blended, wrapping at the end.
float round_noise(float angle, float s) {  // NEW
	float x = angle / TAU * 7.0;
	float i = floor(x);
	float f = x - i;
	f = f * f * (3.0 - 2.0 * f);
	return mix(hash(mod(i, 7.0), s), hash(mod(i + 1.0, 7.0), s), f);
}

void vertex() {
	seed = float(INSTANCE_ID);  // NEW
	// face the camera: the camera's axes, scaled by the particle's size, at the particle
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
	MODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0),
			vec4(0.0, length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0),
			vec4(0.0, 0.0, 0.0, 1.0));
}

void fragment() {
	float from_middle = length(UV - 0.5) * 2.0;  // 0 in the middle, 1 at the quad's edge
	ALBEDO = puff_color;
	float angle = atan(UV.y - 0.5, UV.x - 0.5) + PI;  // NEW: round the middle, 0 .. 2 pi
	float edge = 0.9 - lumps * round_noise(angle, seed);  // NEW: the edge comes in by up to 0.25
	ALPHA = step(from_middle, edge);  // NEW: a lumpy circle
	ALPHA_SCISSOR_THRESHOLD = 0.5;  // a hard cut, no blending
}
