import * as THREE from 'three';
import { GUI } from 'three/addons/libs/lil-gui.module.min.js';

import { createNoise2D } from '../libs/simplex-noise';
import RAPIER from '@dimforge/rapier3d';


export function createTerrain(scene: THREE.Scene, world: RAPIER.World) {

    const segments = 100

    const hscale = 100 / 2

    const noiseScale = .05;
    const bounds = 5
    const fnNoise2D = createNoise2D(Math.random)
    // const fnNoise2D = (x, z) => Math.sin(x / 10) * Math.cos(z / 10);


    const vertices = []
    const heights = new Float32Array((segments + 1) * (segments + 1));

    for (let z = 0; z < segments; z++) {
        for (let x = 0; x < segments; x++) {
            // Get the four corner points of the current grid cell
            const x0 = x - hscale;
            const x1 = x + 1 - hscale;
            const z0 = z - hscale;
            const z1 = z + 1 - hscale;

            // Heights from noise
            const y00 = Math.max(-bounds, Math.min(bounds, fnNoise2D(noiseScale * x0, noiseScale * z0) * bounds));
            const y10 = Math.max(-bounds, Math.min(bounds, fnNoise2D(noiseScale * x1, noiseScale * z0) * bounds));
            const y01 = Math.max(-bounds, Math.min(bounds, fnNoise2D(noiseScale * x0, noiseScale * z1) * bounds));
            const y11 = Math.max(-bounds, Math.min(bounds, fnNoise2D(noiseScale * x1, noiseScale * z1) * bounds));

            // Two triangles per grid cell
            vertices.push(
                // Triangle 1
                x0, y00, z0, // Bottom-left
                x1, y10, z0, // Bottom-right
                x1, y11, z1, // Top-right

                // Triangle 2
                x1, y11, z1, // Top-right
                x0, y01, z1, // Top-left
                x0, y00, z0  // Bottom-left
            );

            heights[x * segments + z] = y00
        }
    }

    const vertexData = new Float32Array(vertices);

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(vertexData, 3));
    const material = new THREE.MeshStandardMaterial({
        color: 0x008Bff, // Grass green
        side: THREE.DoubleSide,
        wireframe: false, // Useful for debugging
    });
    const ground = new THREE.Mesh(geometry, material);

    ground.receiveShadow = true;
    ground.position.set(0, 0, 0)

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
    let groundColliderDesc = RAPIER.ColliderDesc.heightfield(
        segments,
        segments,
        heights,
        new RAPIER.Vector3(hscale * 2, 1, hscale * 2)
    );
    world.createCollider(groundColliderDesc, groundBody);

    ground.userData = { name: "Ground", rigidBody: groundBody };

    scene.add(ground)
}