## The hall's neon colours, shared by the floor, the lights and the trim.
class_name DiscoPalette
extends RefCounted

const MAGENTA := Color(1.0, 0.12, 0.72)
const CYAN := Color(0.1, 0.85, 1.0)
const PURPLE := Color(0.55, 0.2, 1.0)
const YELLOW := Color(1.0, 0.82, 0.15)
const LIME := Color(0.45, 1.0, 0.3)
const WARN := Color(1.0, 0.28, 0.05) ## a tile about to be hit
const ALL := [MAGENTA, CYAN, PURPLE, YELLOW, LIME]


## Colour `index` of the palette, wrapping.
static func pick(index: int) -> Color:
	return ALL[posmod(index, ALL.size())]


## An emissive material (trim, strips, signs): `energy` × colour glow, dark body.
static func neon(color: Color, energy := 3.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color.darkened(0.6)
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
	return material


## A plain surface material.
static func surface(color: Color, roughness := 0.5, metallic := 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metallic
	return material
