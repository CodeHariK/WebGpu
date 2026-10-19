## A place that asks the camera for something different, e.g.
##   "vista"     pull back, wider and higher: a big view opens up
##   "corridor"  close and low: a tight, tense stretch
## `overrides` holds the MoodCamera numbers to change (fov, distance, height, pitch, lag, sway,
## roll, dof_far, dof_near, aberration). Its pull is full inside `radius` and fades to nothing over
## `feather` metres beyond it; MoodCamera eases the weight so walking in and out glides.
## A flat ring on the ground shows it (visible with MoodDemo's zone debug).
class_name MoodCameraZone
extends Node3D

var zone_name := "zone"
var radius := 5.0
var feather := 4.0
var overrides := {}
var weight := 0.0 ## eased by MoodCamera


static func create(parent: Node3D, label: String, at: Vector3, zone_radius: float, values: Dictionary, color: Color) -> MoodCameraZone:
	var zone := MoodCameraZone.new()
	zone.name = "Zone_" + label
	zone.zone_name = label
	zone.radius = zone_radius
	zone.overrides = values
	parent.add_child(zone)
	zone.global_position = at
	var ring := TorusMesh.new()
	ring.inner_radius = zone_radius - 0.12
	ring.outer_radius = zone_radius
	ring.rings = 48
	ring.ring_segments = 4
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	material.no_depth_test = true
	var instance := MeshInstance3D.new()
	instance.name = "Ring"
	instance.mesh = ring
	instance.material_override = material
	instance.scale = Vector3(1, 0.2, 1)
	instance.position.y = 0.1
	instance.visible = false
	zone.add_child(instance)
	return zone


## 1 inside the radius, fading to 0 across the feather (flat distance).
func influence(point: Vector3) -> float:
	var distance := ((point - global_position) * Vector3(1, 0, 1)).length()
	return 1.0 - smoothstep(radius, radius + feather, distance)


func show_ring(on: bool) -> void:
	(get_node("Ring") as Node3D).visible = on
