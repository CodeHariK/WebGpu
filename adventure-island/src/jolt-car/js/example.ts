import * as THREE from 'three';

import { WebGL } from './three/WebGL';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import Stats from 'three/addons/libs/stats.module.js';

import initJolt from "jolt-physics";
import Jolt from "jolt-physics";

// Graphics variables
var stats;
export var camera: THREE.PerspectiveCamera, controls: OrbitControls, scene: THREE.Scene, renderer: THREE.WebGLRenderer;

// Timers
var clock = new THREE.Clock();
var time = 0;

// Physics variables

export let iJolt: typeof initJolt
export function updateJolt(j: typeof initJolt) {
	iJolt = j
}

var joltVar: Jolt.JoltInterface;
export var physicsSystem: Jolt.PhysicsSystem;
export var bodyInterface: Jolt.BodyInterface;

// List of objects spawned
export var dynamicObjects: Array<THREE.Mesh> = [];

// The update function
export var onExampleUpdate: (time: number, deltaTime: number) => void;
export function updateOnExampleUpdate(exampleUpdate: (time: number, deltaTime: number) => void) {
	onExampleUpdate = exampleUpdate
}


export const DegreesToRadians = (deg: number) => deg * (Math.PI / 180.0);

export const wrapVec3 = (v: Jolt.Vec3 | Jolt.RVec3) => new THREE.Vector3(v.GetX(), v.GetY(), v.GetZ());
export const unwrapVec3 = (v: THREE.Vector3) => new iJolt.Vec3(v.x, v.y, v.z);
export const wrapRVec3 = wrapVec3;
export const unwrapRVec3 = (v: THREE.Vector3) => new iJolt.RVec3(v.x, v.y, v.z);
export const wrapQuat = (q: Jolt.Quat) => new THREE.Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
export const unwrapQuat = (q: THREE.Quaternion) => new iJolt.Quat(q.x, q.y, q.z, q.w);

// Object layers
export const LAYER_NON_MOVING = 0;
export const LAYER_MOVING = 1;
export const NUM_OBJECT_LAYERS = 2;

export function getRandomQuat() {
	let vec = new iJolt.Vec3(0.001 + Math.random(), Math.random(), Math.random());
	let quat = iJolt.Quat.prototype.sRotation(vec.Normalized(), 2 * Math.PI * Math.random());
	iJolt.destroy(vec);
	return quat;
}

export function onWindowResize() {

	camera.aspect = window.innerWidth / window.innerHeight;
	camera.updateProjectionMatrix();

	renderer.setSize(window.innerWidth, window.innerHeight);
}

export function initGraphics() {
	let container = document.getElementById('app');

	renderer = new THREE.WebGLRenderer();
	renderer.setClearColor(0xbfd1e5);
	renderer.setPixelRatio(window.devicePixelRatio);
	renderer.setSize(window.innerWidth, window.innerHeight);

	camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.2, 2000);
	camera.position.set(0, 15, 30);
	camera.lookAt(new THREE.Vector3(0, 0, 0));

	scene = new THREE.Scene();

	var dirLight = new THREE.DirectionalLight(0xffffff, 1);
	dirLight.position.set(10, 10, 5);
	scene.add(dirLight);

	controls = new OrbitControls(camera, container);

	container.appendChild(renderer.domElement);

	stats = new Stats();
	stats.domElement.style.position = 'absolute';
	stats.domElement.style.top = '0px';
	container.appendChild(stats.domElement);

	window.addEventListener('resize', onWindowResize, false);
}

let setupCollisionFiltering = function (settings: Jolt.JoltSettings) {
	// Layer that objects can be in, determines which other objects it can collide with
	// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
	// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
	// but only if you do collision testing).
	let objectFilter = new iJolt.ObjectLayerPairFilterTable(NUM_OBJECT_LAYERS);
	objectFilter.EnableCollision(LAYER_NON_MOVING, LAYER_MOVING);
	objectFilter.EnableCollision(LAYER_MOVING, LAYER_MOVING);

	// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
	// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
	// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
	// many object layers you'll be creating many broad phase trees, which is not efficient.
	const BP_LAYER_NON_MOVING = new iJolt.BroadPhaseLayer(0);
	const BP_LAYER_MOVING = new iJolt.BroadPhaseLayer(1);
	const NUM_BROAD_PHASE_LAYERS = 2;
	let bpInterface = new iJolt.BroadPhaseLayerInterfaceTable(NUM_OBJECT_LAYERS, NUM_BROAD_PHASE_LAYERS);
	bpInterface.MapObjectToBroadPhaseLayer(LAYER_NON_MOVING, BP_LAYER_NON_MOVING);
	bpInterface.MapObjectToBroadPhaseLayer(LAYER_MOVING, BP_LAYER_MOVING);

	settings.mObjectLayerPairFilter = objectFilter;
	settings.mBroadPhaseLayerInterface = bpInterface;
	settings.mObjectVsBroadPhaseLayerFilter = new iJolt.ObjectVsBroadPhaseLayerFilterTable(settings.mBroadPhaseLayerInterface, NUM_BROAD_PHASE_LAYERS, settings.mObjectLayerPairFilter, NUM_OBJECT_LAYERS);
};

