import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'

const init = () => {
    const canvas = document.createElement('canvas');
    document.body.appendChild(canvas);

    const renderer = new THREE.WebGLRenderer({ canvas });
    renderer.setSize(window.innerWidth, window.innerHeight);

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
    camera.position.z = 3;

    const controls = new OrbitControls(camera, canvas);
    controls.enableDamping = true;

    const geometry = new THREE.PlaneGeometry(2, 2, 128, 128);

    // Load textures
    const textureLoader = new THREE.TextureLoader();
    const texture1 = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_basecolor.jpg');
    const texture2 = textureLoader.load('src/assets/texture/grass_texture.jpg');
    const displacementMap = textureLoader.load('src/assets/curlnoise.png');

    texture1.wrapS = THREE.RepeatWrapping;
    texture1.wrapT = THREE.RepeatWrapping;
    texture2.wrapS = THREE.RepeatWrapping;
    texture2.wrapT = THREE.RepeatWrapping;

    texture1.minFilter = THREE.LinearMipMapLinearFilter;
    texture1.magFilter = THREE.LinearFilter;
    texture2.minFilter = THREE.LinearMipMapLinearFilter;
    texture2.magFilter = THREE.LinearFilter;

    // Shader Material
    const material = new THREE.ShaderMaterial({
        uniforms: {
            uTexture1: { value: texture1 },
            uTexture2: { value: texture2 },
            uDisplacementMap: { value: displacementMap },
            uTime: { value: 0.0 }, // Controls blend between texture1 and texture2
            uIntensity: { value: 0.5 }, // Controls distortion strength
        },
        vertexShader: `
            varying vec2 vUv;

            void main() {
                vUv = uv;
                gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
            }
        `,
        fragmentShader: `
            varying vec2 vUv;
            uniform sampler2D uTexture1;
            uniform sampler2D uTexture2;
            uniform sampler2D uDisplacementMap;
            uniform float uTime;
            uniform float uIntensity;

            void main() {
                // Sample the displacement map
                vec4 displacement = texture2D(uDisplacementMap, vUv);

                // Use the displacement map to offset UVs
                vec2 displacedUv1 = vUv + displacement.rg * uIntensity * (1.0 - uTime); // Texture1 UV distortion
                vec2 displacedUv2 = vUv - displacement.rg * uIntensity * uTime;         // Texture2 UV distortion (opposite)

                // Sample both textures with displaced UVs
                vec4 tex1 = texture2D(uTexture1, displacedUv1);
                vec4 tex2 = texture2D(uTexture2, displacedUv2);

                // Blend between the two textures based on time
                vec4 finalColor = mix(tex1, tex2, uTime);
                gl_FragColor = finalColor;
            }
        `,
    });

    const mesh = new THREE.Mesh(geometry, material);
    scene.add(mesh);

    // Animation Loop
    const clock = new THREE.Clock();

    const animate = () => {
        requestAnimationFrame(animate);

        // Update time uniform (cycles between 0 and 1)
        const elapsedTime = clock.getElapsedTime();
        material.uniforms.uTime.value = (Math.sin(elapsedTime) + 1.0) / 2.0; // Oscillates between 0 and 1

        controls.update();
        renderer.render(scene, camera);
    };

    animate();
};

init();