import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import * as RAPIER from '@dimforge/rapier3d';
import { Physics } from './physics';
import { CarGui } from './gui';
import { createTerrain } from './terrain';

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

export class AdvScene {

    scene: THREE.Scene

    renderer: THREE.WebGLRenderer

    orbitControls: OrbitControls
    thirdPersonCamera: ThirdPersonCamera
    camera: THREE.PerspectiveCamera

    constructor() {

        let scene = new THREE.Scene();

        const renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.shadowMap.enabled = true;
        renderer.shadowMap.type = THREE.PCFSoftShadowMap; // or other types of shadow maps
        this.renderer = renderer

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

        this.camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
        this.camera.position.set(0, 6, 8)
        this.camera.lookAt(0, 0, 0)

        // Third-Person Camera
        this.thirdPersonCamera = new ThirdPersonCamera(this.camera, new THREE.Vector3(0, 8, -8));

        // Orbit Camera
        this.orbitControls = new OrbitControls(this.camera, renderer.domElement);
        this.orbitControls.target.set(0, 0, 0); // Center the controls on the car
        this.orbitControls.enabled = false;

        this.scene = scene

        this.createGround()

        // // Spawn 4 objects
        // for (let i = 0; i < 40; i++) {
        //     CarScene.spawnRandomObject();
        // }
    }

    createGround() {
        //Ground
        const groundGeometry = new THREE.PlaneGeometry(100, 100);
        const groundMaterial = new THREE.MeshToonMaterial({ color: 0x228b22 });
        const ground = new THREE.Mesh(groundGeometry, groundMaterial);
        ground.rotation.x = -Math.PI / 2;
        ground.receiveShadow = true;
        const groundColliderDesc = RAPIER.ColliderDesc.cuboid(50, 0.1, 50).setTranslation(0, -.1, 0);
        Physics.World.createCollider(groundColliderDesc);

        this.scene.add(ground);

        // createTerrain(this.scene, Physics.World)
    }

    spawnRandomObject() {
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

        this.scene.add(mesh);

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

    update(mesh: THREE.Mesh) {
        // Update the selected camera
        if (CarGui.panelSettings.cameraMode === 'TPS') {
            this.orbitControls.enabled = false;
            this.thirdPersonCamera.update(mesh);
        } else {
            this.orbitControls.enabled = true;
            this.orbitControls.update();
        }

        // let material = new THREE.LineBasicMaterial({
        //     color: 0xffffff,
        //     vertexColors: true,
        // });
        // let geometry = new THREE.BufferGeometry();
        // let lines = new THREE.LineSegments(geometry, material);
        // let buffers = Physics.World.debugRender();
        // lines.visible = true;
        // lines.geometry.setAttribute(
        //     "position",
        //     new THREE.BufferAttribute(buffers.vertices, 3),
        // );
        // lines.geometry.setAttribute(
        //     "color",
        //     new THREE.BufferAttribute(buffers.colors, 4),
        // );
        // Render the scene
        this.renderer.render(this.scene, this.camera);
    }
}