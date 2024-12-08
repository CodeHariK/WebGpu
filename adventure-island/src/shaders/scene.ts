import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'

import { Hello_Shader } from './hello';
import { Normal_Shader } from './normal';
import { Triplanar_Shader } from './triplanar';
import { Height_Shader } from './height';


const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
camera.position.z = 5;

const renderer = new THREE.WebGLRenderer();
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);


// Orbit Controls
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true; // Smooth movement
controls.dampingFactor = 0.05; // Adjust damping strength
controls.screenSpacePanning = false; // Prevent panning in screen space
controls.minDistance = 0.5; // Minimum zoom distance
controls.maxDistance = 100; // Maximum zoom distance


// Create a plane geometry
const planeGeometry = new THREE.PlaneGeometry(2, 2);


const textureLoader = new THREE.TextureLoader();
const normalMap = textureLoader.load('src/shaders/NormalMap.png');
normalMap.wrapS = normalMap.wrapT = THREE.RepeatWrapping;






// const shaderMaterial = Hello_Shader
const shaderMaterial = Height_Shader
// const shaderMaterial = Triplanar_Shader
// const shaderMaterial = Normal_Shader
// const shaderMaterial = Diffuse_Shader









const plane = new THREE.Mesh(planeGeometry, shaderMaterial);
scene.add(plane);


const sphereDiffuse = new THREE.Mesh(new THREE.SphereGeometry(1, 32, 32), shaderMaterial);
sphereDiffuse.position.set(0, 0, 0)
scene.add(sphereDiffuse);



// Animation loop
const clock = new THREE.Clock();
function animate() {
    // shaderMaterial.uniforms.u_time.value = clock.getElapsedTime();
    controls.update();
    renderer.render(scene, camera);
    requestAnimationFrame(animate);
}

animate();




window.addEventListener('resize', () => {
    const width = window.innerWidth;
    const height = window.innerHeight;
    renderer.setSize(width, height);
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    // shaderMaterial.uniforms.u_resolution.value.set(width, height);
});

window.addEventListener('mousemove', (event) => {
    // shaderMaterial.uniforms.u_mouse.value.set(event.clientX, window.innerHeight - event.clientY);
});




