#pragma once

#include <cstddef>
#ifdef DELAUNATOR_HEADER_ONLY
#define INLINE inline
#else
#define INLINE
#endif

#include <limits>
#include <ostream>
#include <vector>

#include <iostream>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace delaunator {

constexpr std::size_t INVALID_INDEX =
		(std::numeric_limits<std::size_t>::max)();

class Point {
public:
	Point(float x, float y) :
			m_x(x), m_y(y) {}
	Point() :
			m_x(0), m_y(0) {}

	float x() const { return m_x; }

	float y() const { return m_y; }

	float magnitude2() const { return m_x * m_x + m_y * m_y; }

	static float determinant(const Point &p1, const Point &p2) {
		return p1.m_x * p2.m_y - p1.m_y * p2.m_x;
	}

	static Point vector(const Point &p1, const Point &p2) {
		return Point(p2.m_x - p1.m_x, p2.m_y - p1.m_y);
	}

	static float dist2(const Point &p1, const Point &p2) {
		Point vec = vector(p1, p2);
		return vec.m_x * vec.m_x + vec.m_y * vec.m_y;
	}

	static bool equal(const Point &p1, const Point &p2, float span) {
		float dist = dist2(p1, p2) / span;

		// ABELL - This number should be examined to figure how how
		// it correlates with the breakdown of calculating determinants.
		return dist < 1e-20;
	}

private:
	float m_x;
	float m_y;
};

inline std::ostream &operator<<(std::ostream &out, const Point &p) {
	out << p.x() << "/" << p.y();
	return out;
}

class Points {
public:
	using const_iterator = Point const *;

	Points(const std::vector<float> &coords) :
			m_coords(coords) {}

	const Point &operator[](size_t offset) {
		return reinterpret_cast<const Point &>(
				*(m_coords.data() + (offset * 2)));
	};

	Points::const_iterator begin() const { return reinterpret_cast<const Point *>(m_coords.data()); }
	Points::const_iterator end() const { return reinterpret_cast<const Point *>(
			m_coords.data() + m_coords.size()); }
	size_t size() const { return m_coords.size() / 2; }

private:
	const std::vector<float> &m_coords;
};

class Delaunator {
public:
	std::vector<float> const &coords;
	Points m_points;

	// 'triangles' stores the indices to the 'X's of the input
	// 'coords'.
	std::vector<std::size_t> triangles;

	// 'halfedges' store indices into 'triangles'.  If halfedges[X] = Y,
	// It says that there's an edge from X to Y where a) X and Y are
	// both indices into triangles and b) X and Y are indices into different
	// triangles in the array.  This allows you to get from a triangle to
	// its adjacent triangle.  If the a triangle edge has no adjacent triangle,
	// its half edge will be INVALID_INDEX.
	std::vector<std::size_t> halfedges;

	std::vector<std::size_t> hull_prev;
	std::vector<std::size_t> hull_next;

	// This contains indexes into the triangles array.
	std::vector<std::size_t> hull_tri;
	std::size_t hull_start;

	bool valid = true;

	INLINE Delaunator(std::vector<float> const &in_coords);
	INLINE float get_hull_area();
	INLINE float get_triangle_area();

private:
	std::vector<std::size_t> m_hash;
	Point m_center;
	std::size_t m_hash_size;
	std::vector<std::size_t> m_edge_stack;

	INLINE std::size_t legalize(std::size_t a);
	INLINE std::size_t hash_key(float x, float y) const;
	INLINE std::size_t add_triangle(
			std::size_t i0,
			std::size_t i1,
			std::size_t i2,
			std::size_t a,
			std::size_t b,
			std::size_t c);
	INLINE void link(std::size_t a, std::size_t b);
};

//////////
//////////
//////////
//////////

//@see https://stackoverflow.com/questions/33333363/built-in-mod-vs-custom-mod-function-improve-the-performance-of-modulus-op/33333636#33333636
inline size_t fast_mod(const size_t i, const size_t c) {
	return i >= c ? i % c : i;
}

