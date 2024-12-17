import * as THREE from 'three';

let scene, camera, renderer, plane, uniforms;

init();
animate();

function init() {
    // Scene setup
    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
    camera.position.z = 5;

    renderer = new THREE.WebGLRenderer();
    renderer.setSize(window.innerWidth, window.innerHeight);
    document.body.appendChild(renderer.domElement);

    // Plane geometry
    const geometry = new THREE.SphereGeometry(1, 20, 20);

    // Shader uniforms
    uniforms = {
        u_time: { value: 0.0 },
        u_waveCenter: { value: new THREE.Vector2(0, 0) },
        u_waveHeight: { value: 0.1 },
        u_damping: { value: 0.5 },
        u_damping_time: { value: 20.0 },
    };

    // Shader material
    const material = new THREE.ShaderMaterial({
        vertexShader: `
            uniform float u_time;
            uniform vec2 u_waveCenter;
            uniform float u_waveHeight;
            uniform float u_damping;
            uniform float u_damping_time;

            varying vec3 vPosition;

            void main() {
                vPosition = position;

                // Compute distance from the wave center
                float dist = distance(u_waveCenter, vPosition.xy);

                // Wave effect with damping
                float wave = u_waveHeight * sin(1.0 * dist - u_time) * exp(-u_damping * dist) * clamp(1.0 - u_time/u_damping_time, 0.0, 1.0);

                // Apply wave displacement to the z-coordinate
                vec3 displacedPosition = position + normal * wave;

                gl_Position = projectionMatrix * modelViewMatrix * vec4(displacedPosition, 1.0);
            }
        `,
        fragmentShader: `
            varying vec3 vPosition;

            void main() {
                gl_FragColor = vec4(0.5 + 0.5 * vPosition.z, 0.7, 1.0, 1.0);
            }
        `,
        uniforms,
        wireframe: true, // Optional for visualizing grid
    });

    plane = new THREE.Mesh(geometry, material);
    scene.add(plane);

    // Mouse click event
    document.addEventListener('mousedown', onMouseDown);
}

function onMouseDown(event: MouseEvent) {
    const mouse = new THREE.Vector2(
        (event.clientX / window.innerWidth) * 2 - 1,
        -(event.clientY / window.innerHeight) * 2 + 1
    );

    const raycaster = new THREE.Raycaster();
    raycaster.setFromCamera(mouse, camera);

    const intersects = raycaster.intersectObject(plane);
    if (intersects.length > 0) {
        const point = intersects[0].point;

        uniforms.u_time.value = 0.0;
        uniforms.u_waveCenter.value.set(point.x, point.y);
    }
}

function animate() {
    requestAnimationFrame(animate);

    // Update time
    uniforms.u_time.value += 0.15;

    renderer.render(scene, camera);
}
