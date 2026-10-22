## Toxic Factory layout: where everything goes. Godot axes: X right, Z toward the camera, Y up.
##
##   river     runs along X in front (z ≈ 23, wiggling), flowing +X, its water below the banks
##   road      along X between the river and the factory (two lanes), lined with jersey barriers
##   compound  fenced yard behind the road, gates in its front fence at x = -14 (in) and x = 18 (out)
##   buildings plant, annex, dock, chimneys, cooling tower, tanks inside the compound
##   pipes     leave the plant, cross over the road on supports and end at outfalls in the bank,
##             pouring sludge into the river
##   trucks    route A: road → in gate → past the dock (stop) → out gate → road
##             route B: straight along the far lane the other way
extends RefCounted

const RIVER_Z := 23.0
const RIVER_HALF := 5.0  # half width of the water
const BANK := 3.0  # horizontal run of the bank slope
const WATER_Y := -1.4
const BED_Y := -2.8
const MAP_HALF := 95.0

const ROAD_Z0 := 3.0
const ROAD_Z1 := 9.0
const LANE_IN := 7.5  # +X lane (near the river)
const LANE_OUT := 4.5  # -X lane

const COMPOUND := Rect2(-26, -34, 52, 35)  # x, z, width, depth (front fence at z = 1)
const GATE_IN_X := -14.0
const GATE_OUT_X := 18.0
const GATE_WIDTH := 7.0
const YARD_LANE_Z := -2.5  # where trucks drive inside the yard

## Kit piece placements: [piece, x, z, yaw degrees]
const BUILDINGS := [
	["Plant_main", 0.0, -13.0, 0.0],
	["Plant_annex", -16.0, -9.0, 0.0],
	["Dock", 14.0, -13.0, 0.0],
	["Chimney", -5.0, -22.0, 0.0],
	["Chimney", 0.5, -23.5, 0.0],
	["CoolingTower", -17.0, -25.0, 0.0],
	["Tank_toxic", 16.0, -25.0, 0.0],
	["Tank_hazard", 21.5, -28.5, 0.0],
	["Tank_toxic", 10.0, -29.0, 0.0],
]
const CHIMNEY_TOP := 16.3
const TOWER_TOP := 10.6
const DOCK_STOP_X := 14.0  # route A trucks pause here, beside the dock

## Pipes: polylines (x, y, z). Each outfall pipe ends at the riverbank with an Outfall mouth.
const PIPE_RADIUS := 0.45
const PIPE_HEIGHT := 7.5  # over the road (trucks are 4.3 m tall)
const OUTFALL_X := [-4.0, 4.0]
const FEED_PIPE := [  # tank → back of the plant, low on supports
	Vector3(16, 2.6, -22.6), Vector3(16, 2.6, -20.5), Vector3(4, 2.6, -20.5), Vector3(4, 2.6, -17.8),
]
## The water surface reaches the banks where they cross WATER_Y (a bit wider than RIVER_HALF).
const WATER_HALF := 6.6


static func river_z(x: float) -> float:
	return RIVER_Z + 2.5 * sin(x * 0.05) + 1.0 * sin(x * 0.13 + 1.0)


## The near (road-side) edge of the water at x.
static func river_near(x: float) -> float:
	return river_z(x) - RIVER_HALF


## Ground height: 0 on land, sloping down the banks to the riverbed.
static func ground_y(x: float, z: float) -> float:
	var d := absf(z - river_z(x))
	if d <= RIVER_HALF:
		return BED_Y
	if d >= RIVER_HALF + BANK:
		return 0.0
	return lerpf(BED_Y, 0.0, smoothstep(RIVER_HALF, RIVER_HALF + BANK, d))


## Pipe route from the plant, over the road, down to an outfall in the bank at x.
static func outfall_pipe(x: float) -> Array[Vector3]:
	var top := river_near(x) - BANK  # where the bank starts to drop
	return [
		Vector3(x, 6.3, -8.4), Vector3(x, PIPE_HEIGHT, -6.5), Vector3(x, PIPE_HEIGHT, 11.0),
		Vector3(x, 0.6, 11.0), Vector3(x, 0.6, top + 0.6),
	]


## Where the sludge leaves the outfall pipe (x, y, z), facing +Z.
static func outfall_mouth(x: float) -> Vector3:
	return outfall_pipe(x)[-1]


## Route A: in from the left, through the in gate, past the dock, out the out gate, off to the right.
static func route_a() -> PackedVector3Array:
	return PackedVector3Array([
		Vector3(-MAP_HALF, 0, LANE_IN), Vector3(GATE_IN_X - 9, 0, LANE_IN), Vector3(GATE_IN_X - 2, 0, LANE_IN - 2.5),
		Vector3(GATE_IN_X, 0, 1.0), Vector3(GATE_IN_X + 2, 0, YARD_LANE_Z), Vector3(GATE_IN_X + 8, 0, YARD_LANE_Z),
		Vector3(DOCK_STOP_X, 0, YARD_LANE_Z), Vector3(GATE_OUT_X - 2, 0, YARD_LANE_Z), Vector3(GATE_OUT_X, 0, 1.0),
		Vector3(GATE_OUT_X + 2, 0, LANE_IN - 1.0), Vector3(GATE_OUT_X + 8, 0, LANE_IN), Vector3(MAP_HALF, 0, LANE_IN),
	])


## Route B: the far lane, right to left, never stopping.
static func route_b() -> PackedVector3Array:
	return PackedVector3Array([Vector3(MAP_HALF, 0, LANE_OUT), Vector3(-MAP_HALF, 0, LANE_OUT)])
