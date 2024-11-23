import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GUI } from 'three/addons/libs/lil-gui.module.min.js';
import * as RAPIER from '@dimforge/rapier3d';
import { createTerrain } from './terrain';

class Physics {
    static World = new RAPIER.World(new RAPIER.Vector3(0, -9.81, 0)); // Gravity

    static applyImpulse(rb: RAPIER.RigidBody, dir: THREE.Vector3) {
        dir.applyQuaternion(new THREE.Quaternion(
            car.rigidBody.rotation().x,
            car.rigidBody.rotation().y,
            car.rigidBody.rotation().z,
            car.rigidBody.rotation().w
        ));
        dir.multiplyScalar(5);

        rb.applyImpulse(new RAPIER.Vector3(dir.x, dir.y, dir.z), true);
    }

    static applyTorque(rb: RAPIER.RigidBody, torque: RAPIER.Vector3) {
        torque.x *= 2;
        torque.y *= 2;
        torque.z *= 2;
        rb.applyTorqueImpulse(torque, true);
    }

    static add(ss: RAPIER.Vector, vv: RAPIER.Vector) {
        return new THREE.Vector3(ss.x + vv.x, ss.y + vv.y, ss.z + vv.z)
    }
    static toVec(ss: RAPIER.Vector) {
        return new THREE.Vector3(ss.x, ss.y, ss.z)
    }
    static fromVec(ss: THREE.Vector3) {
        return new RAPIER.Vector3(ss.x, ss.y, ss.z)
    }

    static linearVelocityAtWorldPoint(rb: RAPIER.RigidBody, point: RAPIER.Vector) {
        // velocity = linearVelocity + angularVelocity * offsetFromCenter 

        return Physics.toVec(rb.linvel()).add(
            Physics.toVec(rb.angvel())
                .cross(
                    Physics.toVec(point).sub(rb.translation())))

    }
}

// Scene, Renderer, and Lighting
const scene = new THREE.Scene();
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap; // or other types of shadow maps

document.body.appendChild(renderer.domElement);

const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);

// Lighting
const directionalLight = new THREE.DirectionalLight(0xffffff, 2); // Increased intensity
directionalLight.position.set(10, 20, 10);
directionalLight.castShadow = true;
directionalLight.shadow.mapSize.width = 1024;
directionalLight.shadow.mapSize.height = 1024;
directionalLight.shadow.bias = -0.005; // Adjust to reduce shadow artifacts
scene.add(directionalLight);
const lightHelper = new THREE.DirectionalLightHelper(directionalLight, 5);
scene.add(lightHelper);

const ambientLight = new THREE.AmbientLight(0x404040, 1.5); // Increased intensity
scene.add(ambientLight);

// Ground
const groundGeometry = new THREE.PlaneGeometry(100, 100);
const groundMaterial = new THREE.MeshMatcapMaterial({ color: 0x228b22 });
const ground = new THREE.Mesh(groundGeometry, groundMaterial);
ground.rotation.x = -Math.PI / 2;
ground.receiveShadow = true;
scene.add(ground);
const groundColliderDesc = RAPIER.ColliderDesc.cuboid(50, 0.1, 50).setTranslation(0, -0.1, 0);
Physics.World.createCollider(groundColliderDesc);


// createTerrain(scene, Physics.World)


class Car {
    mesh: THREE.Mesh;
    rigidBody: RAPIER.RigidBody;

    static wheelOffsets = [
        new RAPIER.Vector3(1, 0, 2), // Front right
        new RAPIER.Vector3(-1, 0, 2), // Front left
        new RAPIER.Vector3(1, 0, -2), // Rear right
        new RAPIER.Vector3(-1, 0, -2), // Rear left
    ];

    static suspensionLength = 0.1; // Maximum suspension travel
    static suspensionStiffness = 100; // Spring stiffness
    static damping = 10; // Damping to reduce oscillation