export function initPhysics() {

	// Initialize Jolt
	let settings = new iJolt.JoltSettings();
	settings.mMaxWorkerThreads = 3; // Limit the number of worker threads to 3 (for a total of 4 threads working on the simulation). Note that this value will always be clamped against the number of CPUs in the system - 1.
	setupCollisionFiltering(settings);
	joltVar = new iJolt.JoltInterface(settings);
	iJolt.destroy(settings);
	physicsSystem = joltVar.GetPhysicsSystem();
	bodyInterface = physicsSystem.GetBodyInterface();
}

function updatePhysics(deltaTime) {

	// When running below 55 Hz, do 2 steps instead of 1
	var numSteps = deltaTime > 1.0 / 55.0 ? 2 : 1;

	// Step the physics world
	joltVar.Step(deltaTime, numSteps);
}

export function initExample(Jolt, updateFunction) {
	window.Jolt = Jolt;

	let container = document.getElementById('app');
	container.innerHTML = "";

	if (WebGL.isWebGLAvailable()) {
		onExampleUpdate = updateFunction;

		initGraphics();
		initPhysics();
		renderExample();
	} else {
		const warning = WebGL.getWebGLErrorMessage();
		container.appendChild(warning);
	}

	// The memory profiler doesn't have an ID so we can't mess with it in css, set an ID here
	let memoryprofilerCanvas = document.getElementById("memoryprofiler_canvas");
	if (memoryprofilerCanvas)
		memoryprofilerCanvas.parentElement.id = "memoryprofiler";
}

export function renderExample() {

	requestAnimationFrame(renderExample);

	// Don't go below 30 Hz to prevent spiral of death
	var deltaTime = clock.getDelta();
	deltaTime = Math.min(deltaTime, 1.0 / 30.0);

	if (onExampleUpdate != null)
		onExampleUpdate(time, deltaTime);

	// Update object transforms
	for (let i = 0, il = dynamicObjects.length; i < il; i++) {
		let objThree = dynamicObjects[i];
		let body = objThree.userData.body;
		objThree.position.copy(wrapVec3(body.GetPosition()));
		objThree.quaternion.copy(wrapQuat(body.GetRotation()));

		if (body.GetBodyType() == iJolt.EBodyType_SoftBody) {
			if (objThree.userData.updateVertex) {
				objThree.userData.updateVertex();
			} else {
				objThree.geometry = createMeshForShape(body.GetShape());
			}
		}
	}

	time += deltaTime;

	updatePhysics(deltaTime);

	controls.update(deltaTime);

	renderer.render(scene, camera);

	stats.update();
}

export function addToThreeScene(body: Jolt.Body, color: number) {
	let threeObject = getThreeObjectForBody(body, color);
	threeObject.userData.body = body;

	scene.add(threeObject);
	dynamicObjects.push(threeObject);
}

export function addToScene(body: Jolt.Body, color: number) {
	bodyInterface.AddBody(body.GetID(), iJolt.EActivation_Activate);

	addToThreeScene(body, color);
}

export function removeFromScene(threeObject: THREE.Mesh) {
	let id = threeObject.userData.body.GetID();
	bodyInterface.RemoveBody(id);
	bodyInterface.DestroyBody(id);
	delete threeObject.userData.body;

	scene.remove(threeObject);
	let idx = dynamicObjects.indexOf(threeObject);
	dynamicObjects.splice(idx, 1);
}

export function createFloor(size = 50) {
	var shape = new iJolt.BoxShape(new iJolt.Vec3(size, 0.5, size), 0.05, null);
	var creationSettings = new iJolt.BodyCreationSettings(shape, new iJolt.RVec3(0, -0.5, 0), new iJolt.Quat(0, 0, 0, 1), iJolt.EMotionType_Static, LAYER_NON_MOVING);
	let body = bodyInterface.CreateBody(creationSettings);
	iJolt.destroy(creationSettings);
	addToScene(body, 0xc7c7c7);
	return body;
}

