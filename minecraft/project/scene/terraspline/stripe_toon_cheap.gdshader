shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_toon, specular_toon, skip_vertex_transform;

// ==============================================================
// CUSTOM BOTW PROCEDURAL UNIFORMS
// ==============================================================
group_uniforms Stylized_Terrain;
uniform vec3 botw_color_grass : source_color = vec3(0.4, 0.7, 0.3);
uniform vec3 botw_color_sand  : source_color = vec3(0.8, 0.7, 0.4);
uniform vec3 botw_color_rock  : source_color = vec3(0.5, 0.5, 0.5);
uniform vec3 botw_color_snow  : source_color = vec3(0.95, 0.95, 1.0);

uniform float botw_biome_scale : hint_range(0.0001, 0.01) = 0.001;
uniform float botw_slope_threshold : hint_range(0.0, 1.0) = 0.7;
uniform float botw_slope_blend : hint_range(0.01, 0.2) = 0.05;

uniform float botw_snow_height : hint_range(0.0, 1000.0) = 150.0;
uniform float botw_snow_blend : hint_range(0.1, 50.0) = 10.0;

uniform float botw_stripe_frequency : hint_range(0.01, 5.0) = 0.5;
uniform float botw_stripe_darkness : hint_range(0.0, 1.0) = 0.2;
group_uniforms; // Ends the grouping
// ==============================================================

// Defined Constants
#define SKIP_PASS 0
#define VERTEX_PASS 1
#define FRAGMENT_PASS 2
#define DECODE_HOLE(control) bool(control >>2u & 0x1u)

// Private uniforms (Required for Vertex Heightmap Displacement)
uniform vec3 _camera_pos = vec3(0.f);
uniform float _mesh_size = 48.f;
uniform uint _background_mode = 1u; // NONE = 0, FLAT = 1, NOISE = 2
uniform uint _mouse_layer = 0x80000000u; // Layer 32
uniform float _vertex_spacing = 1.0;
uniform float _vertex_density = 1.0; // = 1./_vertex_spacing
uniform float _region_size = 1024.0;
uniform float _region_texel_size = 0.0009765625; // = 1./region_size
uniform int _region_map_size = 32;
uniform int _region_map[1024];
uniform vec2 _region_locations[1024];
uniform highp sampler2DArray _height_maps : repeat_disable;
uniform highp sampler2DArray _control_maps : repeat_disable;

#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
    #define fma(a, b, c) ((a) * (b) + (c))
#endif

varying float v_vertex_xz_dist;
varying vec3 v_vertex;

////////////////////////
// Vertex
////////////////////////

// Takes in world space XZ (UV) coordinates & search depth
ivec3 get_index_coord(const vec2 uv, const int search) {
	vec2 r_uv = round(uv);
	vec2 o_uv = mod(r_uv,_region_size);
	ivec2 pos;
	int bounds, layer_index = -1;
	for (int i = -1; i < clamp(search, SKIP_PASS, FRAGMENT_PASS); i++) {
		if ((layer_index == -1 && _background_mode == 0u ) || i < 0) {
			r_uv -= i == -1 ? vec2(0.0) : vec2(float(o_uv.x <= o_uv.y), float(o_uv.y <= o_uv.x));
			pos = ivec2(floor((r_uv) * _region_texel_size)) + (_region_map_size / 2);
			bounds = int(uint(pos.x | pos.y) < uint(_region_map_size));
			layer_index = (_region_map[ pos.y * _region_map_size + pos.x ] * bounds - 1);
		}
	}
	return ivec3(ivec2(mod(r_uv,_region_size)), layer_index);
}

// Takes in descaled world to region space XZ coordinates
vec3 get_index_uv(const vec2 uv2) {
	ivec2 pos = ivec2(floor(uv2)) + (_region_map_size / 2);
	int bounds = int(uint(pos.x | pos.y) < uint(_region_map_size));
	int layer_index = _region_map[ pos.y * _region_map_size + pos.x ] * bounds - 1;
	return vec3(uv2 - _region_locations[layer_index], float(layer_index));
}

