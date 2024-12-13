import * as THREE from 'three';

export const Phong_Shader = new THREE.ShaderMaterial({
    vertexShader: `
        varying vec3 vNormal;
        varying vec3 vPosition;

        void main() {
            // Pass the normal and position to the fragment shader
            vNormal = normalize(normalMatrix * normal);
            vPosition = vec3(modelViewMatrix * vec4(position, 1.0));
            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
    `,
    fragmentShader: `
        precision highp float;

        varying vec3 vNormal;
        varying vec3 vPosition;

        uniform vec3 lightPosition;  // Light source position
        uniform vec3 ambientColor;   // Ambient light color
        uniform vec3 diffuseColor;   // Diffuse material color
        uniform vec3 specularColor;  // Specular highlight color
        uniform float shininess;     // Shininess factor

        void main() {
            // Normalize vectors
            vec3 normal = normalize(vNormal);
            vec3 lightDir = normalize(lightPosition - vPosition);
            vec3 viewDir = normalize(-vPosition);
            vec3 reflectDir = reflect(-lightDir, normal);

            // Ambient
            vec3 ambient = ambientColor;

            // Diffuse
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = diff * diffuseColor;

            // Specular
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
            vec3 specular = spec * specularColor;

            // Combine
            vec3 finalColor = ambient + diffuse + specular;
            gl_FragColor = vec4(finalColor, 1.0);
        }
    `,
    uniforms: {
        lightPosition: { value: new THREE.Vector3(10, 10, 10) },
        ambientColor: { value: new THREE.Color(0.1, 0.1, 0.1) },
        diffuseColor: { value: new THREE.Color(0.5, 0.5, 1.0) },
        specularColor: { value: new THREE.Color(1.0, 1.0, 1.0) },
        shininess: { value: 50.0 },
    },
});
