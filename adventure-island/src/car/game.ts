import * as THREE from 'three';
(window as any).THREE = THREE; // Expose THREE globally

import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GUI } from 'three/addons/libs/lil-gui.module.min.js';
import { Sky } from 'three/addons/objects/Sky.js';
import { CreateCSS2dRenderer, hudClear as updateHUDClear } from './ui';
import RAPIER from '@dimforge/rapier3d';
import Stats from 'three/addons/libs/stats.module.js'
import { TextureSample_Shader } from '../shaders/texturesample';

class RapierDebugRenderer {
    mesh: THREE.LineSegments
    world: RAPIER.World
    enabled = false

    constructor(scene: THREE.Scene, world: RAPIER.World) {
        this.world = world
        this.mesh = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ color: 0xffffff, vertexColors: true }))
        this.mesh.frustumCulled = false
        scene.add(this.mesh)
    }

    update() {
        if (this.enabled) {
            const { vertices, colors } = this.world.debugRender()
            this.mesh.geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3))
            this.mesh.geometry.setAttribute('color', new THREE.BufferAttribute(colors, 4))
            this.mesh.visible = true
        } else {
            this.mesh.visible = false
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
        cameraPosition.y = Math.max(targetPosition.y + 2, cameraPosition.y)

        this.camera.position.copy(cameraPosition);

        this.camera.lookAt(target.position);
    }
}

export class Game {

    SCENE: THREE.Scene

    static gravity = 30

    WORLD = new RAPIER.World(new THREE.Vector3(0, -Game.gravity, 0)); // Gravity

    RENDERER: THREE.WebGLRenderer

    LABEL_RENDERER = CreateCSS2dRenderer()

    orbitControls: OrbitControls
    thirdPersonCamera: ThirdPersonCamera
    CAMERA: THREE.PerspectiveCamera

    LODS: THREE.LOD[] = []

    rapierDebugRenderer: RapierDebugRenderer

    sky: Sky
    sun: THREE.Vector3

    CLOCK = new THREE.Clock()

    stats = new Stats()

    constructor() {

        document.body.appendChild(this.stats.dom)
        this.stats.dom.style.left = '50%';

        let scene = new THREE.Scene();

        const renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.shadowMap.enabled = true;
        renderer.shadowMap.type = THREE.PCFSoftShadowMap; // or other types of shadow maps
        renderer.setPixelRatio(window.devicePixelRatio);
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        renderer.toneMappingExposure = 0.5;
        this.RENDERER = renderer

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

        this.CAMERA = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
        this.CAMERA.position.set(0, 4, -4)
        this.CAMERA.lookAt(0, 0, 0)

        // Third-Person Camera
        this.thirdPersonCamera = new ThirdPersonCamera(this.CAMERA, new THREE.Vector3(0, 2, -4));

        // Orbit Camera
        this.orbitControls = new OrbitControls(this.CAMERA, renderer.domElement);
        this.orbitControls.target.set(0, 0, 0); // Center the controls on the car
        this.orbitControls.enabled = false;
        this.orbitControls.enablePan = true
        this.orbitControls.enableZoom = true

        this.SCENE = scene

        this.initSky();

        this.rapierDebugRenderer = new RapierDebugRenderer(this.SCENE, this.WORLD)

        window.addEventListener('resize', () => {
            this.CAMERA.aspect = window.innerWidth / window.innerHeight;
            this.CAMERA.updateProjectionMatrix();
            renderer.setSize(window.innerWidth, window.innerHeight);
        });
    }

    animate(customUpdate: (deltaTime: number) => void) {
        this.RENDERER.setAnimationLoop(() => { this.update(customUpdate) });
    }

    cameraUpdate(mesh: THREE.Mesh, cameraMode: 'TPS' | 'Orbit') {
        if (cameraMode === 'TPS') {
            this.orbitControls.enabled = false;
            this.thirdPersonCamera.update(mesh);
        } else {
            this.orbitControls.enabled = true;
            this.orbitControls.update();
            this.orbitControls.target = mesh.position
        }

        this.LABEL_RENDERER.render(this.SCENE, this.CAMERA);
    }

    update(customUpdate: (deltaTime: number) => void) {

        updateHUDClear()

        customUpdate(this.CLOCK.getDelta())

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

        this.LODS.forEach((lod) => {
            lod.update(this.CAMERA)
        })

        // Step the physics world
        this.WORLD.step();

        this.RENDERER.render(this.SCENE, this.CAMERA);

        this.rapierDebugRenderer.update()

        this.textureUpdate()

        this.stats.update()
    }

    textureUpdate() {
        TextureSample_Shader.uniforms.uCameraPosition.value = this.CAMERA.position;
    }

    initSky() {

        // Add Sky
        this.sky = new Sky();
        this.sky.scale.setScalar(450000);
        this.SCENE.add(this.sky);

        this.sun = new THREE.Vector3();

        /// GUI

        const effectController = {
            turbidity: 10,
            rayleigh: 3,
            mieCoefficient: 0.005,
            mieDirectionalG: 0.7,
            elevation: 2,
            azimuth: 180,
            exposure: this.RENDERER.toneMappingExposure
        };

        const guiChanged = () => {
            const uniforms = this.sky.material.uniforms;
            uniforms['turbidity'].value = effectController.turbidity;
            uniforms['rayleigh'].value = effectController.rayleigh;
            uniforms['mieCoefficient'].value = effectController.mieCoefficient;
            uniforms['mieDirectionalG'].value = effectController.mieDirectionalG;

            const phi = THREE.MathUtils.degToRad(90 - effectController.elevation);
            const theta = THREE.MathUtils.degToRad(effectController.azimuth);

            this.sun.setFromSphericalCoords(1, phi, theta);

            uniforms['sunPosition'].value.copy(this.sun);

            this.RENDERER.toneMappingExposure = effectController.exposure;
            this.RENDERER.render(this.SCENE, this.CAMERA);

        }

        const gui = new GUI();

        gui.add(effectController, 'turbidity', 0.0, 20.0, 0.1).onChange(guiChanged);
        gui.add(effectController, 'rayleigh', 0.0, 4, 0.001).onChange(guiChanged);
        gui.add(effectController, 'mieCoefficient', 0.0, 0.1, 0.001).onChange(guiChanged);
        gui.add(effectController, 'mieDirectionalG', 0.0, 1, 0.001).onChange(guiChanged);
        gui.add(effectController, 'elevation', 0, 90, 0.1).onChange(guiChanged);
        gui.add(effectController, 'azimuth', - 180, 180, 0.1).onChange(guiChanged);
        gui.add(effectController, 'exposure', 0, 1, 0.0001).onChange(guiChanged);

        guiChanged();
    }
}