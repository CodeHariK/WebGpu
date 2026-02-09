
import { Ray, RigidBody, World } from '@dimforge/rapier3d';

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';

// SCENE
const scene = new THREE.Scene();
scene.background = new THREE.Color(0xa8def0);

// CAMERA
const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 1000);
camera.position.y = 5;
camera.position.z = 10;
camera.position.x = -13;

// RENDERER
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
renderer.shadowMap.enabled = true

// ORBIT CAMERA CONTROLS
const orbitControls = new OrbitControls(camera, renderer.domElement);
orbitControls.enableDamping = true
orbitControls.enablePan = true
orbitControls.minDistance = 5
orbitControls.maxDistance = 20
orbitControls.maxPolarAngle = Math.PI / 2 - 0.05 // prevent camera below ground
orbitControls.minPolarAngle = Math.PI / 4        // prevent top down view
orbitControls.update();

const dLight = new THREE.DirectionalLight('white', 0.6);
dLight.position.x = 20;
dLight.position.y = 30;
dLight.castShadow = true;
dLight.shadow.mapSize.width = 4096;
dLight.shadow.mapSize.height = 4096;
const d = 35;
dLight.shadow.camera.left = - d;
dLight.shadow.camera.right = d;
dLight.shadow.camera.top = d;
dLight.shadow.camera.bottom = - d;
scene.add(dLight);

const aLight = new THREE.AmbientLight('white', 0.4);
scene.add(aLight);

// ANIMATE
document.body.appendChild(renderer.domElement);

// RESIZE HANDLER
function onWindowResize() {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
}
window.addEventListener('resize', onWindowResize);

function loadTexture(path: string): THREE.Texture {
    const texture = new THREE.TextureLoader().load(path);
    texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
    texture.repeat.x = 10;
    texture.repeat.y = 10;
    return texture;
}


// MODEL WITH ANIMATIONS
var characterControls: CharacterControls

