## Bakes a navmesh for the enemy over a level's static colliders, at load time.
##
##   NavBake.add_to(level)   adds a NavigationRegion3D under `level` and bakes it (synchronous,
##                           a few ms for the test level). The navigation map picks it up on the
##                           next physics frame, so ask for paths after that.
##
## Sized for the rolling ball (radius 0.83 m): agent radius 0.5, height 1.5 (so it can't go into
## the crouch tunnel), climbs 0.5 m (stairs). Only static colliders on layer 1 count: loose
## crates (RigidBodies) are ignored and simply get pushed or climbed over.
##
## Step 9 (the real level) will bake this in the editor and save it instead.
extends RefCounted

const GROUP := &"nav_source"


## `max_height`: nothing above this is baked, so the tops of walls and roofs don't get
## unreachable navmesh islands (a route could start on one and never get anywhere).
static func add_to(level: Node3D, max_height := 4.0) -> NavigationRegion3D:
	var nav := NavigationMesh.new()
	nav.cell_size = 0.25  # must match the map (project default)
	nav.cell_height = 0.25
	nav.agent_radius = 0.5
	nav.agent_height = 1.5
	nav.agent_max_climb = 0.5
	nav.agent_max_slope = 45.0
	nav.geometry_parsed_geometry_type = NavigationMesh.PARSED_GEOMETRY_STATIC_COLLIDERS
	nav.geometry_collision_mask = 1
	nav.geometry_source_geometry_mode = NavigationMesh.SOURCE_GEOMETRY_GROUPS_WITH_CHILDREN
	nav.geometry_source_group_name = GROUP
	nav.filter_baking_aabb = AABB(Vector3(-100, -2, -100), Vector3(200, 2 + max_height, 200))
	level.add_to_group(GROUP)
	var region := NavigationRegion3D.new()
	region.navigation_mesh = nav
	level.add_child(region)
	region.bake_navigation_mesh(false)
	return region
