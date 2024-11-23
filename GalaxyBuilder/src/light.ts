import * as THREE from 'three';

export function AddLight(scene: THREE.Scene) {
    // Add ambient light to softly illuminate the scene
    const ambientLight = new THREE.AmbientLight(0x606060, 3);
    scene.add(ambientLight);

    // Add a directional light to simulate sunlight
    const sunLight = new THREE.DirectionalLight(0xffffff, 2);
    sunLight.position.set(10, 20, 10);
    sunLight.castShadow = true;
    sunLight.shadow.mapSize.width = 1024;
    sunLight.shadow.mapSize.height = 1024;
    sunLight.shadow.camera.near = 0.5;
    sunLight.shadow.camera.far = 50;
    scene.add(sunLight);
}
