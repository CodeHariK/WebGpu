import * as THREE from 'three';

const textureLoader = new THREE.TextureLoader();

// Load the diffuse texture and normal map
const diffuseTexture1 = textureLoader.load('src/assets/texture/grass_texture.jpg');
const diffuseTexture2 = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_basecolor.jpg');

diffuseTexture1.wrapS = THREE.RepeatWrapping;
diffuseTexture1.wrapT = THREE.RepeatWrapping;
diffuseTexture1.minFilter = THREE.LinearMipMapLinearFilter;
diffuseTexture1.magFilter = THREE.LinearFilter;

diffuseTexture2.wrapS = THREE.RepeatWrapping;
diffuseTexture2.wrapT = THREE.RepeatWrapping;
diffuseTexture2.minFilter = THREE.LinearMipMapLinearFilter;
diffuseTexture2.magFilter = THREE.LinearFilter;

export const HeightTextureBlend_Shader = (minHeight: number, maxHeight: number) => new THREE.ShaderMaterial({
    vertexShader: `
        varying vec3 vPos;
        varying vec3 vNormal;
        varying vec2 vUv;

        void main() {
            vUv = uv;

            vec4 worldPosition = (modelMatrix * vec4(position, 1.0));
            vPos = worldPosition.xyz;

            vNormal = normalize(mat3(modelMatrix) * normal); // Transform normal to world space

            gl_Position = projectionMatrix * viewMatrix * worldPosition;
        }
    `,
    fragmentShader: `
        precision mediump float;

        varying vec3 vPos;
        varying vec3 vNormal;
        varying vec2 vUv;

        uniform vec3 midLowColor;
        uniform vec3 midHighColor;

        uniform sampler2D uDiffuseTextureLow;
        uniform sampler2D uDiffuseTextureHigh;

        uniform float minHeight;
        uniform float maxHeight;

        void main() {

            vec3 diffuseColorLow = texture2D(uDiffuseTextureLow, vUv).rgb;
            vec3 diffuseColorHigh = texture2D(uDiffuseTextureHigh, vUv).rgb;

            // Normalize height to range [0, 1]
            float normalizedHeight = clamp((vPos.y - minHeight) / (maxHeight - minHeight), 0.0, 1.0);

            // Create weights for each segment using smoothstep
            vec3 color = mix(midLowColor, diffuseColorLow, smoothstep(0.0, 0.25, normalizedHeight));
            color = mix(color, diffuseColorHigh, smoothstep(0.25, 0.5, normalizedHeight));
            color = mix(color, midHighColor, smoothstep(0.5, 1.0, normalizedHeight));

            gl_FragColor = vec4(color, 1.0);
        }
    `,
    uniforms: {
        uDiffuseTextureLow: { value: diffuseTexture2 },
        uDiffuseTextureHigh: { value: diffuseTexture1 },

        midLowColor: { value: new THREE.Color(0x5cb5e1) },
        midHighColor: { value: new THREE.Color(0xffffff) },

        minHeight: { value: minHeight },
        maxHeight: { value: maxHeight }
    }
});
