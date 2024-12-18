import { createNoise3D } from '../libs/simplex-noise';
import * as THREE from 'three';
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { UV_Shader } from '../shaders/uv';

let seed = 17
const seededRandom = () => {
    let x = Math.sin(seed++) * 10000;
    return x - Math.floor(x);
};
const noise3D = createNoise3D(seededRandom)

function computeCurl(x: number, y: number, z: number) {

    var eps = 1e-4;

    //Find rate of change in X
    var n1 = noise3D(x + eps, y, z);
    var n2 = noise3D(x - eps, y, z);
    //Average to find approximate derivative
    var a = (n1 - n2) / (2 * eps);

    //Find rate of change in Y
    n1 = noise3D(x, y + eps, z);
    n2 = noise3D(x, y - eps, z);
    //Average to find approximate derivative
    var b = (n1 - n2) / (2 * eps);

    //Find rate of change in Z
    n1 = noise3D(x, y, z + eps);
    n2 = noise3D(x, y, z - eps);
    //Average to find approximate derivative
    var c = (n1 - n2) / (2 * eps);

    var noiseGrad0 = new THREE.Vector3(a, b, c);

    // Offset position for second noise read
    x += 10000.5;
    y += 10000.5;
    z += 10000.5;

    //Find rate of change in X
    n1 = noise3D(x + eps, y, z);
    n2 = noise3D(x - eps, y, z);
    //Average to find approximate derivative
    a = (n1 - n2) / (2 * eps);

    //Find rate of change in Y
    n1 = noise3D(x, y + eps, z);
    n2 = noise3D(x, y - eps, z);
    //Average to find approximate derivative
    b = (n1 - n2) / (2 * eps);

    //Find rate of change in Z
    n1 = noise3D(x, y, z + eps);
    n2 = noise3D(x, y, z - eps);
    //Average to find approximate derivative
    c = (n1 - n2) / (2 * eps);

    var noiseGrad1 = new THREE.Vector3(a, b, c);

    noiseGrad0 = noiseGrad0.normalize();
    noiseGrad1 = noiseGrad1.normalize();
    var curl = noiseGrad0.cross(noiseGrad1);

    return curl.normalize();
}

function createScene() {
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(window.innerWidth, window.innerHeight);
    document.body.appendChild(renderer.domElement);

    // Controls
    const controls = new OrbitControls(camera, renderer.domElement);

    // Plane (for reference)
    const planeGeometry = new THREE.PlaneGeometry(10, 10, 10, 10);
    const planeMaterial = new THREE.MeshBasicMaterial({ color: 0xffffff, wireframe: false });
    const plane = new THREE.Mesh(planeGeometry, planeMaterial);
    scene.add(plane);

    // Set up lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
    scene.add(ambientLight);

    const pointLight = new THREE.PointLight(0xffffff, 1);
    pointLight.position.set(10, 10, 10);
    scene.add(pointLight);

    // Position the camera
    camera.position.z = 10;

    for (let i = 0; i < 100; i++) {
        // for (let j = 0; j < 100; j++) {
        let offset = .05
        createCurve(new THREE.Vector3(i * offset, 0, 0), 600, 0.005, scene);
        // }
    }

    function animate() {
        requestAnimationFrame(animate);
        controls.update();
        renderer.render(scene, camera);
    }

    animate();

    window.addEventListener('resize', () => {
        renderer.setSize(window.innerWidth, window.innerHeight);
        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();
    });
}

createScene();

function createCurve(v: THREE.Vector3, pointCount: number, scale: number, scene: THREE.Scene) {
    const curvePoints: THREE.Vector3[] = [];
    for (let i = 0; i < pointCount; i++) {
        const curl = computeCurl(v.x, v.y, v.z);
        v = v.add(curl.multiplyScalar(scale))
        curvePoints.push(v.clone());
    }

    const curve = new THREE.CatmullRomCurve3(curvePoints);
    const tubeGeometry = new THREE.TubeGeometry(curve, 100, 0.03, 8, false);
    const tubeMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000 });
    const tube = new THREE.Mesh(tubeGeometry, UV_Shader);
    scene.add(tube);
}
