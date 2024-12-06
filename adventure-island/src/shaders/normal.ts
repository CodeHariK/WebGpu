import * as THREE from 'three';

const textureLoader = new THREE.TextureLoader();
const normalMap = textureLoader.load('src/shaders/NormalMap.png');
normalMap.wrapS = normalMap.wrapT = THREE.RepeatWrapping;

export const Normal_Shader = new THREE.ShaderMaterial({
    uniforms: {
        uNormalMap: { value: normalMap }, // Pass the loaded normal map
        uNormalScale: { value: new THREE.Vector2(1, 1) }, // Scale of the normal map
        uLightDirection: { value: new THREE.Vector3(0, 1, 1).normalize() }, // Directional light
    },
    vertexShader: `
        varying vec3 vNormal;
        varying vec3 vPosition;
        
        void main() {
            vNormal = normalMatrix * normal; // Transform normal into world space
            vPosition = (modelViewMatrix * vec4(position, 1.0)).xyz; // Pass position to fragment shader
            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); // Final vertex position
        }
    `,
    fragmentShader: `
        uniform sampler2D uNormalMap; // The normal map texture
        uniform vec2 uNormalScale;   // Controls the intensity of the normal map
        uniform vec3 uLightDirection; // Light direction for shading

        varying vec3 vNormal;   // The normal vector from the vertex shader
        varying vec3 vPosition; // The position from the vertex shader
    
        void main() {
            // Sample the normal map
            vec3 normal = texture2D(uNormalMap, vPosition.xy * .5 + .5 ).rgb;
            
            // Convert normal from [0,1] to [-1,1]
            normal = normalize(normal * 2.0 - 1.0);

            // Apply the normal scale to adjust the bumpiness
            normal.xy *= uNormalScale;
    
            // Calculate lighting using the dot product of light and normal
            float light = dot(normalize(normal), normalize(uLightDirection));
            light = max(light, 0.0); // Make sure light is not negative

            // Set the final color of the fragment (pixel)
            gl_FragColor = vec4(vec3(light), 1.0);
        }
    `,
});
