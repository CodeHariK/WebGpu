import * as THREE from 'three';

export const Height_Shader = (minHeight: number, maxHeight: number) => new THREE.ShaderMaterial({
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
        uniform vec3 midLowColor;
        uniform vec3 midHighColor;
        uniform vec3 highColor;
        uniform float minHeight;
        uniform float maxHeight;

        void main() {
            // Normalize height to range [0, 1]
            float normalizedHeight = clamp((vHeight - minHeight) / (maxHeight - minHeight), 0.0, 1.0);

            // Create weights for each segment using smoothstep
            vec3 color = mix(lowColor, midLowColor, smoothstep(0.0, 0.25, normalizedHeight));
            color = mix(color, midHighColor, smoothstep(0.25, 0.5, normalizedHeight));
            color = mix(color, highColor, smoothstep(0.5, 1.0, normalizedHeight));

            gl_FragColor = vec4(color, 1.0);
        }
    `,
    uniforms: {
        lowColor: { value: new THREE.Color(0x74ccf4) },
        midLowColor: { value: new THREE.Color(0x00ff00) },
        midHighColor: { value: new THREE.Color(0xffff00) },
        highColor: { value: new THREE.Color(0xff0000) },

        minHeight: { value: minHeight },
        maxHeight: { value: maxHeight }
    }
});
