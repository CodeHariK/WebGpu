import * as THREE from 'three';
import { CSS2DObject, CSS2DRenderer } from 'three/addons/renderers/CSS2DRenderer.js';

export function CreateCSS2dRenderer(): CSS2DRenderer {
    const labelRenderer = new CSS2DRenderer();
    labelRenderer.setSize(window.innerWidth, window.innerHeight);
    labelRenderer.domElement.style.position = 'absolute';
    labelRenderer.domElement.style.top = '0px';
    labelRenderer.domElement.style.pointerEvents = 'none';
    document.getElementById("app")?.appendChild(labelRenderer.domElement)
    return labelRenderer
}

export function AddLabel(name: string, mesh: THREE.Mesh): CSS2DObject {
    const labelDiv = document.createElement('div');
    labelDiv.classList.add("label")
    labelDiv.innerHTML = name.replace(/\n/g, '<br>');
    labelDiv.style.color = 'white';
    labelDiv.style.fontFamily = 'monospace';
    const label = new CSS2DObject(labelDiv);
    label.position.set(0, -0.5, 0);
    mesh.add(label);
    return label
}