    constructor() {
        this.mesh = this.createCarMesh();

        // Create Rapier RigidBody
        const rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(0, 1, 0);
        this.rigidBody = Physics.World.createRigidBody(rigidBodyDesc);

        // Add a Box Collider
        const colliderDesc = RAPIER.ColliderDesc.cuboid(1, 0.5, 2); // Dimensions match the car mesh
        Physics.World.createCollider(colliderDesc, this.rigidBody);
    }

    private createCarMesh(): THREE.Mesh {
        const carGeometry = new THREE.BoxGeometry(2, 1, 4);
        const carMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000 });
        const carMesh = new THREE.Mesh(carGeometry, carMaterial);
        carMesh.castShadow = true;
        carMesh.position.y = 6;
        scene.add(carMesh);
        return carMesh;
    }

    update(): void {
        // Sync mesh position/rotation with Rapier body
        const position = this.rigidBody.translation();
        const rotation = this.rigidBody.rotation();
        this.mesh.position.set(position.x, position.y, position.z);
        this.mesh.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    }

    static applySuspensionForces(car: { rigidBody: RAPIER.RigidBody }) {
        for (const offset of this.wheelOffsets) {

            // Transform wheel position to world space
            const wheelWorldPos = Physics.add(car.rigidBody.translation(), offset)
            const rayOrigin = Physics.add(wheelWorldPos, new RAPIER.Vector3(0, this.suspensionLength, 0));
            const rayDirection = new RAPIER.Vector3(0, -1, 0);

            // Perform raycasting
            const hit = Physics.World.castRay(
                new RAPIER.Ray(rayOrigin, rayDirection),
                this.suspensionLength,
                true
            );

            if (hit) {
                // Calculate compression (distance to ground)
                const compression = this.suspensionLength - hit.timeOfImpact;

                // Suspension force (Hooke's law: F = -kx)
                const force = compression * this.suspensionStiffness;

                // Damping force
                const relativeVelocity = Physics.linearVelocityAtWorldPoint(car.rigidBody, wheelWorldPos)
                const dampingForce = relativeVelocity.y * this.damping;

                // Apply total force at the wheel position
                const suspensionForce = new RAPIER.Vector3(0, force - dampingForce, 0);
                car.rigidBody.addForceAtPoint(suspensionForce, wheelWorldPos, true);
            }
        }
    }
}

// Third-Person Camera Class
class ThirdPersonCamera {
    camera: THREE.PerspectiveCamera;
    offset: THREE.Vector3;

    constructor(camera: THREE.PerspectiveCamera, offset: THREE.Vector3) {
        this.camera = camera;
        this.offset = offset;
    }

    update(target: THREE.Object3D): void {
        const targetPosition = target.position.clone();
        const offsetRotated = this.offset.clone().applyQuaternion(target.quaternion);

        const cameraPosition = targetPosition.clone().add(offsetRotated);
        this.camera.position.copy(cameraPosition);
        this.camera.lookAt(target.position);
    }
}

// Initialize Car
const car = new Car();

// Third-Person Camera
const thirdPersonCamera = new ThirdPersonCamera(camera, new THREE.Vector3(0, 5, 10));

// Orbit Camera
const orbitControls = new OrbitControls(camera, renderer.domElement);
orbitControls.target.set(0, 10, 10); // Center the controls on the car
orbitControls.enabled = false;

// Keyboard Handling
type KeyStates = { [key: string]: boolean };
const keys: KeyStates = {
    ArrowUp: false,
    ArrowDown: false,
    ArrowLeft: false,
    ArrowRight: false,
};

window.addEventListener('keydown', (event: KeyboardEvent) => {
    keys[event.code] = true;
});

window.addEventListener('keyup', (event: KeyboardEvent) => {
    keys[event.code] = false;
});