// Kahan and Babuska summation, Neumaier variant; accumulates less FP error
inline float sum(const std::vector<float> &x) {
	float sum = x[0];
	float err = 0.0;

	for (size_t i = 1; i < x.size(); i++) {
		const float k = x[i];
		const float m = sum + k;
		err += std::fabs(sum) >= std::fabs(k) ? sum - m + k : k - m + sum;
		sum = m;
	}
	return sum + err;
}

inline float dist(
		const float ax,
		const float ay,
		const float bx,
		const float by) {
	const float dx = ax - bx;
	const float dy = ay - by;
	return dx * dx + dy * dy;
}

inline float circumradius(const Point &p1, const Point &p2, const Point &p3) {
	Point d = Point::vector(p1, p2);
	Point e = Point::vector(p1, p3);

	const float bl = d.magnitude2();
	const float cl = e.magnitude2();
	const float det = Point::determinant(d, e);

	Point radius((e.y() * bl - d.y() * cl) * 0.5 / det,
			(d.x() * cl - e.x() * bl) * 0.5 / det);

	if ((bl > 0.0 || bl < 0.0) &&
			(cl > 0.0 || cl < 0.0) &&
			(det > 0.0 || det < 0.0))
		return radius.magnitude2();
	return (std::numeric_limits<float>::max)();
}

inline bool clockwise(const Point &p0, const Point &p1, const Point &p2) {
	Point v0 = Point::vector(p0, p1);
	Point v1 = Point::vector(p0, p2);
	float det = Point::determinant(v0, v1);
	if (det == 0)
		return false;

	float dist = v0.magnitude2() + v1.magnitude2();
	float reldet = std::abs(dist / det);
	if (reldet > 1e14)
		return false;
	return det < 0;
}

inline bool clockwise(float px, float py, float qx, float qy,
		float rx, float ry) {
	Point p0(px, py);
	Point p1(qx, qy);
	Point p2(rx, ry);
	return clockwise(p0, p1, p2);
}

inline bool counterclockwise(const Point &p0, const Point &p1, const Point &p2) {
	Point v0 = Point::vector(p0, p1);
	Point v1 = Point::vector(p0, p2);
	float det = Point::determinant(v0, v1);
	if (det == 0)
		return false;

	float dist = v0.magnitude2() + v1.magnitude2();
	float reldet = std::abs(dist / det);
	if (reldet > 1e14)
		return false;
	return det > 0;
}

inline bool counterclockwise(float px, float py, float qx, float qy,
		float rx, float ry) {
	Point p0(px, py);
	Point p1(qx, qy);
	Point p2(rx, ry);
	return counterclockwise(p0, p1, p2);
}

inline Point circumcenter(
		const float ax,
		const float ay,
		const float bx,
		const float by,
		const float cx,
		const float cy) {
	const float dx = bx - ax;
	const float dy = by - ay;
	const float ex = cx - ax;
	const float ey = cy - ay;

	const float bl = dx * dx + dy * dy;
	const float cl = ex * ex + ey * ey;
	//ABELL - This is suspect for div-by-0.
	const float d = dx * ey - dy * ex;

	const float x = ax + (ey * bl - dy * cl) * 0.5 / d;
	const float y = ay + (dx * cl - ex * bl) * 0.5 / d;

	return Point(x, y);
}

inline bool in_circle(
		const float ax,
		const float ay,
		const float bx,
		const float by,
		const float cx,
		const float cy,
		const float px,
		const float py) {
	const float dx = ax - px;
	const float dy = ay - py;
	const float ex = bx - px;
	const float ey = by - py;
	const float fx = cx - px;
	const float fy = cy - py;

	const float ap = dx * dx + dy * dy;
	const float bp = ex * ex + ey * ey;
	const float cp = fx * fx + fy * fy;

	return (dx * (ey * cp - bp * fy) -
				   dy * (ex * cp - bp * fx) +
				   ap * (ex * fy - ey * fx)) < 0.0;
}

