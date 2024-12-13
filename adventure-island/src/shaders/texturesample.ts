import * as THREE from 'three';

const textureLoader = new THREE.TextureLoader();

// Load the diffuse texture and normal map
const diffuseTexture = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_basecolor.jpg');
const normalMap = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_normal.jpg');

export const TextureSample_Shader = new THREE.ShaderMaterial({
    uniforms: {
        uDiffuseTexture: { value: diffuseTexture },
        uNormalMap: { value: normalMap },
        uLightPosition: { value: new THREE.Vector3(10.0, 20.0, 10.0) },
        uCameraPosition: { value: new THREE.Vector3(0.0, 10.0, 0.0) },
    },
    vertexShader: `
        varying vec2 vUv;
        varying vec3 vNormal;
        varying vec3 vFragPos;

        void main() {
            vUv = uv;
            vNormal = normalize(normalMatrix * normal); // Transform normal to view space
            vFragPos = (modelViewMatrix * vec4(position, 1.0)).xyz; // Fragment position in view space
            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
    `,
    fragmentShader: `
        varying vec2 vUv;
        varying vec3 vNormal;
        varying vec3 vFragPos;

        uniform sampler2D uDiffuseTexture;
        uniform sampler2D uNormalMap;
        uniform vec3 uLightPosition;
        uniform vec3 uCameraPosition;

        void main() {
            // Sample the diffuse texture
            vec4 diffuseColor = texture2D(uDiffuseTexture, vUv);

            // Sample and transform the normal map
            vec3 normal = texture2D(uNormalMap, vUv).rgb;
            normal = normalize(normal * 2.0 - 1.0); // Convert [0,1] range to [-1,1]

            // Calculate lighting
            vec3 lightDir = normalize(uLightPosition - vFragPos);
            float diff = max(dot(normal, lightDir), 0.0); // Diffuse component

            // Calculate view direction and specular highlights
            vec3 viewDir = normalize(uCameraPosition - vFragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0); // Specular component

            // Combine components
            vec3 color = diffuseColor.rgb * diff + vec3(spec);
            gl_FragColor = vec4(color, diffuseColor.a);
        }
    `,
});
