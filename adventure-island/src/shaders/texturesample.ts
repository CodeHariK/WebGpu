import * as THREE from 'three';

const textureLoader = new THREE.TextureLoader();

// Load the diffuse texture and normal map
// const diffuseTexture = textureLoader.load('src/assets/texture/grass_texture.jpg');
const diffuseTexture = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_basecolor.jpg');
const normalMap = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_normal.jpg');


diffuseTexture.wrapS = THREE.RepeatWrapping;
diffuseTexture.wrapT = THREE.RepeatWrapping;
normalMap.wrapS = THREE.RepeatWrapping;
normalMap.wrapT = THREE.RepeatWrapping;

diffuseTexture.minFilter = THREE.LinearMipMapLinearFilter;
diffuseTexture.magFilter = THREE.LinearFilter;
normalMap.minFilter = THREE.LinearMipMapLinearFilter;
normalMap.magFilter = THREE.LinearFilter;

export const TextureSample_Shader = new THREE.ShaderMaterial({
    uniforms: {
        uDiffuseTexture: { value: diffuseTexture },
        uNormalMap: { value: normalMap },
        uLightPosition: { value: new THREE.Vector3(0.0, 50.0, 0.0) },

        uCameraPosition: { value: new THREE.Vector3(0.0, 0.0, 0.0) },
    },
    vertexShader: `
        varying vec2 vUv;
        varying vec3 worldPos;
        varying vec3 vNormal;

        void main() {
            vUv = uv;

            vNormal = normalize(mat3(modelMatrix) * normal); // Transform normal to world space

            worldPos = (modelMatrix * vec4(position, 1.0)).xyz;

            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
    `,
    fragmentShader: `
        varying vec2 vUv;
        varying vec3 worldPos;
        varying vec3 vNormal;

        uniform sampler2D uDiffuseTexture;
        uniform sampler2D uNormalMap;
        uniform vec3 uCameraPosition;
        uniform vec3 uLightPosition;

        void main() {

            // Sample textures
            vec4 diffuseColor = texture2D(uDiffuseTexture, vUv);
            vec3 texnormal = texture2D(uNormalMap, vUv).rgb;
            texnormal = normalize(texnormal * 2.0 - 1.0); // Convert [0,1] to [-1,1]

            // Combine the geometry normal (vNormal) and the normal map
            vec3 normal = normalize(vNormal + texnormal);

            vec3 lightDir = normalize(uLightPosition - worldPos) * 1.0;
            vec3 viewDir = normalize(uCameraPosition - worldPos);
            vec3 halfVec = normalize(lightDir + viewDir);

            // Diffuse term (Lambertian reflectance)
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = diff * diffuseColor.rgb;

            // Specular term (Blinn-Phong model)
            float spec = pow(max(dot(normal, halfVec), 0.0), 64.0);
            vec3 specular = vec3(1.0) * spec * vec3(1.0,1.0,1.0);

            vec3 ambient = vec3(0.1) * diffuseColor.rgb;

            // Combine lighting components
            vec3 finalColor = ambient + diffuse + specular;

            // Output the final color
            gl_FragColor = vec4(finalColor, diffuseColor.a);
        }
    `,
});
