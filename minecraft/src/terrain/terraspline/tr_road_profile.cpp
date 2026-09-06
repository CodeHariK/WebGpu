/**
 * @file tr_road_profile.cpp
 * @brief TerrainSplineRoad: cross-section presets as closed 2-D polygons.
 *
 * Polygons are counter-clockwise in (x right, y up) so the sweep can orient every face outwards.
 * Each corner carries the region of the edge that STARTS there.
 */
#include "tr_road.h"

namespace godot {

namespace {

void add(
		std::vector<TerrainSplineRoad::ProfilePoint> &r_pts,
		float x,
		float y,
		int p_region
) {
	TerrainSplineRoad::ProfilePoint pt;
	pt.pos = Vector2(x, y);
	pt.region = p_region;
	r_pts.push_back(pt);
}

} // namespace

/**
 * @brief Builds the cross-section for the current preset.
 *  SLAB:       underside -> right side (chamfered) -> deck (right to left) -> left side
 *  SLAB_RAILS: same, with a rectangular rail rising from each deck edge
 *  HALF_PIPE:  parabolic deck between two rims, with a parallel underside
 *  CUSTOM:     the Curve2D's baked points; closed when the last point returns to the first
 */
void TerrainSplineRoad::_build_profile(
		std::vector<ProfilePoint> &r_points,
		bool &r_closed
) const {
	r_points.clear();
	r_closed = true;
	const float hw = width * 0.5f;
	const float t = thickness;
	const float r = MIN(edge_radius, MIN(hw * 0.45f, t * 0.9f));

	switch (profile) {
		case PROFILE_SLAB: {
			add(r_points, -hw, -t, REGION_UNDERSIDE);
			add(r_points, hw, -t, REGION_EDGE);
			if (r > 0.0f) {
				add(r_points, hw, -r, REGION_EDGE);
				add(r_points, hw - r, 0.0f, REGION_DECK);
				add(r_points, -hw + r, 0.0f, REGION_EDGE);
				add(r_points, -hw, -r, REGION_EDGE);
			} else {
				add(r_points, hw, 0.0f, REGION_DECK);
				add(r_points, -hw, 0.0f, REGION_EDGE);
			}
		} break;

		case PROFILE_SLAB_RAILS: {
			const float rw = MIN(rail_width, hw * 0.45f);
			const float rh = rail_height;
			add(r_points, -hw, -t, REGION_UNDERSIDE);
			add(r_points, hw, -t, REGION_RAIL); // Right outer side
			add(r_points, hw, rh, REGION_RAIL); // Right rail top
			add(r_points, hw - rw, rh, REGION_RAIL); // Right rail inner face
			add(r_points, hw - rw, 0.0f, REGION_DECK); // Deck, right to left
			add(r_points, -hw + rw, 0.0f, REGION_RAIL); // Left rail inner face
			add(r_points, -hw + rw, rh, REGION_RAIL); // Left rail top
			add(r_points, -hw, rh, REGION_RAIL); // Left outer side, down to the underside
		} break;

		case PROFILE_HALF_PIPE: {
			const int n = MAX(2, pipe_segments);
			// Underside, left to right, parallel to the deck.
			for (int i = 0; i <= n; ++i) {
				const float u = -1.0f + 2.0f * i / n;
				add(r_points, u * hw, pipe_depth * (u * u - 1.0f) - t, i == n ? REGION_EDGE : REGION_UNDERSIDE);
			}
			// Deck, right to left (rims at y = 0, middle at -pipe_depth).
			for (int i = n; i >= 0; --i) {
				const float u = -1.0f + 2.0f * i / n;
				add(r_points, u * hw, pipe_depth * (u * u - 1.0f), i == 0 ? REGION_EDGE : REGION_DECK);
			}
		} break;

		case PROFILE_CUSTOM:
		default: {
			r_closed = false;
			if (cross_section.is_null()) {
				return;
			}
			PackedVector2Array pts = cross_section->get_baked_points();
			if (pts.size() >= 3 && pts[0].distance_to(pts[pts.size() - 1]) < 1e-3f) {
				pts.remove_at(pts.size() - 1);
				r_closed = true;
			}
			for (int i = 0; i < pts.size(); ++i) {
				add(r_points, pts[i].x, pts[i].y, REGION_DECK);
			}
			// Make counter-clockwise so outward orientation works like the presets.
			float area2 = 0.0f;
			for (size_t i = 0, j = r_points.size() - 1; i < r_points.size(); j = i++) {
				area2 += r_points[j].pos.x * r_points[i].pos.y - r_points[i].pos.x * r_points[j].pos.y;
			}
			if (r_closed && area2 < 0.0f) {
				std::reverse(r_points.begin(), r_points.end());
			}
		} break;
	}
}

Color TerrainSplineRoad::_region_color(int p_region) const {
	switch (p_region) {
		case REGION_EDGE:
			return edge_color;
		case REGION_RAIL:
			return rail_color;
		case REGION_UNDERSIDE:
			return underside_color;
		case REGION_DECK:
		default:
			return deck_color;
	}
}

} // namespace godot