export function createBox(position: Jolt.RVec3, rotation: Jolt.Quat, halfExtent: Jolt.Vec3, motionType: Jolt.EMotionType, layer: number, color = 0xffffff) {
	let shape = new iJolt.BoxShape(halfExtent, 0.05, null);
	let creationSettings = new iJolt.BodyCreationSettings(shape, position, rotation, motionType, layer);
	let body = bodyInterface.CreateBody(creationSettings);
	iJolt.destroy(creationSettings);
	addToScene(body, color);
	return body;
}

export function createSphere(position: Jolt.RVec3, radius: number, motionType: Jolt.EMotionType, layer: number, color = 0xffffff) {
	let shape = new iJolt.SphereShape(radius, null);
	let creationSettings = new iJolt.BodyCreationSettings(shape, position, iJolt.Quat.prototype.sIdentity(), motionType, layer);
	let body = bodyInterface.CreateBody(creationSettings);
	iJolt.destroy(creationSettings);
	addToScene(body, color);
	return body;
}

function createMeshForShape(shape: Jolt.Shape) {
	// Get triangle data
	let scale = new iJolt.Vec3(1, 1, 1);
	let triContext = new iJolt.ShapeGetTriangles(shape, iJolt.AABox.prototype.sBiggest(), shape.GetCenterOfMass(), iJolt.Quat.prototype.sIdentity(), scale);
	iJolt.destroy(scale);

	// Get a view on the triangle data (does not make a copy)
	let vertices = new Float32Array(iJolt.HEAPF32.buffer, triContext.GetVerticesData(), triContext.GetVerticesSize() / Float32Array.BYTES_PER_ELEMENT);

	// Now move the triangle data to a buffer and clone it so that we can free the memory from the C++ heap (which could be limited in size)
	let buffer = new THREE.BufferAttribute(vertices, 3).clone();
	iJolt.destroy(triContext);

	// Create a three mesh
	let geometry = new THREE.BufferGeometry();
	geometry.setAttribute('position', buffer);
	geometry.computeVertexNormals();

	return geometry;
}

export function getSoftBodyMesh(body: Jolt.Body, material) {
	const motionProperties = iJolt.castObject(body.GetMotionProperties(), iJolt.SoftBodyMotionProperties);
	const vertexSettings = motionProperties.GetVertices();
	const settings = motionProperties.GetSettings();
	const positionOffset = iJolt.SoftBodyVertexTraits.prototype.mPositionOffset;
	const faceData = settings.mFaces;

	// Get a view on the triangle data
	const softVertex = [];
	for (let i = 0; i < vertexSettings.size(); i++) {
		softVertex.push(new Float32Array(iJolt.HEAP32.buffer, iJolt.getPointer(vertexSettings.at(i)) + positionOffset, 3));
	}

	// Define faces (indices of vertices for the triangles)
	const faces = new Uint32Array(faceData.size() * 3);
	for (let i = 0; i < faceData.size(); i++) {
		faces.set(new Uint32Array(iJolt.HEAP32.buffer, iJolt.getPointer(faceData.at(i)), 3), i * 3);
	}

	// Create a three mesh
	let geometry = new THREE.BufferGeometry();
	let vertices = new Float32Array(vertexSettings.size() * 3);
	geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
	geometry.setIndex(new THREE.BufferAttribute(faces, 1));
	material.side = THREE.DoubleSide;
	const threeObject = new THREE.Mesh(geometry, material);
	threeObject.userData.updateVertex = () => {
		for (let i = 0; i < softVertex.length; i++) {
			vertices.set(softVertex[i], i * 3);
		}
		geometry.computeVertexNormals();
		geometry.getAttribute('position').needsUpdate = true;
		geometry.getAttribute('normal').needsUpdate = true;
	}
	threeObject.userData.updateVertex();
	return threeObject;
}

