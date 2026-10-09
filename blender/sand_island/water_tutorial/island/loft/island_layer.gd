## One "sheet" draped over (or wrapped around) the island body — a grass cap, a band of icing.
## It follows the plateau outline but is wider (overhang), has its own thickness, rounded edges
## (bevel) and an optional wavy, drippy bottom edge. Stack several for a layered ice-cream look.
##
##        ___________________          ← top (flat, or only a rim if cap = false)
##       /  bevel                \
##      |  thickness              |    ← side, `overhang` metres outside the body
##       \__  ~ drips ~       ___/     ← bottom edge, rounded by bottom_bevel
##          \______________ /          ← underside tucks back inside the body (hidden)
class_name IslandLayer
extends RefCounted

var top := 1.75             # height of the layer's top surface
var thickness := 0.35       # top to bottom edge (before drips)
var overhang := 0.25        # how far it sticks out past the body (metres)
var bevel := 0.15           # radius of the rounded top edge (0 = sharp 90° corner)
var bottom_bevel := 0.1     # radius of the rounded bottom edge
var drip := 0.08            # how far the bottom edge dips down in places (metres)
var colour := Color(0.45, 0.74, 0.3)
var cap := true             # true: closed top (covers the plateau); false: a band around the side
var segments := 4           # rings per bevel (more = rounder)


func _init(p_top: float, p_thickness: float, p_overhang: float, p_bevel: float, p_colour: Color, p_cap: bool) -> void:
	top = p_top
	thickness = p_thickness
	overhang = p_overhang
	bevel = p_bevel
	bottom_bevel = minf(p_bevel, p_thickness * 0.4)
	colour = p_colour
	cap = p_cap


## Ready-made stacks: 0 none, 1 grass sheet, 2 layered ice cream.
static func preset(index: int, plateau_height: float) -> Array[IslandLayer]:
	var layers: Array[IslandLayer] = []
	match index:
		1:
			layers.append(IslandLayer.new(plateau_height + 0.08, 0.38, 0.25, 0.15, Color(0.45, 0.74, 0.3), true))
		2:
			layers.append(IslandLayer.new(plateau_height + 0.1, 0.32, 0.3, 0.16, Color(0.95, 0.88, 0.76), true))   # vanilla top
			var pink := IslandLayer.new(plateau_height - 0.35, 0.34, 0.2, 0.15, Color(0.96, 0.6, 0.72), false)
			pink.drip = 0.12
			layers.append(pink)
			var choc := IslandLayer.new(plateau_height - 0.8, 0.32, 0.12, 0.14, Color(0.55, 0.33, 0.22), false)
			choc.drip = 0.05
			layers.append(choc)
	return layers


const PRESET_NAMES := ["no layers", "grass sheet", "layered ice cream"]
