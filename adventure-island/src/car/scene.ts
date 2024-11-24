import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import * as RAPIER from '@dimforge/rapier3d';
import { createTerrain } from './terrain';
import { Physics } from './physics';
import { Car } from './car';
import { CarGui } from './gui';
import { Keyboard } from './keyboard';

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


export class CarScene {

    static scene: THREE.Scene

    static renderer: THREE.WebGLRenderer

    static orbitControls: OrbitControls
    static thirdPersonCamera: ThirdPersonCamera
    static camera: THREE.PerspectiveCamera

    static create() {

        let scene = new THREE.Scene();

        const renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.shadowMap.enabled = true;
        renderer.shadowMap.type = THREE.PCFSoftShadowMap; // or other types of shadow maps
        CarScene.renderer = renderer

        document.body.appendChild(renderer.domElement);

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

        const axesHelper = new THREE.AxesHelper(5);
        scene.add(axesHelper);

        CarScene.camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
        CarScene.camera.position.set(0, 6, 8)
        CarScene.camera.lookAt(0, 0, 0)

        // Third-Person Camera
        CarScene.thirdPersonCamera = new ThirdPersonCamera(CarScene.camera, new THREE.Vector3(0, 8, -8));

        // Orbit Camera
        CarScene.orbitControls = new OrbitControls(CarScene.camera, renderer.domElement);
        CarScene.orbitControls.target.set(0, 0, 0); // Center the controls on the car
        CarScene.orbitControls.enabled = false;

        CarScene.scene = scene

        CarScene.createTerrain()

        // // Spawn 4 objects
        // for (let i = 0; i < 40; i++) {
        //     CarScene.spawnRandomObject();
        // }

        return scene
    }

    static createTerrain() {
        //Ground
        const groundGeometry = new THREE.PlaneGeometry(100, 100);
        const groundMaterial = new THREE.MeshToonMaterial({ color: 0x228b22 });
        const ground = new THREE.Mesh(groundGeometry, groundMaterial);
        ground.rotation.x = -Math.PI / 2;
        ground.receiveShadow = true;
        const groundColliderDesc = RAPIER.ColliderDesc.cuboid(50, 0.1, 50).setTranslation(0, 0.1, 0);
        Physics.World.createCollider(groundColliderDesc);

        CarScene.scene.add(ground);

        // createTerrain(scene, Physics.World)
    }

    static spawnRandomObject() {
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

        CarScene.scene.add(mesh);

        // Create RigidBody and Collider
        const bodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(
            mesh.position.x,
            mesh.position.y,
            mesh.position.z
        );
        const rigidBody = Physics.World.createRigidBody(bodyDesc);

        let colliderDesc: RAPIER.ColliderDesc;
        if (geometry instanceof THREE.SphereGeometry) {
            colliderDesc = RAPIER.ColliderDesc.ball(1); // Sphere radius
        } else if (geometry instanceof THREE.BoxGeometry) {
            colliderDesc = RAPIER.ColliderDesc.cuboid(1, 1, 1); // Box half extents
        } else if (geometry instanceof THREE.ConeGeometry) {
            colliderDesc = RAPIER.ColliderDesc.cone(1, 1); // Cone height and radius
        }

        Physics.World.createCollider(colliderDesc, rigidBody);

        mesh.userData = { name: "Obj", rigidBody: rigidBody };

        return { mesh, rigidBody };
    }

    static update(mesh: THREE.Mesh) {
        // Update the selected camera
        if (CarGui.panelSettings.cameraMode === 'TPS') {
            CarScene.orbitControls.enabled = false;
            CarScene.thirdPersonCamera.update(mesh);
        } else {
            CarScene.orbitControls.enabled = true;
            CarScene.orbitControls.update();
        }

        // Render the scene
        CarScene.renderer.render(CarScene.scene, CarScene.camera);
    }
}