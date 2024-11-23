import * as THREE from 'three';
import { GUI } from 'three/addons/libs/lil-gui.module.min.js';

import { createNoise2D } from './libs/simplex-noise';
import RAPIER from '@dimforge/rapier3d';


export function createTerrain(scene: THREE.Scene, world: RAPIER.World) {
    const geometry = new THREE.PlaneGeometry(200, 200, 100, 100); // Width, height, segments

    const material = new THREE.MeshStandardMaterial({
        color: 0x008Bff, // Grass green
        side: THREE.DoubleSide,
        wireframe: false, // Useful for debugging
    });
    const ground = new THREE.Mesh(geometry, material);
    ground.rotation.x = -Math.PI / 2; // Make the plane horizontal
    ground.receiveShadow = true;
    ground.position.set(0, -.1, 0)

    const vertices = geometry.attributes.position.array;

    const noiseScale = 0.02;
    const fnNoise2D = createNoise2D(Math.random)
    for (let i = 0; i < vertices.length; i += 3) {
        const x = vertices[i];
        const z = vertices[i + 1];
        const noise = fnNoise2D(noiseScale * x, noiseScale * z); // Adjust scale for finer/larger terrain
        vertices[i + 2] = Math.max(-5, Math.min(5, noise * 5));  // Apply noise to the y-coordinate
    }

    geometry.attributes.position.needsUpdate = true;
    geometry.computeVertexNormals(); // Recalculate normals for lighting


    // const textureLoader = new THREE.TextureLoader();
    // const grassTexture = textureLoader.load('./src/assets/Grass_005_BaseColor.jpg');
    // grassTexture.wrapS = grassTexture.wrapT = THREE.RepeatWrapping;
    // grassTexture.repeat.set(10, 10); // Adjust repeat for scaling

    // material.map = grassTexture;
    material.needsUpdate = true;


    // function updateTerrain() {
    //     for (let i = 0; i < vertices.length; i += 3) {
    //         const x = vertices[i];
    //         const z = vertices[i + 2];
    //         const noise = fnNoise2D(x * settings.noiseScale, z * settings.noiseScale);
    //         vertices[i + 1] = noise * settings.height;
    //     }

    //     geometry.attributes.position.needsUpdate = true;
    //     geometry.computeVertexNormals(); // Update normals after vertex changes
    // }


    // const gui = new GUI();
    // const settings = {
    //     noiseScale: 0.05,
    //     height: 10,
    // };

    // gui.add(settings, 'noiseScale', 0.01, 0.1).onChange(updateTerrain);
    // gui.add(settings, 'height', 1, 20).onChange(updateTerrain);


    let groundBodyDesc = RAPIER.RigidBodyDesc.fixed();
    let groundBody = world.createRigidBody(groundBodyDesc);
    let groundCollider = RAPIER.ColliderDesc.heightfield(
        100,
        100,
        new Float32Array(vertices.filter((_, i) => i % 3 === 2)),
        new RAPIER.Vector3(2, 10, 2)
    );
    world.createCollider(groundCollider, groundBody);

    scene.add(ground)
}