// Wind sway shared by the mood shaders. A vertex bends in the wind direction by the square of its
// height above the ground (trunks barely move, crowns swing), in slow gusts that roll across the
// world (the phase depends on world position, so neighbouring trees don't move in lockstep), with
// a quick small flutter on top. `sway` 0 turns it off for anything that shouldn't move.

uniform float wind = 0.0; // strength 0 … 1 (set every frame by MoodShaders.set_wind)
uniform vec2 wind_direction = vec2(1.0, 0.0);
uniform float sway = 0.0; // 0 = rigid, 1 = sways (trees and leaves)

vec3 wind_offset(vec3 world, float time) {
	float h = max(world.y, 0.0);
	float t = time * (0.8 + wind * 1.4);
	float gust = 0.55 + 0.35 * sin(t * 1.1 + world.x * 0.12 + world.z * 0.09) + 0.15 * sin(t * 2.6 + world.x * 0.37);
	float bend = h * h * 0.004 * wind * gust;
	float flutter = sin(t * 7.0 + world.x * 2.1 + world.y * 3.3 + world.z * 1.7) * 0.015 * wind * min(h, 4.0);
	vec2 push = normalize(wind_direction) * bend;
	return vec3(push.x + flutter, 0.0, push.y + flutter) * sway;
}
