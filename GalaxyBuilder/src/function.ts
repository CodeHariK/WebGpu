import * as THREE from 'three';

import { CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';

export function AddLabel(name: string, mesh: THREE.Mesh) {
    const labelDiv = document.createElement('div');
    labelDiv.classList.add("label")
    labelDiv.innerHTML = name.replace(/\n/g, '<br>');
    labelDiv.style.color = 'white';
    const label = new CSS2DObject(labelDiv);
    label.position.set(0, -0.5, 0);
    mesh.add(label);
}

export function isBitSet(number: number, position: number) {
    return (number & (1 << position)) !== 0;
}

export function setBit(number: number, position: number) {
    return number | (1 << position);
}