void vertex() {
	// Get vertex of flat plane in world coordinates and set world UV
	v_vertex = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;

	// Camera distance to vertex on flat plane
	v_vertex_xz_dist = length(v_vertex.xz - _camera_pos.xz);

	// Geomorph vertex, set end and start for linear height interpolate
	float scale = MODEL_MATRIX[0][0];
	float vertex_lerp = smoothstep(0.55, 0.95, (v_vertex_xz_dist / scale - _mesh_size - 4.0) / (_mesh_size - 2.0));
	vec2 v_fract = fract(VERTEX.xz * 0.5) * 2.0;
	// For LOD0 morph from a regular grid to an alternating grid to align with LOD1+
	vec2 shift = (scale < _vertex_spacing + 1e-6) ? // LOD0 or not
		mix(v_fract, vec2(v_fract.x, -v_fract.y), round(fract(round(mod(v_vertex.z * _vertex_density, 4.0)) * round(mod(v_vertex.x * _vertex_density, 4.0)) * 0.25))) :
		v_fract * round((fract(v_vertex.xz * 0.25 / scale) - 0.5) * 4.0);
	vec2 start_pos = v_vertex.xz * _vertex_density;
	vec2 end_pos = (v_vertex.xz - shift * scale) * _vertex_density;
	v_vertex.xz -= shift * scale * vertex_lerp;

	// Discard vertices for Holes
	ivec3 v_region = get_index_coord(start_pos, VERTEX_PASS);
	uint control = floatBitsToUint(texelFetch(_control_maps, v_region, 0)).r;
	bool hole = DECODE_HOLE(control);

	if ( !(CAMERA_VISIBLE_LAYERS == _mouse_layer) && (hole || (_background_mode == 0u && v_region.z == -1))) {
		v_vertex.x = 0. / 0.;
	} else {		
		// Set final vertex height & calculate vertex normals
		ivec3 coord_a = get_index_coord(start_pos, VERTEX_PASS);
		ivec3 coord_b = get_index_coord(end_pos, VERTEX_PASS);
		float h = mix(texelFetch(_height_maps, coord_a, 0).r,texelFetch(_height_maps, coord_b, 0).r,vertex_lerp);
		v_vertex.y = h;
	}

	// Convert model space to view space w/ skip_vertex_transform render mode
	VERTEX = (VIEW_MATRIX * vec4(v_vertex, 1.0)).xyz;
	NORMAL = normalize((MODELVIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	BINORMAL = normalize((MODELVIEW_MATRIX * vec4(BINORMAL, 0.0)).xyz);
	TANGENT = normalize((MODELVIEW_MATRIX * vec4(TANGENT, 0.0)).xyz);
}


////////////////////////
// Fragment
////////////////////////

void fragment() {
	// ==============================================================
	// SMOOTH NORMALS (Bilinear Interpolation)
	// ==============================================================
	vec2 uv = v_vertex.xz * _vertex_density;
	vec2 uv2 = fma(uv, vec2(_region_texel_size), vec2(0.5 * _region_texel_size));
	vec3 region_uv = get_index_uv(uv2);

	const vec3 offsets = vec3(0, 1, 2);
	vec2 index_id = floor(uv);
	vec2 weight = fract(uv);
	vec2 invert = 1.0 - weight;
	vec4 weights = vec4(
		invert.x * weight.y, // 0
		weight.x * weight.y, // 1
		weight.x * invert.y, // 2
		invert.x * invert.y  // 3
	);

	ivec3 index[4];
	index[0] = get_index_coord(index_id + offsets.xy, FRAGMENT_PASS);
	index[1] = get_index_coord(index_id + offsets.yy, FRAGMENT_PASS);
	index[2] = get_index_coord(index_id + offsets.yx, FRAGMENT_PASS);
	index[3] = get_index_coord(index_id + offsets.xx, FRAGMENT_PASS);
	
	float h[4];
	h[3] = texelFetch(_height_maps, index[3], 0).r; // 0 (0,0)
	h[2] = texelFetch(_height_maps, index[2], 0).r; // 1 (1,0)
	h[1] = texelFetch(_height_maps, index[1], 0).r; // 3 (1,1)
	h[0] = texelFetch(_height_maps, index[0], 0).r; // 2 (0,1)

	vec3 index_normal[4];
	index_normal[3] = normalize(vec3(h[3] - h[2], _vertex_spacing, h[3] - h[0]));

	vec3 w_normal = index_normal[3];

	// Only do the smooth math if we are actually rendering the detailed terrain
	vec3 base_ddx = dFdx(v_vertex);
	vec3 base_ddy = dFdy(v_vertex);
	float region_mip = log2(max(length(base_ddx.xz), length(base_ddy.xz)) * _vertex_density);
	
	if (region_mip < 0.0 && region_uv.z > -1.) {
		float h_4 = texelFetch(_height_maps, get_index_coord(index_id + offsets.yz, FRAGMENT_PASS), 0).r; // 4 (1,2)
		float h_5 = texelFetch(_height_maps, get_index_coord(index_id + offsets.zy, FRAGMENT_PASS), 0).r; // 5 (2,1)
		float h_6 = texelFetch(_height_maps, get_index_coord(index_id + offsets.zx, FRAGMENT_PASS), 0).r; // 6 (2,0)
		float h_7 = texelFetch(_height_maps, get_index_coord(index_id + offsets.xz, FRAGMENT_PASS), 0).r; // 7 (0,2)

		index_normal[0] = normalize(vec3(h[0] - h[1], _vertex_spacing, h[0] - h_7));
		index_normal[1] = normalize(vec3(h[1] - h_5, _vertex_spacing, h[1] - h_4));
		index_normal[2] = normalize(vec3(h[2] - h_6, _vertex_spacing, h[2] - h[1]));

		w_normal =
			index_normal[0] * weights[0] +
			index_normal[1] * weights[1] +
			index_normal[2] * weights[2] +
			index_normal[3] * weights[3] ;
	}

	// ==============================================================
	// CUSTOM BOTW PROCEDURAL OVERRIDE 
	// ==============================================================

	vec3 world_pos = v_vertex;
	vec3 world_normal = w_normal; 

	// Fast Macro Biome Noise 
	vec2 p = world_pos.xz * botw_biome_scale; 
	vec2 i = floor(p); 
	vec2 f = fract(p);
	vec2 noise_blend = f * f * (3.0 - 2.0 * f); 
	
	float n00 = fract(sin(dot(i + vec2(0.0, 0.0), vec2(12.9898, 78.233))) * 43758.5453);
	float n10 = fract(sin(dot(i + vec2(1.0, 0.0), vec2(12.9898, 78.233))) * 43758.5453);
	float n01 = fract(sin(dot(i + vec2(0.0, 1.0), vec2(12.9898, 78.233))) * 43758.5453);
	float n11 = fract(sin(dot(i + vec2(1.0, 1.0), vec2(12.9898, 78.233))) * 43758.5453);
	float biome_noise = mix(mix(n00, n10, noise_blend.x), mix(n01, n11, noise_blend.x), noise_blend.y); 

	// Mixing the Biomes (Sand vs Grass)
	vec3 flat_color = mix(botw_color_sand, botw_color_grass, smoothstep(0.4, 0.6, biome_noise));
	
	// Mixing the Slopes (Steep = Rock)
	float slope = dot(normalize(world_normal), vec3(0.0, 1.0, 0.0));
	vec3 surface_color = mix(botw_color_rock, flat_color, smoothstep(botw_slope_threshold - botw_slope_blend, botw_slope_threshold, slope));

	// Altitude (Snow Peaks)
	surface_color = mix(surface_color, botw_color_snow, smoothstep(botw_snow_height, botw_snow_height + botw_snow_blend, world_pos.y));
	
	// Topography Stripes (Strata)
	float stripe = fract(world_pos.y * botw_stripe_frequency);
	surface_color = mix(surface_color, surface_color * (1.0 - botw_stripe_darkness), step(stripe, 0.15));

	// FINAL OUTPUT 
	ALBEDO = surface_color;
	ROUGHNESS = 0.9;
	SPECULAR = 0.0; 
	NORMAL_MAP = vec3(0.5, 0.5, 1.0); 
	NORMAL_MAP_DEPTH = 0.0;
	AO = 1.0; 
	AO_LIGHT_AFFECT = 0.0;
}
