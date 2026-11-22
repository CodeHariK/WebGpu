// Colour Blocks — the 3D preview's view: a three.js renderer in a given element, a camera you orbit
// (drag), pan (right drag) and zoom (wheel) round the map, sun and sky light with shadows, and the
// city (Map26Scene) swapped in whenever the map changes.
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import type { Blocks } from './Map26Render';
import { buildCity } from './Map26Scene';

export type View3D = { show: (p_b: Blocks) => void; dispose: () => void };

// p_screen: pixels on screen; p_size: the map's size (pixels: the scene's units).
export function createView(p_parent: HTMLElement, p_screen: number, p_size: number): View3D {
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(p_screen, p_screen);
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    p_parent.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.background = new THREE.Color('#bfe3f5');
    scene.fog = new THREE.Fog('#bfe3f5', p_size * 1.2, p_size * 3);
    const camera = new THREE.PerspectiveCamera(40, 1, 1, p_size * 6);
    camera.position.set(0, p_size * 0.62, p_size * 0.72);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.maxPolarAngle = Math.PI * 0.47; // never under the ground
    controls.minDistance = 20;
    controls.maxDistance = p_size * 2;
    scene.add(new THREE.HemisphereLight('#fff6e6', '#6b8f5a', 1.4));
    const sun = new THREE.DirectionalLight('#fff1d6', 2.2);
    sun.position.set(-p_size * 0.4, p_size * 0.7, -p_size * 0.25);
    sun.castShadow = true;
    sun.shadow.mapSize.set(4096, 4096);
    const half = p_size * 0.55;
    Object.assign(sun.shadow.camera, { left: -half, right: half, top: half, bottom: -half, near: 1, far: p_size * 3 });
    sun.shadow.bias = -0.0005;
    scene.add(sun);
    let city: ReturnType<typeof buildCity> | null = null;
    renderer.setAnimationLoop(() => {
        controls.update();
        renderer.render(scene, camera);
    });
    return {
        show: (p_b) => {
            if (city) {
                scene.remove(city.group);
                city.dispose();
            }
            city = buildCity(p_b, p_size);
            scene.add(city.group);
        },
        dispose: () => {
            renderer.setAnimationLoop(null);
            if (city) city.dispose();
            controls.dispose();
            renderer.dispose();
            renderer.domElement.remove();
        },
    };
}
