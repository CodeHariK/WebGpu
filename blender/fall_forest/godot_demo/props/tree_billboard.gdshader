// A far tree (TreeBillboards): a flat, opaque shape turned about its upright to face the camera.
//   turning    the instance's position and size are kept, its own turn is dropped: the shape's
//              x goes across the view, its z (the blobs' layering) toward the camera, its y up,
//              leaning back toward the camera by `lean` when seen from above (else from a hill
//              a flat tree turns to a sliver); its trunk's foot stays on the ground
//   colour     the vertex colours, unlit (shading painted in), each tree a little lighter or
//              darker and warmer or cooler (a hash of its position)
// Opaque, so the depth test hides what is behind it; it casts no shadow (it is past the shadow
// distance); fog still applies.
shader_type spatial;
render_mode unshaded, cull_disabled, world_vertex_coords;

uniform float brightness = 1.0; // to sit with the lit 3D trees near it
uniform float lean = 0.6; // 0 always upright .. 1 always square on to the camera
uniform float variation = 0.15;

varying vec3 tint;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void vertex() {
	vec3 root = MODEL_MATRIX[3].xyz;
	mat3 basis = mat3(MODEL_MATRIX);
	vec3 local = inverse(basis) * (VERTEX - root); // the shape as built
	float size = length(basis[1]);
	vec3 to_camera = normalize(CAMERA_POSITION_WORLD - root);
	vec3 flat_to_camera = normalize(vec3(to_camera.x, 0.0, to_camera.z) + vec3(0.0001, 0.0, 0.0));
	vec3 across = vec3(flat_to_camera.z, 0.0, -flat_to_camera.x);
	vec3 up = normalize(mix(vec3(0.0, 1.0, 0.0), cross(to_camera, across), lean));
	vec3 facing = normalize(cross(across, up));
	VERTEX = root + (across * local.x + up * local.y + facing * local.z) * size;
	float r = hash(root.xz);
	tint = vec3(1.0 + variation * (r - 0.5), 1.0, 1.0 - variation * (r - 0.5)) * mix(1.0 - variation, 1.0 + variation, hash(root.zx));
}

void fragment() {
	ALBEDO = COLOR.rgb * tint * brightness;
}
