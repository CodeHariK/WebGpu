import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { CarGui } from './gui';
import { CreateCSS2dRenderer } from './ui';
import RAPIER from '@dimforge/rapier3d';

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

export class Game {

    SCENE: THREE.Scene

    WORLD = new RAPIER.World(new THREE.Vector3(0, -9.81, 0)); // Gravity

    renderer: THREE.WebGLRenderer

    LABEL_RENDERER = CreateCSS2dRenderer()

    orbitControls: OrbitControls
    thirdPersonCamera: ThirdPersonCamera
    camera: THREE.PerspectiveCamera

    previousTime: number

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

        this.camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
        this.camera.position.set(0, 6, -8)
        this.camera.lookAt(0, 0, 0)

        // Third-Person Camera
        this.thirdPersonCamera = new ThirdPersonCamera(this.camera, new THREE.Vector3(0, 6, -8));

        // Orbit Camera
        this.orbitControls = new OrbitControls(this.camera, renderer.domElement);
        this.orbitControls.target.set(0, 0, 0); // Center the controls on the car
        this.orbitControls.enabled = false;

        this.SCENE = scene
    }

    animate(customUpdate: (deltaTime: number) => void) {
        this.renderer.setAnimationLoop(() => { this.update(customUpdate) });
    }

    cameraUpdate(mesh: THREE.Mesh) {
        if (CarGui.panelSettings.cameraMode === 'TPS') {
            this.orbitControls.enabled = false;
            this.thirdPersonCamera.update(mesh);
        } else {
            this.orbitControls.enabled = true;
            this.orbitControls.update();
        }

        this.LABEL_RENDERER.render(this.SCENE, this.camera);
    }

    update(customUpdate: (deltaTime: number) => void) {

        const currentTime = performance.now();
        const deltaTime = (currentTime - this.previousTime) / 1000; // Convert milliseconds to seconds
        this.previousTime = currentTime;

        customUpdate(deltaTime)

        // Synchronize Three.js objects with Rapier bodies
        this.SCENE.children.forEach((child) => {

            if (child.userData.rigidBody) {
                const rigidBody = child.userData.rigidBody;
                const position = rigidBody.translation();
                const rotation = rigidBody.rotation();
                child.position.set(position.x, position.y, position.z);
                child.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
            }
        });

        // Step the physics world
        this.WORLD.step();

        this.renderer.render(this.SCENE, this.camera);
    }
}