constexpr float EPSILON = std::numeric_limits<float>::epsilon();

inline bool check_pts_equal(float x1, float y1, float x2, float y2) {
	return std::fabs(x1 - x2) <= EPSILON &&
			std::fabs(y1 - y2) <= EPSILON;
}

// monotonically increases with real angle, but doesn't need expensive trigonometry
inline float pseudo_angle(const float dx, const float dy) {
	const float p = dx / (std::abs(dx) + std::abs(dy));
	return (dy > 0.0 ? 3.0 - p : 1.0 + p) / 4.0; // [0..1)
}

inline Delaunator::Delaunator(std::vector<float> const &in_coords) :
		coords(in_coords), m_points(in_coords) {
	std::size_t n = m_points.size();
	if (n < 3) {
		std::cerr << "Can't triangulate fewer than three points." << std::endl;
		valid = false;
		return;
	}

	std::vector<std::size_t> ids(n);
	std::iota(ids.begin(), ids.end(), 0);

	float max_x = std::numeric_limits<float>::lowest();
	float max_y = std::numeric_limits<float>::lowest();
	float min_x = (std::numeric_limits<float>::max)();
	float min_y = (std::numeric_limits<float>::max)();
	for (const Point &p : m_points) {
		min_x = std::min(p.x(), min_x);
		min_y = std::min(p.y(), min_y);
		max_x = std::max(p.x(), max_x);
		max_y = std::max(p.y(), max_y);
	}
	float width = max_x - min_x;
	float height = max_y - min_y;
	float span = width * width + height * height; // Everything is square dist.

	Point center((min_x + max_x) / 2, (min_y + max_y) / 2);

	std::size_t i0 = INVALID_INDEX;
	std::size_t i1 = INVALID_INDEX;
	std::size_t i2 = INVALID_INDEX;

	// pick a seed point close to the centroid
	float min_dist = (std::numeric_limits<float>::max)();
	for (size_t i = 0; i < m_points.size(); ++i) {
		const Point &p = m_points[i];
		const float d = Point::dist2(center, p);
		if (d < min_dist) {
			i0 = i;
			min_dist = d;
		}
	}

	const Point &p0 = m_points[i0];

	min_dist = (std::numeric_limits<float>::max)();

	// find the point closest to the seed
	for (std::size_t i = 0; i < n; i++) {
		if (i == i0)
			continue;
		const float d = Point::dist2(p0, m_points[i]);
		if (d < min_dist && d > 0.0) {
			i1 = i;
			min_dist = d;
		}
	}

	if (i1 == INVALID_INDEX) {
		std::cerr << "All points are duplicates of one another." << std::endl;
		valid = false;
		return;
	}

	const Point &p1 = m_points[i1];

	float min_radius = (std::numeric_limits<float>::max)();

	// find the third point which forms the smallest circumcircle
	// with the first two
	for (std::size_t i = 0; i < n; i++) {
		if (i == i0 || i == i1)
			continue;

		const float r = circumradius(p0, p1, m_points[i]);
		if (r < min_radius) {
			i2 = i;
			min_radius = r;
		}
	}

	if (i2 == INVALID_INDEX) {
		std::cerr << "All points collinear." << std::endl;
		valid = false;
		return;
	}

	const Point &p2 = m_points[i2];

	if (counterclockwise(p0, p1, p2))
		std::swap(i1, i2);

	float i0x = p0.x();
	float i0y = p0.y();
	float i1x = m_points[i1].x();
	float i1y = m_points[i1].y();
	float i2x = m_points[i2].x();
	float i2y = m_points[i2].y();

	m_center = circumcenter(i0x, i0y, i1x, i1y, i2x, i2y);

	// Calculate the distances from the center once to avoid having to
	// calculate for each compare.  This used to be done in the comparator,
	// but GCC 7.5+ would copy the comparator to iterators used in the
	// sort, and this was excruciatingly slow when there were many points
	// because you had to copy the vector of distances.
	std::vector<float> dists;
	dists.reserve(m_points.size());
	for (const Point &p : m_points)
		dists.push_back(dist(p.x(), p.y(), m_center.x(), m_center.y()));

	// sort the points by distance from the seed triangle circumcenter
	std::sort(ids.begin(), ids.end(),
			[&dists](std::size_t i, std::size_t j) { return dists[i] < dists[j]; });

	// initialize a hash table for storing edges of the advancing convex hull
	// 1.618 is the golden ratio.
	m_hash_size = static_cast<std::size_t>(1.618 * std::ceil(std::sqrt(n)));
	m_hash.resize(m_hash_size);
	std::fill(m_hash.begin(), m_hash.end(), INVALID_INDEX);

	// initialize arrays for tracking the edges of the advancing convex hull
	hull_prev.resize(n);
	hull_next.resize(n);
	hull_tri.resize(n);

	hull_start = i0;

	hull_next[i0] = hull_prev[i2] = i1;
	hull_next[i1] = hull_prev[i0] = i2;
	hull_next[i2] = hull_prev[i1] = i0;

	hull_tri[i0] = 0;
	hull_tri[i1] = 1;
	hull_tri[i2] = 2;

	m_hash[hash_key(i0x, i0y)] = i0;
	m_hash[hash_key(i1x, i1y)] = i1;
	m_hash[hash_key(i2x, i2y)] = i2;

	// ABELL - Why are we doing this is n < 3?  There is no triangulation if
	//  there is no triangle.

	std::size_t max_triangles = n < 3 ? 1 : 2 * n - 5;
	triangles.reserve(max_triangles * 3);
	halfedges.reserve(max_triangles * 3);
	add_triangle(i0, i1, i2, INVALID_INDEX, INVALID_INDEX, INVALID_INDEX);
	if (!valid) {
		return;
	}
	float xp = std::numeric_limits<float>::quiet_NaN();
	float yp = std::numeric_limits<float>::quiet_NaN();

	// Go through points based on distance from the center.
	for (std::size_t k = 0; k < n; k++) {
		const std::size_t i = ids[k];
		const float x = coords[2 * i];
		const float y = coords[2 * i + 1];

		// skip near-duplicate points
		if (k > 0 && check_pts_equal(x, y, xp, yp))
			continue;
		xp = x;
		yp = y;

		//ABELL - This is dumb.  We have the indices.  Use them.
		// skip seed triangle points
		if (check_pts_equal(x, y, i0x, i0y) ||
				check_pts_equal(x, y, i1x, i1y) ||
				check_pts_equal(x, y, i2x, i2y))
			continue;

		// find a visible edge on the convex hull using edge hash
		std::size_t start = 0;

		size_t key = hash_key(x, y);
		for (size_t j = 0; j < m_hash_size; j++) {
			start = m_hash[fast_mod(key + j, m_hash_size)];

			// ABELL - Not sure how hull_next[start] could ever equal start
			// I *think* hull_next is just a representation of the hull in one
			// direction.
			if (start != INVALID_INDEX && start != hull_next[start])
				break;
		}

		//ABELL
		// Make sure what we found is on the hull.
		assert(hull_prev[start] != start);
		assert(hull_prev[start] != INVALID_INDEX);

		start = hull_prev[start];
		size_t e = start;
		size_t q;

		// Advance until we find a place in the hull where our current point
		// can be added.
		while (true) {
			q = hull_next[e];
			if (Point::equal(m_points[i], m_points[e], span) ||
					Point::equal(m_points[i], m_points[q], span)) {
				e = INVALID_INDEX;
				break;
			}
			if (counterclockwise(x, y, coords[2 * e], coords[2 * e + 1],
						coords[2 * q], coords[2 * q + 1]))
				break;
			e = q;
			if (e == start) {
				e = INVALID_INDEX;
				break;
			}
		}

		// ABELL
		// This seems wrong.  Perhaps we should check what's going on?
		if (e == INVALID_INDEX) // likely a near-duplicate point; skip it
			continue;

		// add the first triangle from the point
		std::size_t t = add_triangle(
				e,
				i,
				hull_next[e],
				INVALID_INDEX,
				INVALID_INDEX,
				hull_tri[e]);

		if (!valid) {
			return;
		}

		hull_tri[i] = legalize(t + 2); // Legalize the triangle we just added.
		hull_tri[e] = t;

		// walk forward through the hull, adding more triangles and
		// flipping recursively
		std::size_t next = hull_next[e];
		while (true) {
			q = hull_next[next];
			if (!counterclockwise(x, y, coords[2 * next], coords[2 * next + 1],
						coords[2 * q], coords[2 * q + 1]))
				break;
			t = add_triangle(next, i, q,
					hull_tri[i], INVALID_INDEX, hull_tri[next]);
			if (!valid) {
				return;
			}
			hull_tri[i] = legalize(t + 2);
			hull_next[next] = next; // mark as removed
			next = q;
		}

		// walk backward from the other side, adding more triangles and flipping
		if (e == start) {
			while (true) {
				q = hull_prev[e];
				if (!counterclockwise(x, y, coords[2 * q], coords[2 * q + 1],
							coords[2 * e], coords[2 * e + 1]))
					break;
				t = add_triangle(q, i, e,
						INVALID_INDEX, hull_tri[e], hull_tri[q]);
				if (!valid) {
					return;
				}
				legalize(t + 2);
				hull_tri[q] = t;
				hull_next[e] = e; // mark as removed
				e = q;
			}
		}

		// update the hull indices
		hull_prev[i] = e;
		hull_start = e;
		hull_prev[next] = i;
		hull_next[e] = i;
		hull_next[i] = next;

		m_hash[hash_key(x, y)] = i;
		m_hash[hash_key(coords[2 * e], coords[2 * e + 1])] = e;
	}
}

