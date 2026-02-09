import * as THREE from 'three';

// Create the shader material
export const UV_Shader = new THREE.ShaderMaterial({
    vertexShader: `
        varying vec2 vUv;
        void main() {
            vUv = uv;
            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
    `,
    fragmentShader: `
        varying vec2 vUv;
        void main() {
            gl_FragColor = vec4(mod(vUv.x,1.0), mod(vUv.y,1.0), 0.0, 1.0);
        }
    `,
});