import('@dimforge/rapier3d').then(RAPIER => {

    function body(scene: THREE.Scene, world: World,
        bodyType: 'dynamic' | 'static' | 'kinematicPositionBased',
        colliderType: 'cube' | 'sphere' | 'cylinder' | 'cone', dimension: any,
        translation: { x: number, y: number, z: number },
        rotation: { x: number, y: number, z: number },
        color: string): { rigid: RigidBody, mesh: THREE.Mesh } {

        let bodyDesc

        if (bodyType === 'dynamic') {
            bodyDesc = RAPIER.RigidBodyDesc.dynamic();
        } else if (bodyType === 'kinematicPositionBased') {
            bodyDesc = RAPIER.RigidBodyDesc.kinematicPositionBased();
        } else if (bodyType === 'static') {
            bodyDesc = RAPIER.RigidBodyDesc.fixed();
            bodyDesc.setCanSleep(false);
        }

        if (translation) {
            bodyDesc.setTranslation(translation.x, translation.y, translation.z)
        }
        if (rotation) {
            const q = new THREE.Quaternion().setFromEuler(
                new THREE.Euler(rotation.x, rotation.y, rotation.z, 'XYZ')
            )
            bodyDesc.setRotation({ x: q.x, y: q.y, z: q.z, w: q.w })
        }

        let rigidBody = world.createRigidBody(bodyDesc);

        let collider;
        if (colliderType === 'cube') {
            collider = RAPIER.ColliderDesc.cuboid(dimension.hx, dimension.hy, dimension.hz);
        } else if (colliderType === 'sphere') {
            collider = RAPIER.ColliderDesc.ball(dimension.radius);
        } else if (colliderType === 'cylinder') {
            collider = RAPIER.ColliderDesc.cylinder(dimension.hh, dimension.radius);
        } else if (colliderType === 'cone') {
            collider = RAPIER.ColliderDesc.cone(dimension.hh, dimension.radius);
            // cone center of mass is at bottom
            collider.centerOfMass = { x: 0, y: 0, z: 0 }
        }
        world.createCollider(collider, rigidBody);

        let bufferGeometry;
        if (colliderType === 'cube') {
            bufferGeometry = new THREE.BoxGeometry(dimension.hx * 2, dimension.hy * 2, dimension.hz * 2);
        } else if (colliderType === 'sphere') {
            bufferGeometry = new THREE.SphereGeometry(dimension.radius, 32, 32);
        } else if (colliderType === 'cylinder') {
            bufferGeometry = new THREE.ConeGeometry(dimension.radius,
                dimension.radius, dimension.hh * 2, 32, false);
        } else if (colliderType === 'cone') {
            bufferGeometry = new THREE.ConeGeometry(dimension.radius, dimension.hh * 2,
                32, 32);
        }

        const threeMesh = new THREE.Mesh(bufferGeometry, new THREE.MeshPhongMaterial({ color: color }));
        threeMesh.castShadow = true;
        threeMesh.receiveShadow = true;
        scene.add(threeMesh);

        return { rigid: rigidBody, mesh: threeMesh };
    }

    function generateTerrain(nsubdivs: number, scale: { x: number, y: number, z: number }) {
        let heights: number[] = []

        // three plane
        const threeFloor = new THREE.Mesh(
            new THREE.PlaneGeometry(scale.x, scale.z, nsubdivs, nsubdivs),
            new THREE.MeshStandardMaterial({
                map: loadTexture('./src/assets/grass/Grass_005_BaseColor.jpg'),
                normalMap: loadTexture('./src/assets/grass/Grass_005_Normal.jpg'),
                aoMap: loadTexture('./src/assets/grass/Grass_005_AmbientOcclusion.jpg'),
                roughnessMap: loadTexture('./src/assets/grass/Grass_005_Roughness.jpg'),
                roughness: 0.6
            }));
        threeFloor.rotateX(- Math.PI / 2);
        threeFloor.receiveShadow = true;
        threeFloor.castShadow = true;
        scene.add(threeFloor);

        // add height data to plane
        const vertices = threeFloor.geometry.attributes.position.array;
        const dx = scale.x / nsubdivs;
        const dy = scale.z / nsubdivs;
        // store height data in map column-row map
        const columsRows = new Map();
        for (let i = 0; i < vertices.length; i += 3) {
            // translate into colum / row indices
            let row = Math.floor(Math.abs((vertices as any)[i] + (scale.x / 2)) / dx);
            let column = Math.floor(Math.abs((vertices as any)[i + 1] - (scale.z / 2)) / dy);
            // generate height for this column & row
            const randomHeight = Math.random();
            (vertices as any)[i + 2] = scale.y * randomHeight;
            // store height
            if (!columsRows.get(column)) {
                columsRows.set(column, new Map());
            }
            columsRows.get(column).set(row, randomHeight);
        }
        threeFloor.geometry.computeVertexNormals();

        // store height data into column-major-order matrix array
        for (let i = 0; i <= nsubdivs; ++i) {
            for (let j = 0; j <= nsubdivs; ++j) {
                heights.push(columsRows.get(j).get(i));
            }
        }

        let groundBodyDesc = RAPIER.RigidBodyDesc.fixed();
        let groundBody = world.createRigidBody(groundBodyDesc);
        let groundCollider = RAPIER.ColliderDesc.heightfield(
            nsubdivs, nsubdivs, new Float32Array(heights), scale
        );
        world.createCollider(groundCollider, groundBody);
    }

    // Use the RAPIER module here.
    let gravity = { x: 0.0, y: -9.81, z: 0.0 };
    let world = new RAPIER.World(gravity);

    // Bodys
    const bodys: { rigid: RigidBody, mesh: THREE.Mesh }[] = []

    // Create Ground.
    let nsubdivs = 20;
    let scale = new RAPIER.Vector3(70.0, 3.0, 70.0);
    generateTerrain(nsubdivs, scale);

    const staticB = body(scene, world, 'static', 'cube',
        { hx: 10, hy: 0.8, hz: 10 }, { x: scale.x / 2, y: 2.5, z: 0 },
        { x: 0, y: 0, z: 0.3 }, 'pink');
    bodys.push(staticB);

    const cubeBody = body(scene, world, 'dynamic', 'cube',
        { hx: 0.5, hy: 0.5, hz: 0.5 }, { x: 0, y: 15, z: 0 },
        { x: 0, y: 0.4, z: 0.7 }, 'orange');
    bodys.push(cubeBody);

    const sphereBody = body(scene, world, 'dynamic', 'sphere',
        { radius: 0.7 }, { x: 4, y: 15, z: 2 },
        { x: 0, y: 1, z: 0 }, 'blue');
    bodys.push(sphereBody);

    const sphereBody2 = body(scene, world, 'dynamic', 'sphere',
        { radius: 0.7 }, { x: 0, y: 15, z: 0 },
        { x: 0, y: 1, z: 0 }, 'red');
    bodys.push(sphereBody2);

    const cylinderBody = body(scene, world, 'dynamic', 'cylinder',
        { hh: 1.0, radius: 0.7 }, { x: -7, y: 15, z: 8 },
        { x: 0, y: 1, z: 0 }, 'green');
    bodys.push(cylinderBody);

    const coneBody = body(scene, world, 'dynamic', 'cone',
        { hh: 1.0, radius: 1 }, { x: 7, y: 15, z: -8 },
        { x: 0, y: 1, z: 0 }, 'purple');
    bodys.push(coneBody);

    // character controller
    new GLTFLoader().load('./src/assets/Soldier.glb', function (gltf) {
        const model = gltf.scene;
        model.traverse(function (object: any) {
            if (object.isMesh) object.castShadow = true;
        });
        scene.add(model);

        const gltfAnimations: THREE.AnimationClip[] = gltf.animations;
        const mixer = new THREE.AnimationMixer(model);
        const animationsMap: Map<string, THREE.AnimationAction> = new Map()
        gltfAnimations.filter(a => a.name != 'TPose').forEach((a: THREE.AnimationClip) => {
            animationsMap.set(a.name, mixer.clipAction(a))
        })


        // RIGID BODY
        let bodyDesc = RAPIER.RigidBodyDesc.kinematicPositionBased().setTranslation(-1, 3, 1)
        let rigidBody = world.createRigidBody(bodyDesc);
        let dynamicCollider = RAPIER.ColliderDesc.ball(CONTROLLER_BODY_RADIUS);
        world.createCollider(dynamicCollider, rigidBody);

        characterControls = new CharacterControls(model, mixer,
            animationsMap, orbitControls,
            camera, 'Idle',
            new RAPIER.Ray(
                { x: 0, y: 0, z: 0 },
                { x: 0, y: -1, z: 0 }
            ), rigidBody)
    });

    const clock = new THREE.Clock();
    // Game loop.
    let gameLoop = () => {
        let deltaTime = clock.getDelta();

        if (characterControls) {
            characterControls.update(world, deltaTime, keysPressed);
        }

        // Step the simulation forward.  
        world.step();
        // update 3d world with physical world
        bodys.forEach(body => {
            let position = body.rigid.translation();
            let rotation = body.rigid.rotation();

            body.mesh.position.x = position.x
            body.mesh.position.y = position.y
            body.mesh.position.z = position.z

            body.mesh.setRotationFromQuaternion(
                new THREE.Quaternion(rotation.x,
                    rotation.y,
                    rotation.z,
                    rotation.w));
        });

        orbitControls.update()
        renderer.render(scene, camera);

        setTimeout(gameLoop, 16);
    };

    gameLoop();
})