inline float Delaunator::get_hull_area() {
	std::vector<float> hull_area;
	size_t e = hull_start;
	do {
		hull_area.push_back((coords[2 * e] - coords[2 * hull_prev[e]]) *
				(coords[2 * e + 1] + coords[2 * hull_prev[e] + 1]));
		e = hull_next[e];
	} while (e != hull_start);
	return sum(hull_area);
}

inline float Delaunator::get_triangle_area() {
	std::vector<float> vals;
	for (size_t i = 0; i < triangles.size(); i += 3) {
		const float ax = coords[2 * triangles[i]];
		const float ay = coords[2 * triangles[i] + 1];
		const float bx = coords[2 * triangles[i + 1]];
		const float by = coords[2 * triangles[i + 1] + 1];
		const float cx = coords[2 * triangles[i + 2]];
		const float cy = coords[2 * triangles[i + 2] + 1];
		//ABELL - Is this right? It looks like a cross-product, which would give you twice the area.
		//  Test.
		float val = std::fabs((by - ay) * (cx - bx) - (bx - ax) * (cy - by));
		vals.push_back(val);
	}
	return sum(vals);
}

inline std::size_t Delaunator::legalize(std::size_t a) {
	std::size_t i = 0;
	std::size_t ar = 0;
	m_edge_stack.clear();

	// recursion eliminated with a fixed-size stack
	while (true) {
		const size_t b = halfedges[a];

		/* if the pair of triangles doesn't satisfy the Delaunay condition
		 * (p1 is inside the circumcircle of [p0, pl, pr]), flip them,
		 * then do the same check/flip recursively for the new pair of triangles
		 *
		 *           pl                    pl
		 *          /||\                  /  \
		 *       al/ || \bl            al/    \a
		 *        /  ||  \              /      \
		 *       /  a||b  \    flip    /___ar___\
		 *     p0\   ||   /p1   =>   p0\---bl---/p1
		 *        \  ||  /              \      /
		 *       ar\ || /br             b\    /br
		 *          \||/                  \  /
		 *           pr                    pr
		 */
		const size_t a0 = 3 * (a / 3);
		ar = a0 + (a + 2) % 3;

		if (b == INVALID_INDEX) {
			if (i > 0) {
				i--;
				a = m_edge_stack[i];
				continue;
			} else {
				//i = INVALID_INDEX;
				break;
			}
		}

		const size_t b0 = 3 * (b / 3);
		const size_t al = a0 + (a + 1) % 3;
		const size_t bl = b0 + (b + 2) % 3;

		const std::size_t p0 = triangles[ar];
		const std::size_t pr = triangles[a];
		const std::size_t pl = triangles[al];
		const std::size_t p1 = triangles[bl];

		const bool illegal = in_circle(
				coords[2 * p0],
				coords[2 * p0 + 1],
				coords[2 * pr],
				coords[2 * pr + 1],
				coords[2 * pl],
				coords[2 * pl + 1],
				coords[2 * p1],
				coords[2 * p1 + 1]);

		if (illegal) {
			triangles[a] = p1;
			triangles[b] = p0;

			auto hbl = halfedges[bl];

			// Edge swapped on the other side of the hull (rare).
			// Fix the halfedge reference
			if (hbl == INVALID_INDEX) {
				std::size_t e = hull_start;
				do {
					if (hull_tri[e] == bl) {
						hull_tri[e] = a;
						break;
					}
					e = hull_prev[e];
				} while (e != hull_start);
			}
			link(a, hbl);
			link(b, halfedges[ar]);
			link(ar, bl);
			std::size_t br = b0 + (b + 1) % 3;

			if (i < m_edge_stack.size()) {
				m_edge_stack[i] = br;
			} else {
				m_edge_stack.push_back(br);
			}
			i++;

		} else {
			if (i > 0) {
				i--;
				a = m_edge_stack[i];
				continue;
			} else {
				break;
			}
		}
	}
	return ar;
}

