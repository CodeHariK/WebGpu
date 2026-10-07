## A cartoon bat's body, built in code: a fuzzy egg body, a big head (ears, eyes, pink nose,
## two tiny fangs), two wings (BatWing) and two little legs. Faces −Z, sides are left (−X) and
## right (+X).
##
## No Skeleton3D: each arm (shoulder → elbow → wrist) and each leg (hip → knee → foot) is a
## two-bone LimbChain, and every limb and finger is drawn by one LimbRenderer. Bat drives it each
## frame: pose_wings() and the feet targets, then solve() — which solves the chains, lets the
## wings place their fingers and rebuild their membranes, and draws. Everything is in step in
## the same frame, no skeleton callbacks.
class_name BatRig
extends Node3D

const SHOULDER := Vector3(0.07, 0.035, -0.03) ## right shoulder (left is mirrored)
const HIP := Vector3(0.045, -0.045, 0.1) ## right hip
const MEMBRANE_HIP := Vector3(0.055, -0.01, 0.07) ## where the right membrane meets the body
const UPPER_ARM := 0.15
const FOREARM := 0.2
const THIGH := 0.055
const SHIN := 0.06
const TUCKED_FOOT := Vector3(0.04, -0.05, 0.2) ## right foot while flying, trailing behind
const ARM_RADIUS := 0.011
const LEG_RADIUS := 0.01

var fur_color := Color(0.45, 0.32, 0.34)
var belly_color := Color(0.62, 0.48, 0.45)
var membrane_color := Color(0.38, 0.23, 0.42)
var inner_ear_color := Color(0.95, 0.6, 0.65)

var head: Node3D ## turn it to look
var wings: Array[BatWing] = [] ## left, right
var feet: Array[Marker3D] = [] ## leg targets, left, right (place in world space)
var arms: Array[LimbChain] = [] ## left, right: shoulder → elbow → wrist
var legs: Array[LimbChain] = [] ## left, right: hip → knee → foot
var renderer: LimbRenderer

var _knee_poles: Array[Marker3D] = []


func _ready() -> void:
	renderer = LimbRenderer.new()
	renderer.name = "Limbs"
	renderer.roughness = 0.8
	add_child(renderer)
	for i in 2:
		_build_side(i)
	_build_body()


## Pose both wings the same: `wrist` in right-wing coordinates relative to the shoulder
## (BatWing.wrist_offset), `spread` 0 folded … 1 splayed fingers.
func pose_wings(wrist: Vector3, spread: float) -> void:
	for wing in wings:
		wing.stroke(_mirror(SHOULDER, wing.side), wrist, spread)


## Where foot `i` (0 left, 1 right) tucks while flying, in world space.
func tucked_foot(i: int) -> Vector3:
	return to_global(_mirror(TUCKED_FOOT, _side(i)))


## Solve every limb toward its target, then fingers + membranes, then draw. Call once per frame
## after moving the body, the wing strokes and the feet.
func solve() -> void:
	for i in 2:
		var side := _side(i)
		var arm := arms[i]
		arm.root = to_global(_mirror(SHOULDER, side))
		arm.target = wings[i].target.global_position
		arm.pole = wings[i].pole.global_position
		arm.solve()
		var leg := legs[i]
		leg.root = to_global(_mirror(HIP, side))
		leg.target = feet[i].global_position
		leg.pole = _knee_poles[i].global_position
		leg.solve()
		wings[i].rebuild()
	renderer.draw()


# One side: arm + wing, leg + foot target + knee pole.
func _build_side(i: int) -> void:
	var side := _side(i)
	var prefix := "L_" if side < 0.0 else "R_"
	var bone := fur_color.darkened(0.25)
	var forearm := Vector3(side * FOREARM * 0.99, 0, -0.03).length() # a slight bend, as before
	var arm := LimbChain.new(PackedFloat32Array([UPPER_ARM, forearm]))
	arm.radius = ARM_RADIUS
	arm.joint_radius = ARM_RADIUS
	arm.color = bone
	renderer.add(arm)
	arms.append(arm)
	var thigh := Vector3(0, -THIGH * 0.5, THIGH * 0.85).length()
	var leg := LimbChain.new(PackedFloat32Array([thigh, SHIN]))
	leg.radius = LEG_RADIUS
	leg.joint_radius = LEG_RADIUS
	leg.color = bone
	renderer.add(leg)
	legs.append(leg)
	var wing := BatWing.new()
	wing.name = prefix + "Wing"
	wing.side = side
	wing.membrane_color = membrane_color
	add_child(wing)
	wing.setup(arm, leg, _mirror(MEMBRANE_HIP, side), renderer)
	wings.append(wing)
	var foot_target := Marker3D.new()
	foot_target.name = prefix + "FootTarget"
	add_child(foot_target)
	foot_target.position = _mirror(TUCKED_FOOT, side)
	feet.append(foot_target)
	var knee_pole := Marker3D.new()
	knee_pole.name = prefix + "KneePole"
	add_child(knee_pole)
	knee_pole.position = _mirror(HIP, side) + Vector3(side * 0.15, 0.0, 0.05) # knees bow outward
	_knee_poles.append(knee_pole)


func _build_body() -> void:
	add_child(BatMeshes.blob(Vector3(0.1, 0.095, 0.135), Vector3(0, 0, 0.01), fur_color))
	add_child(BatMeshes.blob(Vector3(0.08, 0.075, 0.11), Vector3(0, -0.025, 0.0), belly_color))
	head = Node3D.new()
	head.name = "Head"
	head.position = Vector3(0, 0.04, -0.13)
	add_child(head)
	head.add_child(BatMeshes.blob(Vector3(0.085, 0.08, 0.08), Vector3.ZERO, fur_color))
	head.add_child(BatMeshes.blob(Vector3(0.03, 0.025, 0.02), Vector3(0, 0.07, 0.0), fur_color)) # tuft
	head.add_child(BatMeshes.blob(Vector3(0.018, 0.013, 0.012), Vector3(0, -0.005, -0.082), inner_ear_color)) # nose
	for side: float in [-1.0, 1.0]:
		var ear := Basis(Vector3.BACK, -side * 0.4) * Basis(Vector3.RIGHT, -0.15)
		head.add_child(BatMeshes.instance(BatMeshes.cone(0.042, 0.13), Transform3D(ear, Vector3(side * 0.045, 0.1, 0.0)), fur_color))
		head.add_child(BatMeshes.instance(BatMeshes.cone(0.028, 0.1), Transform3D(ear, Vector3(side * 0.045, 0.095, -0.012)), inner_ear_color))
		head.add_child(BatMeshes.blob(Vector3.ONE * 0.03, Vector3(side * 0.036, 0.015, -0.062), Color(0.98, 0.98, 0.95)))
		head.add_child(BatMeshes.blob(Vector3.ONE * 0.017, Vector3(side * 0.038, 0.017, -0.088), Color(0.05, 0.04, 0.06)))
		var fang := Transform3D(Basis(Vector3.RIGHT, PI), Vector3(side * 0.016, -0.045, -0.068))
		head.add_child(BatMeshes.instance(BatMeshes.cone(0.007, 0.022), fang, Color(1, 1, 0.95)))


static func _side(i: int) -> float:
	return -1.0 if i == 0 else 1.0


static func _mirror(v: Vector3, side: float) -> Vector3:
	return Vector3(v.x * side, v.y, v.z)