export function getThreeObjectForBody(body: Jolt.Body, color: number) {
	let material = new THREE.MeshPhongMaterial({ color: color });

	let threeObject: THREE.Mesh;

	let shape = body.GetShape();
	switch (shape.GetSubType()) {
		case iJolt.EShapeSubType_Box:
			let boxShape = iJolt.castObject(shape, iJolt.BoxShape);
			let extent = wrapVec3(boxShape.GetHalfExtent()).multiplyScalar(2);
			threeObject = new THREE.Mesh(new THREE.BoxGeometry(extent.x, extent.y, extent.z, 1, 1, 1), material);
			break;
		case iJolt.EShapeSubType_Sphere:
			let sphereShape = iJolt.castObject(shape, iJolt.SphereShape);
			threeObject = new THREE.Mesh(new THREE.SphereGeometry(sphereShape.GetRadius(), 32, 32), material);
			break;
		case iJolt.EShapeSubType_Capsule:
			let capsuleShape = iJolt.castObject(shape, iJolt.CapsuleShape);
			threeObject = new THREE.Mesh(new THREE.CapsuleGeometry(capsuleShape.GetRadius(), 2 * capsuleShape.GetHalfHeightOfCylinder(), 20, 10), material);
			break;
		case iJolt.EShapeSubType_Cylinder:
			let cylinderShape = iJolt.castObject(shape, iJolt.CylinderShape);
			threeObject = new THREE.Mesh(new THREE.CylinderGeometry(cylinderShape.GetRadius(), cylinderShape.GetRadius(), 2 * cylinderShape.GetHalfHeight(), 20, 1), material);
			break;
		default:
			if (body.GetBodyType() == iJolt.EBodyType_SoftBody)
				threeObject = getSoftBodyMesh(body, material);
			else
				threeObject = new THREE.Mesh(createMeshForShape(shape), material);
			break;
	}

	threeObject.position.copy(wrapVec3(body.GetPosition()));
	threeObject.quaternion.copy(wrapQuat(body.GetRotation()));

	return threeObject;
}

export function createMeshFloor(n: number, cellSize: number, maxHeight: number, posX: number, posY: number, posZ: number) {
	// Create regular grid of triangles
	let height = function (x: number, y: number) { return Math.sin(x / 2) * Math.cos(y / 3); };
	let triangles = new iJolt.TriangleList;
	triangles.resize(n * n * 2);
	for (let x = 0; x < n; ++x)
		for (let z = 0; z < n; ++z) {
			let center = n * cellSize / 2;

			let x1 = cellSize * x - center;
			let z1 = cellSize * z - center;
			let x2 = x1 + cellSize;
			let z2 = z1 + cellSize;

			{
				let t = triangles.at((x * n + z) * 2);
				let v1 = t.get_mV(0), v2 = t.get_mV(1), v3 = t.get_mV(2);
				v1.x = x1, v1.y = height(x, z), v1.z = z1;
				v2.x = x1, v2.y = height(x, z + 1), v2.z = z2;
				v3.x = x2, v3.y = height(x + 1, z + 1), v3.z = z2;
			}

			{
				let t = triangles.at((x * n + z) * 2 + 1);
				let v1 = t.get_mV(0), v2 = t.get_mV(1), v3 = t.get_mV(2);
				v1.x = x1, v1.y = height(x, z), v1.z = z1;
				v2.x = x2, v2.y = height(x + 1, z + 1), v2.z = z2;
				v3.x = x2, v3.y = height(x + 1, z), v3.z = z1;
			}
		}
	let materials = new iJolt.PhysicsMaterialList;
	let shape = new iJolt.MeshShapeSettings(triangles, materials).Create().Get();
	iJolt.destroy(triangles);
	iJolt.destroy(materials);

	// Create body
	let creationSettings = new iJolt.BodyCreationSettings(shape, new iJolt.RVec3(posX, posY, posZ), new iJolt.Quat(0, 0, 0, 1), iJolt.EMotionType_Static, LAYER_NON_MOVING);
	let body = bodyInterface.CreateBody(creationSettings);
	iJolt.destroy(creationSettings);
	addToScene(body, 0xc7c7c7);
}