const keysPressed: any = {}
document.addEventListener('keydown', (event) => {
    if (event.shiftKey && characterControls) {
        characterControls.switchRunToggle()
    }
    keysPressed[event.key.toLowerCase()] = true
}, false);
document.addEventListener('keyup', (event) => {
    keysPressed[event.key.toLowerCase()] = false
}, false);





export const W = 'w'
export const A = 'a'
export const S = 's'
export const D = 'd'
export const SHIFT = 'shift'
export const DIRECTIONS = [W, A, S, D]


export const CONTROLLER_BODY_RADIUS = 0.28;

export class CharacterControls {

    model: THREE.Group
    mixer: THREE.AnimationMixer
    animationsMap: Map<string, THREE.AnimationAction> = new Map() // Walk, Run, Idle
    orbitControl: OrbitControls
    camera: THREE.Camera

    // state
    toggleRun: boolean = true
    currentAction: string

    // temporary data
    walkDirection = new THREE.Vector3()
    rotateAngle = new THREE.Vector3(0, 1, 0)
    rotateQuarternion: THREE.Quaternion = new THREE.Quaternion()
    cameraTarget = new THREE.Vector3()
    storedFall = 0

    // constants
    fadeDuration: number = 0.2
    runVelocity = 5
    walkVelocity = 2

    ray: Ray
    rigidBody: RigidBody
    lerp = (x: number, y: number, a: number) => x * (1 - a) + y * a;

    constructor(model: THREE.Group,
        mixer: THREE.AnimationMixer, animationsMap: Map<string, THREE.AnimationAction>,
        orbitControl: OrbitControls, camera: THREE.Camera,
        currentAction: string,
        ray: Ray, rigidBody: RigidBody
    ) {
        this.model = model
        this.mixer = mixer
        this.animationsMap = animationsMap
        this.currentAction = currentAction
        this.animationsMap.forEach((value, key) => {
            if (key == currentAction) {
                value.play()
            }
        })

        this.ray = ray
        this.rigidBody = rigidBody

        this.orbitControl = orbitControl
        this.camera = camera
        this.updateCameraTarget(new THREE.Vector3(0, 1, 5))
    }

    public switchRunToggle() {
        this.toggleRun = !this.toggleRun
    }

