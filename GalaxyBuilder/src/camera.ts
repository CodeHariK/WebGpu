import * as THREE from 'three';

import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { CSS2DRenderer, CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';


export function CreateOrbitControls(camera: THREE.PerspectiveCamera | THREE.OrthographicCamera, renderer: THREE.WebGLRenderer) {
    const orbit_control = new OrbitControls(camera, renderer.domElement);
    orbit_control.enableDamping = true;
    orbit_control.enableZoom = true
    orbit_control.enableRotate = true
    orbit_control.enablePan = true
    orbit_control.dampingFactor = 0.05;    // Lower values for smoother orbiting
    orbit_control.minDistance = 5;         // Minimum zoom distance
    orbit_control.maxDistance = 20;        // Maximum zoom distance
    orbit_control.maxPolarAngle = Math.PI / 2; // Limit vertical movement
    return orbit_control
}

export function CreateRenderer(): THREE.WebGLRenderer {
    const renderer = new THREE.WebGLRenderer();
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.shadowMap.enabled = true;
    renderer.setSize(window.innerWidth, window.innerHeight);
    document.body.appendChild(renderer.domElement);
    return renderer
}

export function CreateCSS2dRenderer(): CSS2DRenderer {
    const labelRenderer = new CSS2DRenderer();
    labelRenderer.setSize(window.innerWidth, window.innerHeight);
    labelRenderer.domElement.style.position = 'absolute';
    labelRenderer.domElement.style.top = '0px';
    labelRenderer.domElement.style.pointerEvents = 'none';
    document.body.appendChild(labelRenderer.domElement)
    return labelRenderer
}

export function CreateHUDCamera(aspectRatio: number): THREE.OrthographicCamera {
    const HUD_ORTHOGRAPHIC_CAMERA = new THREE.OrthographicCamera(
        -aspectRatio, aspectRatio, 1, -1, 0.1, 10
    );
    HUD_ORTHOGRAPHIC_CAMERA.position.z = 5;
    return HUD_ORTHOGRAPHIC_CAMERA
}

export function ResizeReset(ACTIVE_CAMERA: THREE.PerspectiveCamera | THREE.OrthographicCamera, ASPECT_RATIO: number, renderer: THREE.WebGLRenderer) {
    window.addEventListener('resize', () => {

        ASPECT_RATIO = window.innerWidth / window.innerHeight;

        // Update the active camera's aspect ratio
        if (ACTIVE_CAMERA instanceof THREE.PerspectiveCamera) {
            ACTIVE_CAMERA.aspect = ASPECT_RATIO;
            ACTIVE_CAMERA.updateProjectionMatrix();
        } else if (ACTIVE_CAMERA instanceof THREE.OrthographicCamera) {
            // Adjust orthographic camera's frustum on resize
            ACTIVE_CAMERA.left = -10 * ASPECT_RATIO;
            ACTIVE_CAMERA.right = 10 * ASPECT_RATIO;
            ACTIVE_CAMERA.top = 10;
            ACTIVE_CAMERA.bottom = -10;
            ACTIVE_CAMERA.updateProjectionMatrix();
        }

        renderer.setSize(window.innerWidth, window.innerHeight);
    });
}