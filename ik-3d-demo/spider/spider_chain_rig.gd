## SpiderRig with custom-IK legs: each leg is a LimbChain (two-bone, analytic, bending toward its
## knee pole) and every leg of the spider is drawn by one LimbRenderer — no leg bones, no
## TwoBoneIK3D, no BoneAttachment3Ds. The Skeleton3D stays, holding only the body bone (the body
## parts, head and eyes ride on it as before), and so do the foot targets, knee poles and the
## rigid one-bone legs, so the gait and everything above it are untouched.
##
## The legs are solved once per rendered frame, late (process_priority), after the spider, springs
## and brain have moved the body and the feet — the same moment Skeleton3D's IK would run.
## Arms (legs that don't walk) get two pincer fingers: small one-segment chains aimed off the
## forearm's tip, turned with the forearm.
class_name SpiderChainRig
extends SpiderRig

const FINGER_LENGTH := 0.2
const FINGER_SPREAD := 0.45 ## radians each side of the forearm
const SOLVE_PRIORITY := 1000 ## run after the default-priority nodes in the frame

var chains: Array[LimbChain] = [] ## per leg (solved for rigid legs too, but not drawn)
var renderer: LimbRenderer

var _fingers: Array = [] ## per leg: [] or two finger chains
var _poles_enabled := true


## Solve every leg toward its foot target and redraw. Called every frame by the driver node.
func solve() -> void:
	var body := skeleton.global_transform
	for leg in chains.size():
		var chain := chains[leg]
		var definition := layout.legs[leg]
		chain.root = body * definition.hip
		chain.target = targets[leg].global_position
		if _poles_enabled:
			chain.pole = poles[leg].global_position
		else: # no pole: bend the way the rest pose bends, knee up
			chain.pole = body * (definition.hip + definition.upper_vec()) + body.basis.y * 0.5
		chain.solve()
		if not _fingers[leg].is_empty():
			_aim_fingers(leg, body)
	renderer.draw()


func set_poles_enabled(enabled: bool) -> void:
	_poles_enabled = enabled


func joint_position(leg: int, joint: int) -> Vector3:
	return chains[leg].points[joint]


func _add_leg_parts(leg: SpiderLayout.LegDef, _body_bone: int, body: Node3D) -> void:
	if renderer == null:
		_add_renderer(body)
	knee_bones.append(-1) # no bones
	var chain := LimbChain.new(PackedFloat32Array([leg.upper_vec().length(), leg.lower_vec().length()]))
	chain.radius = leg.radius
	chain.joint_radius = leg.radius # cylinder + same-size balls = the capsule look of SpiderRig
	chain.color = layout.leg_color
	if leg.square:
		chain.style = LimbChain.Style.BLOCK
		chain.gap = leg.segment_gap
	chains.append(chain)
	if leg.rigid: # drawn by update_rigid_limbs() as one piece, like SpiderRig
		rigid_limbs.append(_add_rigid_limb(leg, body))
	else:
		rigid_limbs.append(null)
		renderer.add(chain)
	var fingers: Array[LimbChain] = []
	if not leg.walks:
		for i in 2:
			var finger := LimbChain.new(PackedFloat32Array([FINGER_LENGTH]), Vector3.ZERO, LimbChain.Solver.AIM)
			finger.radius = leg.radius * 0.5
			finger.joint_radius = finger.radius
			finger.color = layout.leg_color
			renderer.add(finger)
			fingers.append(finger)
	_fingers.append(fingers)


func _add_ik() -> void:
	pass # LimbChains solve themselves


# A claw's fingers splay in a V past the forearm's tip, in the forearm's rest frame turned by the
# shortest rotation from the rest forearm to the solved one.
func _aim_fingers(leg: int, body: Transform3D) -> void:
	var chain := chains[leg]
	var rest := body.basis * layout.legs[leg].lower_vec()
	var now := chain.points[2] - chain.points[1]
	if rest.length() < 1e-4 or now.length() < 1e-4:
		return
	var turn := Quaternion(rest.normalized(), now.normalized())
	var along := rest.normalized()
	var side := along.cross(body.basis.y).normalized()
	for i in 2:
		var spread := FINGER_SPREAD * (-1.0 if i == 0 else 1.0)
		var finger: LimbChain = _fingers[leg][i]
		finger.root = chain.points[2]
		finger.target = finger.root + turn * (along * cos(spread) + side * sin(spread)) * FINGER_LENGTH
		finger.solve()


func _add_renderer(body: Node3D) -> void:
	renderer = LimbRenderer.new()
	renderer.name = "LegRenderer"
	renderer.roughness = 0.6 # SpiderRig's leg material
	body.add_child(renderer)
	var driver := _Driver.new()
	driver.name = "LegSolver"
	driver.rig = self
	driver.process_priority = SOLVE_PRIORITY
	body.add_child(driver)


## Calls solve() every frame; lives under the body, so it goes away with a rebuild.
class _Driver extends Node:
	var rig: SpiderChainRig

	func _process(_delta: float) -> void:
		rig.solve()
