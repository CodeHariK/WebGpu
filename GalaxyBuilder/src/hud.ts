import * as THREE from 'three';

export function CreateHUDView(sceneHUD: THREE.Scene, ASPECT_RATIO: number) {
    const hudGeometry = new THREE.PlaneGeometry(0.1, 0.1);
    const hudMaterial = new THREE.MeshMatcapMaterial({ color: 0xff0000 });
    const hudElement = new THREE.Mesh(hudGeometry, hudMaterial);
    hudElement.position.set(-ASPECT_RATIO * .9, -0.9, 0);
    sceneHUD.add(hudElement);
}