function handleKeyboardInput(): void {
    car.rigidBody.resetForces(true);
    car.rigidBody.resetTorques(true);

    if (keys.ArrowUp) {
        Physics.applyImpulse(car.rigidBody, new THREE.Vector3(0, 0, -1))
    }
    if (keys.ArrowDown) {
        Physics.applyImpulse(car.rigidBody, new THREE.Vector3(0, 0, 1))
    }
    if (keys.ArrowLeft) {
        Physics.applyTorque(car.rigidBody, new RAPIER.Vector3(0, 1, 0))
    }
    if (keys.ArrowRight) {
        Physics.applyTorque(car.rigidBody, new RAPIER.Vector3(0, -1, 0))
    }
}
// lil-gui for Camera Toggle and Lighting Controls
const gui = new GUI();
const panelSettings = {
    cameraMode: 'Orbit', // Default camera mode
    directionalLightIntensity: 2,
    ambientLightIntensity: 1.5,
};

// Camera mode dropdown
gui.add(panelSettings, 'cameraMode', ['Third-Person', 'Orbit']).name('Camera Mode');

// Lighting controls
gui.add(panelSettings, 'directionalLightIntensity', 0, 5, 0.1)
    .name('Directional Light')
    .onChange((value: number) => {
        directionalLight.intensity = value;
    });

gui.add(panelSettings, 'ambientLightIntensity', 0, 5, 0.1)
    .name('Ambient Light')
    .onChange((value: number) => {
        ambientLight.intensity = value;
    });





function spawnRandomObject() {
    const objectGeometries = [
        new THREE.SphereGeometry(1, 32, 32),
        new THREE.BoxGeometry(2, 2, 2),
        new THREE.ConeGeometry(1, 2, 32),
    ];
    const geometry = objectGeometries[Math.floor(Math.random() * objectGeometries.length)];
    const material = new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff });
    const mesh = new THREE.Mesh(geometry, material);

    // Random spawn position above terrain
    mesh.position.set(
        (Math.random() - 0.5) * 50, // X
        20 + Math.random() * 10,   // Y (above terrain)
        (Math.random() - 0.5) * 50 // Z
    );
    mesh.castShadow = true;
    mesh.receiveShadow = true;

    scene.add(mesh);

    // Create RigidBody and Collider
    const bodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(
        mesh.position.x,
        mesh.position.y,
        mesh.position.z
    );
    const rigidBody = Physics.World.createRigidBody(bodyDesc);

    let colliderDesc;
    if (geometry instanceof THREE.SphereGeometry) {
        colliderDesc = RAPIER.ColliderDesc.ball(1); // Sphere radius
    } else if (geometry instanceof THREE.BoxGeometry) {
        colliderDesc = RAPIER.ColliderDesc.cuboid(1, 1, 1); // Box half extents
    } else if (geometry instanceof THREE.ConeGeometry) {
        colliderDesc = RAPIER.ColliderDesc.cone(2, 1); // Cone height and radius
    }

    Physics.World.createCollider(colliderDesc, rigidBody);

    mesh.userData.rigidBody = rigidBody;

    return { mesh, rigidBody };
}

// Spawn 4 objects
for (let i = 0; i < 4; i++) {
    spawnRandomObject();
}


// Animation Loop
function animate(): void {
    requestAnimationFrame(animate);

    // Handle keyboard input
    handleKeyboardInput();

    // Apply suspension forces
    Car.applySuspensionForces(car);

    // Update car position and rotation based on physics
    car.update();


    // Synchronize Three.js objects with Rapier bodies
    scene.children.forEach((child) => {

        if (child.userData.rigidBody) {
            const rigidBody = child.userData.rigidBody;
            const position = rigidBody.translation();
            const rotation = rigidBody.rotation();
            child.position.set(position.x, position.y, position.z);
            child.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
        }
    });


    // Step the physics world
    Physics.World.step();


    // Update the selected camera
    if (panelSettings.cameraMode === 'Third-Person') {
        orbitControls.enabled = false;
        thirdPersonCamera.update(car.mesh);
    } else {
        orbitControls.enabled = true;
        orbitControls.update();
    }

    // Render the scene
    renderer.render(scene, camera);
}

animate();