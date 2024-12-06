import * as THREE from 'three';

const textureLoader = new THREE.TextureLoader();
const textureX = textureLoader.load('src/shaders/NormalMap.png');
const textureY = textureLoader.load('src/shaders/NormalMap.png');
const textureZ = textureLoader.load('src/shaders/NormalMap.png');

[textureX, textureY, textureZ].forEach((texture) => {
    texture.wrapS = THREE.RepeatWrapping;
    texture.wrapT = THREE.RepeatWrapping;
});

export const Triplanar_Shader = new THREE.ShaderMaterial({
    uniforms: {
        uTextureX: { value: textureX },
        uTextureY: { value: textureY },
        uTextureZ: { value: textureZ },
        uTextureScale: { value: new THREE.Vector2(0.1, 0.1) },
    },
    vertexShader: `
        varying vec3 vPosition;
        varying vec3 vNormal;
        void main() {
            vPosition = position;
            vNormal = normalize(normalMatrix * normal);
            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
    `,
    fragmentShader: `
        #ifdef GL_ES
        precision mediump float;
        #endif

        uniform sampler2D uTextureX;
        uniform sampler2D uTextureY;
        uniform sampler2D uTextureZ;
        uniform vec2 uTextureScale;

        varying vec3 vPosition;
        varying vec3 vNormal;

        vec4 triplanarMapping(vec3 position, vec3 normal) {
            vec3 blending = abs(normal);
            blending /= (blending.x + blending.y + blending.z);

            vec2 uvX = position.yz * uTextureScale;
            vec2 uvY = position.zx * uTextureScale;
            vec2 uvZ = position.xy * uTextureScale;

            vec4 texX = texture2D(uTextureX, uvX);
            vec4 texY = texture2D(uTextureY, uvY);
            vec4 texZ = texture2D(uTextureZ, uvZ);

            return texX * blending.x + texY * blending.y + texZ * blending.z;
        }

        void main() {
            vec3 normal = normalize(vNormal);
            vec4 color = triplanarMapping(vPosition, normal);
            gl_FragColor = color;
        }
    `,
});
