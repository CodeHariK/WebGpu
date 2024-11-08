import * as THREE from 'three';

import Stats from 'three/addons/libs/stats.module.js';
import { GUI } from 'three/addons/libs/lil-gui.module.min.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { CSS2DRenderer, CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';


import { TextureLoader } from 'three';
import { AddLabel, isBitSet } from './function';
import { side, corner, THREEMESHES, LoadMeshes } from './mesh';
import { GenerateMap, MAPGROUP, RenderMap, SolveMap } from './Map';
import { INPUT, InputKeys } from './input';

INPUT.RegisterKeys()

const perspectiveCamera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
// Position the camera
perspectiveCamera.position.set(0, 10, 10);
perspectiveCamera.lookAt(0, 0, 0);


const aspect = window.innerWidth / window.innerHeight;
const orthographicCamera = new THREE.OrthographicCamera(
  -aspect, aspect, 1, -1, 0.1, 10
);
orthographicCamera.position.z = 5;


const labelRenderer = new CSS2DRenderer();
labelRenderer.setSize(window.innerWidth, window.innerHeight);
labelRenderer.domElement.style.position = 'absolute';
labelRenderer.domElement.style.top = '0px';
labelRenderer.domElement.style.pointerEvents = 'none';
document.body.appendChild(labelRenderer.domElement);


const renderer = new THREE.WebGLRenderer();
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);


const controls = new OrbitControls(perspectiveCamera, renderer.domElement);
controls.enableDamping = true;
controls.enableZoom = true
controls.enableRotate = true
controls.enablePan = true
controls.dampingFactor = 0.05;    // Lower values for smoother orbiting
controls.minDistance = 5;         // Minimum zoom distance
controls.maxDistance = 50;        // Maximum zoom distance
controls.maxPolarAngle = Math.PI / 2; // Limit vertical movement


// Set up the scene, camera, and renderer
const scene3D = new THREE.Scene();
const sceneHUD = new THREE.Scene();

const hudGeometry = new THREE.PlaneGeometry(0.1, 0.1);
const hudMaterial = new THREE.MeshBasicMaterial({ color: 0xff0000 });
const hudElement = new THREE.Mesh(hudGeometry, hudMaterial);
sceneHUD.add(hudElement);

LoadMeshes(scene3D, () => {
  GenerateMap(scene3D)
})


// Add ambient light to softly illuminate the scene
const ambientLight = new THREE.AmbientLight(0xffffff, 0.3); // Adjust intensity as needed
scene3D.add(ambientLight);

// Add a directional light to simulate sunlight
const sunLight = new THREE.DirectionalLight(0xffffff, 2);
sunLight.position.set(10, 20, 10);
sunLight.castShadow = true;
sunLight.shadow.mapSize.width = 1024;
sunLight.shadow.mapSize.height = 1024;
sunLight.shadow.camera.near = 0.5;
sunLight.shadow.camera.far = 50;
scene3D.add(sunLight);

// Enable shadows in the renderer
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;

let KeyPressed = false

// Initialize renderer and set autoClear to false
renderer.autoClear = false;

// Render loop
function animate() {

  if (INPUT.current == InputKeys.ArrowDOWN && !KeyPressed) {
    document.querySelectorAll(".label").forEach((label) => {
      label.remove();  // Remove each label from the DOM
    });

    MAPGROUP.clear()

    SolveMap()
    RenderMap()
    scene3D.add(MAPGROUP)
  }

  requestAnimationFrame(animate);
  controls.update();

  labelRenderer.render(scene3D, perspectiveCamera);

  // Clear the whole canvas
  renderer.clear();

  // Render the 3D scene with the perspective camera
  renderer.render(scene3D, perspectiveCamera);

  // Clear depth to render HUD elements on top
  renderer.clearDepth();

  // Render the HUD scene with the orthographic camera
  renderer.render(sceneHUD, orthographicCamera);

  if (INPUT.current) {
    KeyPressed = true
  } else {
    KeyPressed = false
  }
}
animate();

// Adjust camera and renderer on window resize
window.addEventListener('resize', () => {

  perspectiveCamera.aspect = window.innerWidth / window.innerHeight;
  perspectiveCamera.updateProjectionMatrix();

  // Adjust orthographic camera
  const aspect = window.innerWidth / window.innerHeight;
  orthographicCamera.left = -aspect;
  orthographicCamera.right = aspect;
  orthographicCamera.updateProjectionMatrix();

  renderer.setSize(window.innerWidth, window.innerHeight);
});