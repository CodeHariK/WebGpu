// Puffs rising off the froth (not in the Blender file: its spray is small spheres only),
// on the quads of Particles/Puffs:
//   billboard  each quad turned to face the camera
//   shape      a circle with a lumpy edge (its own per particle), cut out hard (alpha scissor)
//   life       grows from 45 % as it rises, crumbles away with blotchy holes at the end
//   light      lit as if it were a ball: the froth's toon light
// Built step by step in puff_steps/ (the last step is this file).
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 puff_color = vec3(2.2, 2.2, 2.3);  // linear, past white like the froth
uniform float lumps = 0.25;  // step 2: how far the edge goes in and out
uniform vec3 sun_direction = vec3(-0.777, 0.602, 0.370);  // step 4: toward the sun, Godot's axes
uniform float shade = 0.45;  // step 4: the side away from the sun, like the froth

varying float seed;  // each puff its own number (its particle index)
varying float age;  // 0 when the puff is born .. 1 when it dies

// a random number for a whole number and a seed.
float hash(float n, float s) {
	return fract(sin(n * 12.9898 + s * 78.233) * 43758.5453);
}

// smooth noise round a circle: 7 random heights spaced round it, blended, wrapping at the end.
float round_noise(float angle, float s) {
	float x = angle / TAU * 7.0;
	float i = floor(x);
	float f = x - i;
	f = f * f * (3.0 - 2.0 * f);
	return mix(hash(mod(i, 7.0), s), hash(mod(i + 1.0, 7.0), s), f);
}

// smooth noise across the puff: random heights on a grid, blended (value noise).
float blotches(vec2 p, float s) {
	vec2 i = floor(p);
	vec2 f = p - i;
	f = f * f * (3.0 - 2.0 * f);
	float a = hash(i.x + i.y * 57.0, s);
	float b = hash(i.x + 1.0 + i.y * 57.0, s);
	float c = hash(i.x + (i.y + 1.0) * 57.0, s);
	float d = hash(i.x + 1.0 + (i.y + 1.0) * 57.0, s);
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void vertex() {
	seed = float(INSTANCE_ID);
	age = INSTANCE_CUSTOM.y;  // the particle's age over its lifetime (ParticleProcessMaterial puts it there)
	// face the camera: the camera's axes, scaled by the particle's size, at the particle
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
	MODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0),
			vec4(0.0, length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0),
			vec4(0.0, 0.0, 0.0, 1.0));
}

void fragment() {
	float from_middle = length(UV - 0.5) * 2.0;  // 0 in the middle, 1 at the quad's edge
	// light: pretend the flat puff is a ball facing the camera: its normal from where we are
	// on the disc (view space: x right, y up, z toward the camera), then the froth's toon light
	vec2 on_disc = vec2(UV.x - 0.5, 0.5 - UV.y) * 2.0;
	vec3 ball = vec3(on_disc, sqrt(max(1.0 - dot(on_disc, on_disc), 0.0)));
	vec3 to_sun = normalize((VIEW_MATRIX * vec4(sun_direction, 0.0)).xyz);
	float lit = smoothstep(-0.05, 0.05, dot(normalize(ball), to_sun));
	ALBEDO = puff_color * mix(shade, 1.0, lit);
	float angle = atan(UV.y - 0.5, UV.x - 0.5) + PI;  // round the middle, 0 .. 2 pi
	float edge = 0.9 - lumps * round_noise(angle, seed);  // the edge comes in by up to 0.25
	edge *= mix(0.45, 1.0, sqrt(age));  // it grows: under half size when born, full as it dies
	// it breaks up: in its last 40 %, holes eat in where a blotchy noise is low
	float blotch = blotches(UV * 4.0, seed + 3.0);
	float gone = smoothstep(0.6, 1.0, age);
	edge *= step(gone, blotch * 0.9 + 0.05);  // where the blotch is under how gone it is: a hole
	ALPHA = step(from_middle, edge);  // a lumpy circle
	ALPHA_SCISSOR_THRESHOLD = 0.5;  // a hard cut, no blending
}
