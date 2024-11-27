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

export function createButton(
    label: string,
    callback: () => void,
    options?: { id?: string; className?: string; style?: Partial<CSSStyleDeclaration> }
): HTMLButtonElement {
    const button = document.createElement('button');
    button.textContent = label;
    button.addEventListener('click', callback);

    // Apply additional options if provided
    if (options?.id) button.id = options.id;
    if (options?.className) button.className = options.className;
    if (options?.style) Object.assign(button.style, options.style);

    return button;
}

export function createSlider(
    min: number,
    max: number,
    step: number,
    value: number,
    callback: (value: number) => void,
    options?: { id?: string; className?: string; style?: Partial<CSSStyleDeclaration> }
): HTMLInputElement {
    const slider = document.createElement('input');
    slider.type = 'range';
    slider.min = min.toString();
    slider.max = max.toString();
    slider.step = step.toString();
    slider.value = value.toString();
    slider.addEventListener('input', () => callback(parseFloat(slider.value)));

    // Apply additional options if provided
    if (options?.id) slider.id = options.id;
    if (options?.className) slider.className = options.className;
    if (options?.style) Object.assign(slider.style, options.style);

    return slider;
}

export function createCheckbox(
    checked: boolean,
    callback: (checked: boolean) => void,
    options?: { id?: string; className?: string; style?: Partial<CSSStyleDeclaration> }
): HTMLInputElement {
    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.checked = checked;
    checkbox.addEventListener('change', () => callback(checkbox.checked));

    // Apply additional options if provided
    if (options?.id) checkbox.id = options.id;
    if (options?.className) checkbox.className = options.className;
    if (options?.style) Object.assign(checkbox.style, options.style);

    return checkbox;
}
