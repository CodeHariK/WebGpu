import * as THREE from 'three';

// Create the shader material
export const Height_Shader = new THREE.ShaderMaterial({
    vertexShader: `
        varying float vHeight;
        void main() {
            vec4 worldPosition = modelMatrix * vec4(position, 1.0);
            vHeight = worldPosition.y;
            gl_Position = projectionMatrix * viewMatrix * worldPosition;
        }
    `,
    fragmentShader: `
        precision mediump float;
        varying float vHeight;
        uniform vec3 lowColor;
        uniform vec3 highColor;
        uniform float minHeight;
        uniform float maxHeight;

        void main() {
            float normalizedHeight = clamp((vHeight - minHeight) / (maxHeight - minHeight), -0.5, 0.5) + 0.5;
            vec3 color = mix(lowColor, highColor, normalizedHeight);
            gl_FragColor = vec4(color, 1.0);
        }
    `,
    uniforms: {
        lowColor: { value: new THREE.Color(0x0000ff) },
        highColor: { value: new THREE.Color(0xff0000) },
        minHeight: { value: 0.0 },
        maxHeight: { value: 2 }
    }
});