export function createVehicleTrack() {
	const track = [
		[
			[[38, 64, -14], [38, 64, -16], [38, -64, -16], [38, -64, -14], [64, -64, -16], [64, -64, -14], [64, 64, -16], [64, 64, -14]],
			[[-16, 64, -14], [-16, 64, -16], [-16, -64, -16], [-16, -64, -14], [10, -64, -16], [10, -64, -14], [10, 64, -16], [10, 64, -14]],
			[[10, -48, -14], [10, -48, -16], [10, -64, -16], [10, -64, -14], [38, -64, -16], [38, -64, -14], [38, -48, -16], [38, -48, -14]],
			[[10, 64, -14], [10, 64, -16], [10, 48, -16], [10, 48, -14], [38, 48, -16], [38, 48, -14], [38, 64, -16], [38, 64, -14]]
		],
		[
			[[38, 48, -10], [38, 48, -14], [38, -48, -14], [38, -48, -10], [40, -48, -14], [40, -48, -10], [40, 48, -14], [40, 48, -10]],
			[[62, 62, -10], [62, 62, -14], [62, -64, -14], [62, -64, -10], [64, -64, -14], [64, -64, -10], [64, 62, -14], [64, 62, -10]],
			[[8, 48, -10], [8, 48, -14], [8, -48, -14], [8, -48, -10], [10, -48, -14], [10, -48, -10], [10, 48, -14], [10, 48, -10]],
			[[-16, 62, -10], [-16, 62, -14], [-16, -64, -14], [-16, -64, -10], [-14, -64, -14], [-14, -64, -10], [-14, 62, -14], [-14, 62, -10]],
			[[-14, -62, -10], [-14, -62, -14], [-14, -64, -14], [-14, -64, -10], [62, -64, -14], [62, -64, -10], [62, -62, -14], [62, -62, -10]],
			[[8, -48, -10], [8, -48, -14], [8, -50, -14], [8, -50, -10], [40, -50, -14], [40, -50, -10], [40, -48, -14], [40, -48, -10]],
			[[8, 50, -10], [8, 50, -14], [8, 48, -14], [8, 48, -10], [40, 48, -14], [40, 48, -10], [40, 50, -14], [40, 50, -10]],
			[[-16, 64, -10], [-16, 64, -14], [-16, 62, -14], [-16, 62, -10], [64, 62, -14], [64, 62, -10], [64, 64, -14], [64, 64, -10]]
		],
		[
			[[-4, 22, -14], [-4, -14, -14], [-4, -14, -10], [4, -14, -14], [4, -14, -10], [4, 22, -14]],
			[[-4, -27, -14], [-4, -48, -14], [-4, -48, -11], [4, -48, -14], [4, -48, -11], [4, -27, -14]],
			[[-4, 50, -14], [-4, 30, -14], [-4, 30, -12], [4, 30, -14], [4, 30, -12], [4, 50, -14]],
			[[46, 50, -14], [46, 31, -14], [46, 50, -12], [54, 31, -14], [54, 50, -12], [54, 50, -14]],
			[[46, 16, -14], [46, -19, -14], [46, 16, -10], [54, -19, -14], [54, 16, -10], [54, 16, -14]],
			[[46, -28, -14], [46, -48, -14], [46, -28, -11], [54, -48, -14], [54, -28, -11], [54, -28, -14]]
		]
	];

	const mapColors = [0x666666, 0x006600, 0x000066];

	let tempVec = new iJolt.Vec3(0, 1, 0);
	const mapRot = iJolt.Quat.prototype.sRotation(tempVec, 0.5 * Math.PI);
	let tempRVec = new iJolt.RVec3(0, 0, 0);
	track.forEach((type, tIdx) => {
		type.forEach(block => {

			const hull = new iJolt.ConvexHullShapeSettings;
			block.forEach(v => {
				tempVec.Set(-v[1], v[2], v[0]);
				hull.mPoints.push_back(tempVec);
			});
			const shape = hull.Create().Get();

			tempRVec.Set(0, 10, 0);
			const creationSettings = new iJolt.BodyCreationSettings(shape, tempRVec, mapRot, iJolt.EMotionType_Static, LAYER_NON_MOVING);
			iJolt.destroy(hull);

			const body = bodyInterface.CreateBody(creationSettings);
			iJolt.destroy(creationSettings);
			body.SetFriction(1.0);
			addToScene(body, mapColors[tIdx]);
		});
	});
	iJolt.destroy(tempVec);
	iJolt.destroy(tempRVec);
}

export function addLine(from, to, color) {
	const material = new THREE.LineBasicMaterial({ color: color });
	const points = [];
	points.push(wrapRVec3(from));
	points.push(wrapRVec3(to));
	const geometry = new THREE.BufferGeometry().setFromPoints(points);
	const line = new THREE.Line(geometry, material);
	scene.add(line);
}

export function addMarker(location, size, color) {
	const material = new THREE.LineBasicMaterial({ color: color });
	const points = [];
	const center = wrapVec3(location);
	points.push(center.clone().add(new THREE.Vector3(-size, 0, 0)));
	points.push(center.clone().add(new THREE.Vector3(size, 0, 0)));
	points.push(center.clone().add(new THREE.Vector3(0, -size, 0)));
	points.push(center.clone().add(new THREE.Vector3(0, size, 0)));
	points.push(center.clone().add(new THREE.Vector3(0, 0, -size)));
	points.push(center.clone().add(new THREE.Vector3(0, 0, size)));
	const geometry = new THREE.BufferGeometry().setFromPoints(points);
	const line = new THREE.LineSegments(geometry, material);
	scene.add(line);
}