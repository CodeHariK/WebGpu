// Lava, mobile: steps 1 - 5 (liquid_common.gdshaderinc) and its own step 6: dark crust plates
// drifting with the flow, and everything but the crust glows. Matte, opaque.
// MCGrid picks this shader when the map's liquid is lava. Its working copy, and a desktop
// look, are in the Water Shader Steps project (blender/sand_island/water_tutorial/).
shader_type spatial;
render_mode cull_disabled; // falls are seen from both sides

#include "res://material/shaders/terrain/liquid/liquid_common.gdshaderinc"

// Steps 1 - 5: lava's colours and speed.
uniform vec3 deep_color : source_color = vec3(1.0, 0.42, 0.05); // step 1 / 2
uniform vec3 shallow_color : source_color = vec3(0.5, 0.06, 0.02); // step 2: dark red where it cools
uniform float flow_speed = 0.2; // step 3: metres a second: lava creeps
uniform vec3 streak_color : source_color = vec3(1.0, 0.85, 0.3); // step 4: hot yellow
uniform vec3 foam_color : source_color = vec3(1.0, 0.85, 0.35); // step 5: a hot rim and splash

// Step 6: the crust (its colour, plate size, how much of the lava it covers, how wide the cracks
// between plates are, how long one copy drifts) and the glow (the lava's own light; not the crust's).
uniform vec3 crust_color : source_color = vec3(0.13, 0.05, 0.04);
uniform float crust_scale = 0.9; // plates a metre (about)
uniform float crust_cover : hint_range(0.0, 1.0) = 0.85;
uniform float crack_width : hint_range(0.01, 0.4) = 0.08;
uniform float crust_cycle = 5.0; // seconds
uniform float lava_glow = 1.5;

// Step 6: cells (Voronoi): every whole point of the plane gets one plate centre, somewhere random in
// its square. A pixel belongs to the plate whose centre is nearest. The distance to the nearest (d1)
// and to the second nearest (d2) tell how far the pixel is from the line between two plates:
// d2 - d1 is 0 on that line (a crack) and grows toward a plate's middle. Only the 3 x 3 squares
// round the pixel can hold its two nearest centres, so only those are tried.
// Returns 1 on a crusted plate, 0 in a crack or on a plate that isn't crusted: each plate rolls a
// random number (from its square) and is crust when that is below p_cover (the share of plates).
// p_life (0..1, see flowing_crust) is how far through its life this copy of the crust is: each
// plate rolls a second number, and shows while p_life is above it, fading in / out over a moment.
float crust(vec2 p, float p_cover, float p_life) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	float d1 = 8.0;
	float d2 = 8.0;
	vec2 nearest = vec2(0.0); // the nearest plate's square
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 square = vec2(float(x), float(y));
			vec2 to_centre = square + hash2(i + square) - f;
			float d = length(to_centre);
			if (d < d1) {
				d2 = d1;
				d1 = d;
				nearest = i + square;
			} else if (d < d2) {
				d2 = d;
			}
		}
	}
	float plate = smoothstep(crack_width, crack_width * 2.0, d2 - d1); // 1 inside, 0 on a crack
	float crusted = step(hash(nearest + 17.0), p_cover); // this plate: crust or not
	float life = hash(nearest + 41.0); // when, in this copy's life, the plate shows
	float shows = smoothstep(life - 0.08, life + 0.08, p_life);
	return plate * crusted * shows;
}

// Step 6: the crust carried along the flow with the flow-map trick (as flowing_ripples, step 3),
// but not cross-faded: blending two sets of hard-edged plates shows both at half strength (muddy,
// doubled outlines). Instead each copy's life rises 0 -> 1 -> 0 over its cycle, and its plates pop
// in one by one as it rises and melt away one by one as it falls, so every plate on screen is
// solid; a copy restarts at life 0, when none of its plates show. The two copies together: max.
float flowing_crust(vec2 p, vec2 velocity, float t, float p_cover) {
	float phase0 = fract(t / crust_cycle);
	float phase1 = fract(t / crust_cycle + 0.5);
	float life0 = 1.0 - abs(1.0 - 2.0 * phase0); // 0 at copy 0's restart, 1 halfway
	float c0 = crust(p - velocity * phase0 * crust_cycle, p_cover, life0);
	float c1 = crust(p - velocity * phase1 * crust_cycle + vec2(5.3, 2.9), p_cover, 1.0 - life0);
	return max(c0, c1);
}

void vertex() {
	depth = CUSTOM0.x; // step 2: this vertex's depth in metres
}

void fragment() {
	LiquidLook look = LiquidLook(deep_color, shallow_color, flow_speed, streak_color, foam_color);
	LiquidBase base = liquid_base(look, UV, UV2, COLOR, depth, TIME);
	vec3 colour = base.colour;

	// step 6: the share of plates that are crust: most near the edges (shallow, cooler), fewer
	// where it is deep or runs
	float cover = crust_cover * mix(1.0, 0.55, base.deepness) * (1.0 - 0.5 * base.flows);
	// the crust: plates drifting with the flow (the same velocity as the ripples, step 3); none
	// down the falls or on foam
	float crusted = flowing_crust(UV * crust_scale, base.velocity * crust_scale, TIME, cover);
	crusted *= base.on_flat * (1.0 - base.foam);
	colour = mix(colour, crust_color, crusted);

	ALBEDO = colour;
	// the glow: everything but the crust gives off its own light (EMISSION is added on top of the
	// lit colour, so it shows even in shadow): the cracks between plates glow
	EMISSION = colour * lava_glow * (1.0 - crusted);
	ROUGHNESS = 1.0; // matte
	SPECULAR = 0.0;
}