inline std::size_t Delaunator::hash_key(const float x, const float y) const {
	const float dx = x - m_center.x();
	const float dy = y - m_center.y();
	return fast_mod(
			static_cast<std::size_t>(std::llround(std::floor(pseudo_angle(dx, dy) * static_cast<float>(m_hash_size)))),
			m_hash_size);
}

inline std::size_t Delaunator::add_triangle(
		std::size_t i0,
		std::size_t i1,
		std::size_t i2,
		std::size_t a,
		std::size_t b,
		std::size_t c) {
	std::size_t t = triangles.size();
	triangles.push_back(i0);
	triangles.push_back(i1);
	triangles.push_back(i2);
	link(t, a);
	if (!valid) {
		return INVALID_INDEX;
	}
	link(t + 1, b);
	if (!valid) {
		return INVALID_INDEX;
	}
	link(t + 2, c);
	if (!valid) {
		return INVALID_INDEX;
	}

	return t;
}

inline void Delaunator::link(const std::size_t a, const std::size_t b) {
	std::size_t s = halfedges.size();
	if (a == s) {
		halfedges.push_back(b);
	} else if (a < s) {
		halfedges[a] = b;
	} else {
		std::cerr << "Cannot link edge." << std::endl;
		valid = false;
		return;
	}
	if (b != INVALID_INDEX) {
		std::size_t s2 = halfedges.size();
		if (b == s2) {
			halfedges.push_back(a);
		} else if (b < s2) {
			halfedges[b] = a;
		} else {
			std::cerr << "Cannot link edge." << std::endl;
			valid = false;
			return;
		}
	}
}

} //namespace delaunator

#undef INLINE
