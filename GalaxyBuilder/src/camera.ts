import * as THREE from 'three';

import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { CSS2DRenderer } from 'three/addons/renderers/CSS2DRenderer.js';
import { Game } from './game';


export function CreateOrbitControls(game: Game) {
    const orbit_control = new OrbitControls(game.ACTIVE_CAMERA, game.RENDERER.domElement);
    orbit_control.enableDamping = true;
    orbit_control.enableZoom = true
    orbit_control.enableRotate = true
    orbit_control.enablePan = true
    orbit_control.dampingFactor = 0.05;    // Lower values for smoother orbiting
    orbit_control.minDistance = 5;         // Minimum zoom distance
    orbit_control.maxDistance = 20;        // Maximum zoom distance
    orbit_control.maxPolarAngle = Math.PI / 2; // Limit vertical movement

    game.ORBIT_CONTROLS = orbit_control

    return orbit_control
}

export function CreateRenderer(): THREE.WebGLRenderer {
    const renderer = new THREE.WebGLRenderer();
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.shadowMap.enabled = true;
    renderer.autoClear = false;
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

export function CreateHUDCamera(aspectRatio: number, mag: number): THREE.OrthographicCamera {
    const HUD_ORTHOGRAPHIC_CAMERA = new THREE.OrthographicCamera(
        -aspectRatio * mag, aspectRatio * mag, mag, -mag, 0.1, 10
    );
    HUD_ORTHOGRAPHIC_CAMERA.position.z = 5;
    return HUD_ORTHOGRAPHIC_CAMERA
}

export function ResizeReset(game: Game) {
    window.addEventListener('resize', () => {

        game.ASPECT_RATIO = window.innerWidth / window.innerHeight;

        // Update the active camera's aspect ratio
        if (game.ACTIVE_CAMERA instanceof THREE.PerspectiveCamera) {
            game.ACTIVE_CAMERA.aspect = game.ASPECT_RATIO;
            game.ACTIVE_CAMERA.updateProjectionMatrix();
        } else if (game.ACTIVE_CAMERA instanceof THREE.OrthographicCamera) {
            // Adjust orthographic camera's frustum on resize
            game.ACTIVE_CAMERA.left = -10 * game.ASPECT_RATIO;
            game.ACTIVE_CAMERA.right = 10 * game.ASPECT_RATIO;
            game.ACTIVE_CAMERA.top = 10;
            game.ACTIVE_CAMERA.bottom = -10;
            game.ACTIVE_CAMERA.updateProjectionMatrix();
        }

        game.RENDERER.setSize(window.innerWidth, window.innerHeight);
    });
}

export function CreateNewCamera(game: Game, perspective: boolean) {
    game.ACTIVE_CAMERA.clear();

    // Switch active camera based on GUI selection
    if (perspective) {
        game.ACTIVE_CAMERA = new THREE.PerspectiveCamera(75, game.ASPECT_RATIO, 0.1, 1000);
        game.ACTIVE_CAMERA.position.set(-2, 5, 5);
    } else {
        const viewSize = 3;
        game.ACTIVE_CAMERA = new THREE.OrthographicCamera(
            -viewSize * game.ASPECT_RATIO, viewSize * game.ASPECT_RATIO,
            viewSize, -viewSize,
            0.1, 1000
        );
        game.ACTIVE_CAMERA.position.set(3, 3, 6);
    }
    game.ACTIVE_CAMERA.lookAt(0, 0, 0);

    game.ORBIT_CONTROLS.dispose();

    game.ORBIT_CONTROLS = CreateOrbitControls(game);
}
