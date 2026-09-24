shader_type spatial;

// =============================================================================
// Folio "Leaves"  (folio World/Leaves.js)
//
// A field of autumn leaves drifting down around the camera, all in one mesh,
// animated entirely in the vertex stage. Folio runs this as a GPU-compute
// particle sim (position/velocity buffers, vehicle push, explosion, terrain
// damping); here we capture the LOOK procedurally like RainLines/WindLines: each
// leaf is a small quad whose origin tiles across a `size`-wide area centred on
// the view, falls on a looping progress, flutters sideways, tumbles (spins on
// two axes), and is tinted brown->orange per leaf.
//
// Time comes from the global shader clock `folio_elapsed_scaled` (so leaves obey
// the 2x world scale + bullet-time), and the drift reads the global wind field
// (`folio_wind_direction` / `folio_wind_strength`) the same way grass does.
//
// Per-vertex data: VERTEX.xz = leaf origin in [0,1]; UV = quad corner in
// [-0.5,0.5]; UV2 = (rnd, rnd2) per-leaf randoms (timing/colour, phase/size).
//
// Deferred vs folio: vehicle push, explosion, terrain-aware damping + floor
// (clamps to y=0 here), and YearCycles-driven density (a plain `amount` gate
// stands in).
// =============================================================================

render_mode cull_disabled;

global uniform float folio_elapsed_scaled;
global uniform vec2 folio_wind_direction;
global uniform float folio_wind_strength;

uniform float size = 80.0;
uniform vec2 center = vec2(0.0);
uniform float elevation = 18.0;
uniform float amount : hint_range(0.0, 1.0) = 0.6; // fraction of leaves shown

uniform float leaf_size = 0.6;
uniform float fall_speed = 0.06;
uniform float flutter_freq = 1.5;
uniform float flutter_amp = 0.6;
uniform float spin_speed = 1.2;
uniform float wind_push = 0.6;

uniform vec3 color_a : source_color = vec3(0.584, 0.318, 0.227); // 0x95513a brown
uniform vec3 color_b : source_color = vec3(0.961, 0.416, 0.227); // 0xf56a3a orange
uniform float ambient : hint_range(0.0, 1.0) = 0.5; // keeps leaves coloured when facing away from the sun
uniform float near_cull = 3.0; // hide leaves closer than this to the camera

vec3 rot_y(vec3 v, float a) {
	float c = cos(a);
	float s = sin(a);
	return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

vec3 rot_x(vec3 v, float a) {
	float c = cos(a);
	float s = sin(a);
	return vec3(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
}

void vertex() {
	float t = folio_elapsed_scaled;
	vec2 corner = UV; // [-0.5, 0.5]
	float rnd = UV2.x;
	float rnd2 = UV2.y;

	// Tile the leaf origin around the view centre (folio loop).
	vec2 o = VERTEX.xz * size - center;
	float hs = size * 0.5;
	o.x = mod(o.x + hs, size) - hs;
	o.y = mod(o.y + hs, size) - hs;
	o += center;

	// Fall: looping progress from `elevation` down to the ground.
	float prog = fract(t * fall_speed * (0.6 + 0.4 * rnd2) + rnd);
	float y = elevation - prog * elevation;

	// Flutter + global wind push (stronger higher up).
	float phase = rnd * TAU;
	float sway_x = sin(t * flutter_freq + phase) * flutter_amp;
	float sway_z = cos(t * flutter_freq * 0.8 + phase) * flutter_amp;
	vec2 wind = folio_wind_direction * (folio_wind_strength * wind_push);
	vec2 pos_xz = o + vec2(sway_x, sway_z) + wind * (y * 0.04);

	// Tumble the quad corner on two axes for a fluttering leaf.
	float ang = t * spin_speed * (0.5 + rnd2) + phase;
	vec3 c = vec3(corner.x, 0.0, corner.y) * (leaf_size * (0.6 + 0.8 * rnd2));
	c = rot_y(c, ang);
	c = rot_x(c, ang * 0.7);

	vec3 world = vec3(pos_xz.x, y, pos_xz.y) + c;

	// Density gate: show a fraction ~= amount; shove the rest far away.
	float show = step(1.0 - amount, fract(rnd * 97.0));
	world.y += (1.0 - show) * 9999.0;

	// Hide leaves that land right on the lens (they'd render as huge dark quads).
	float too_close = step(distance(world, CAMERA_POSITION_WORLD), near_cull);
	world.y += too_close * 9999.0;

	VERTEX = world;
	NORMAL = rot_x(rot_y(vec3(0.0, 1.0, 0.0), ang), ang * 0.7);
	COLOR = vec4(mix(color_a, color_b, fract(rnd * 13.0)), 1.0);
}

void fragment() {
	ALBEDO = COLOR.rgb;
	// Two-sided lighting: flip the normal on back faces.
	if (!FRONT_FACING) {
		NORMAL = -NORMAL;
	}
	ROUGHNESS = 0.9;
	SPECULAR = 0.1;
	// Ambient/bounce floor so a leaf tumbling away from the sun keeps its colour
	// instead of going black (stands in for MeshDefaultMaterial's light bounce).
	EMISSION = ALBEDO * ambient;
}