    public update(world: World, delta: number, keysPressed: any) {
        const directionPressed = DIRECTIONS.some(key => keysPressed[key] == true)

        var play = '';
        if (directionPressed && this.toggleRun) {
            play = 'Run'
        } else if (directionPressed) {
            play = 'Walk'
        } else {
            play = 'Idle'
        }

        if (this.currentAction != play) {
            const toPlay = this.animationsMap.get(play)
            const current = this.animationsMap.get(this.currentAction)

            current.fadeOut(this.fadeDuration)
            toPlay.reset().fadeIn(this.fadeDuration).play();

            this.currentAction = play
        }

        this.mixer.update(delta)

        this.walkDirection.x = this.walkDirection.y = this.walkDirection.z = 0

        let velocity = 0
        if (this.currentAction == 'Run' || this.currentAction == 'Walk') {
            // calculate towards camera direction
            var angleYCameraDirection = Math.atan2(
                (this.camera.position.x - this.model.position.x),
                (this.camera.position.z - this.model.position.z))
            // diagonal movement angle offset
            var directionOffset = this.directionOffset(keysPressed)

            // rotate model
            this.rotateQuarternion.setFromAxisAngle(this.rotateAngle, angleYCameraDirection + directionOffset)
            this.model.quaternion.rotateTowards(this.rotateQuarternion, 0.2)

            // calculate direction
            this.camera.getWorldDirection(this.walkDirection)
            this.walkDirection.y = 0
            this.walkDirection.normalize()
            this.walkDirection.applyAxisAngle(this.rotateAngle, directionOffset)

            // run/walk velocity
            velocity = this.currentAction == 'Run' ? this.runVelocity : this.walkVelocity
        }

        const translation = this.rigidBody.translation();
        if (translation.y < -1) {
            // don't fall below ground
            this.rigidBody.setNextKinematicTranslation({
                x: 0,
                y: 10,
                z: 0
            });
        } else {
            const cameraPositionOffset = this.camera.position.sub(this.model.position);
            // update model and camera
            this.model.position.x = translation.x
            this.model.position.y = translation.y
            this.model.position.z = translation.z
            this.updateCameraTarget(cameraPositionOffset)

            this.walkDirection.y += this.lerp(this.storedFall, -9.81 * delta, 0.10)
            this.storedFall = this.walkDirection.y
            this.ray.origin.x = translation.x
            this.ray.origin.y = translation.y
            this.ray.origin.z = translation.z
            let hit = world.castRay(this.ray, 0.5, false);
            if (hit) {
                const point = this.ray.pointAt(hit.timeOfImpact);
                let diff = translation.y - (point.y + CONTROLLER_BODY_RADIUS);
                if (diff < 0.0) {
                    this.storedFall = 0
                    this.walkDirection.y = this.lerp(0, Math.abs(diff), 0.5)
                }
            }

            this.walkDirection.x = this.walkDirection.x * velocity * delta
            this.walkDirection.z = this.walkDirection.z * velocity * delta

            this.rigidBody.setNextKinematicTranslation({
                x: translation.x + this.walkDirection.x,
                y: translation.y + this.walkDirection.y,
                z: translation.z + this.walkDirection.z
            });
        }
    }

    private updateCameraTarget(offset: THREE.Vector3) {
        // move camera
        const rigidTranslation = this.rigidBody.translation();
        this.camera.position.x = rigidTranslation.x + offset.x
        this.camera.position.y = rigidTranslation.y + offset.y
        this.camera.position.z = rigidTranslation.z + offset.z

        // update camera target
        this.cameraTarget.x = rigidTranslation.x
        this.cameraTarget.y = rigidTranslation.y + 1
        this.cameraTarget.z = rigidTranslation.z
        this.orbitControl.target = this.cameraTarget
    }

    private directionOffset(keysPressed: any) {
        var directionOffset = 0 // w

        if (keysPressed[W]) {
            if (keysPressed[A]) {
                directionOffset = Math.PI / 4 // w+a
            } else if (keysPressed[D]) {
                directionOffset = - Math.PI / 4 // w+d
            }
        } else if (keysPressed[S]) {
            if (keysPressed[A]) {
                directionOffset = Math.PI / 4 + Math.PI / 2 // s+a
            } else if (keysPressed[D]) {
                directionOffset = -Math.PI / 4 - Math.PI / 2 // s+d
            } else {
                directionOffset = Math.PI // s
            }
        } else if (keysPressed[A]) {
            directionOffset = Math.PI / 2 // a
        } else if (keysPressed[D]) {
            directionOffset = - Math.PI / 2 // d
        }

        return directionOffset
    }

}