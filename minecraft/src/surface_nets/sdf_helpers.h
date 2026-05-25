#ifndef SDF_HELPERS_H
#define SDF_HELPERS_H

#include <algorithm>
#include <cmath>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

// Maps a binary corner index (0-7) to the corresponding C++ corner offset index (0-7)
// as defined in the `corner_offsets` array:
// c0 = (0,0,1) -> 0b100 = 4
// c1 = (1,0,1) -> 0b101 = 5
// c2 = (1,0,0) -> 0b001 = 1
// c3 = (0,0,0) -> 0b000 = 0
// c4 = (0,1,1) -> 0b110 = 6
// c5 = (1,1,1) -> 0b111 = 7
// c6 = (1,1,0) -> 0b011 = 3
// c7 = (0,1,0) -> 0b010 = 2
inline const int BINARY_TO_CPP_CORNER[8] = { 3, 2, 7, 6, 0, 1, 4, 5 };

inline Vector3 vec3_mul(const Vector3 &a, const Vector3 &b) {
	return Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
}

// Compute normal as the gradient of the distance field using bilinear interpolation
// between the differences along the 12 edges based on the surface intersection point s.
// dists are ordered in binary corner layout.
inline Vector3 sdf_gradient(const float dists[8], const Vector3 &s) {
	Vector3 p00(dists[0b001], dists[0b010], dists[0b100]);
	Vector3 n00(dists[0b000], dists[0b000], dists[0b000]);

	Vector3 p10(dists[0b101], dists[0b011], dists[0b110]);
	Vector3 n10(dists[0b100], dists[0b001], dists[0b010]);

	Vector3 p01(dists[0b011], dists[0b110], dists[0b101]);
	Vector3 n01(dists[0b010], dists[0b100], dists[0b001]);

	Vector3 p11(dists[0b111], dists[0b111], dists[0b111]);
	Vector3 n11(dists[0b110], dists[0b101], dists[0b011]);

	Vector3 d00 = p00 - n00;
	Vector3 d10 = p10 - n10;
	Vector3 d01 = p01 - n01;
	Vector3 d11 = p11 - n11;

	Vector3 neg(1.0f - s.x, 1.0f - s.y, 1.0f - s.z);

	Vector3 neg_yzx(neg.y, neg.z, neg.x);
	Vector3 neg_zxy(neg.z, neg.x, neg.y);
	Vector3 s_yzx(s.y, s.z, s.x);
	Vector3 s_zxy(s.z, s.x, s.y);

	Vector3 term1 = vec3_mul(vec3_mul(neg_yzx, neg_zxy), d00);
	Vector3 term2 = vec3_mul(vec3_mul(neg_yzx, s_zxy), d10);
	Vector3 term3 = vec3_mul(vec3_mul(s_yzx, neg_zxy), d01);
	Vector3 term4 = vec3_mul(vec3_mul(s_yzx, s_zxy), d11);

	return term1 + term2 + term3 + term4;
}

// Geometric SDF Shape Classes
class SDFSphere {
private:
	Vector3 center;
	float radius = 0.0f;

public:
	SDFSphere(const Vector3 &p_center, float p_radius) : center(p_center), radius(p_radius) {}

	inline float sample(const Vector3 &p) const {
		return p.distance_to(center) - radius;
	}
};

class SDFBox {
private:
	Vector3 center;
	Vector3 half_size;

public:
	SDFBox(const Vector3 &p_center, const Vector3 &p_half_size) : center(p_center), half_size(p_half_size) {}

	// Fast approximation (Chebyshev)
	// Good for cheap intersection tests or broad-phase masking
	inline float sample_approx(const Vector3 &p) const {
		float dx = std::abs(p.x - center.x) - half_size.x;
		float dy = std::abs(p.y - center.y) - half_size.y;
		float dz = std::abs(p.z - center.z) - half_size.z;
		return std::max(dx, std::max(dy, dz));
	}

	// Exact Euclidean distance
	// Required for perfect meshing and accurate normal gradients
	inline float sample_exact(const Vector3 &p) const {
		// 1. Translate the point to local space and mirror it to the positive quadrant
		Vector3 local_p(
				std::abs(p.x - center.x),
				std::abs(p.y - center.y),
				std::abs(p.z - center.z));

		// 2. Find the distance from the point to the box's surface on each axis
		Vector3 d = local_p - half_size;

		// 3. Calculate the distance if the point is OUTSIDE the box
		// We clamp the negative values to 0, then take the physical length of the vector
		Vector3 max_d(
				std::max(d.x, 0.0f),
				std::max(d.y, 0.0f),
				std::max(d.z, 0.0f));
		float outside_dist = max_d.length();

		// 4. Calculate the distance if the point is INSIDE the box
		// We find the closest surface (the max component) and clamp positive values to 0
		float inside_dist = std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f);

		// 5. Combine them. If inside, outside_dist is 0. If outside, inside_dist is 0.
		return outside_dist + inside_dist;
	}
};

inline float sdf_union(float d1, float d2) {
	return std::min(d1, d2);
}

} // namespace godot

#endif // SDF_HELPERS_H
