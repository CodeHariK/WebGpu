import * as THREE from 'three';

import { createNoise2D } from '../libs/simplex-noise';
import RAPIER from '@dimforge/rapier3d';
import { Game } from './game';

function genHeightfieldGeometry(heights: Float32Array<ArrayBuffer>, segments: number, scale: RAPIER.Vector3) {

    let nrows = segments
    let ncols = segments

    let vertices = [];
    let indices = [];
    let eltWX = 1.0 / segments;
    let eltWY = 1.0 / segments;

    let i: number;
    let j: number;
    for (j = 0; j <= ncols; ++j) {
        for (i = 0; i <= nrows; ++i) {
            let x = (j * eltWX - 0.5) * scale.x;
            let y = heights[j * (nrows + 1) + i] * scale.y;
            let z = (i * eltWY - 0.5) * scale.z;

            vertices.push(x, y, z);
        }
    }

    for (j = 0; j < ncols; ++j) {
        for (i = 0; i < nrows; ++i) {
            let i1 = (i + 0) * (ncols + 1) + (j + 0);
            let i2 = (i + 0) * (ncols + 1) + (j + 1);
            let i3 = (i + 1) * (ncols + 1) + (j + 0);
            let i4 = (i + 1) * (ncols + 1) + (j + 1);

            indices.push(i1, i3, i2);
            indices.push(i3, i4, i2);
        }
    }

    return {
        vertices: new Float32Array(vertices),
        indices: new Uint32Array(indices),
    };
}

export function createTerrainHeight(segments: number, noiseScale: number) {

    const fnNoise2D = createNoise2D(Math.random)

    const heights = new Float32Array(segments * segments);
    for (let i = 0; i < segments; i++) {
        for (let j = 0; j < segments; j++) {
            heights[i * segments + j] = fnNoise2D(noiseScale * i, noiseScale * j)
        }
    }
    return heights
}

export function createTerrain(game: Game, heights: Float32Array<ArrayBuffer>, segments: number, scale: THREE.Vector3, position: THREE.Vector3) {

    let groundBodyDesc = RAPIER.RigidBodyDesc.fixed().setTranslation(position.x, position.y, position.z);
    let groundBody = game.WORLD.createRigidBody(groundBodyDesc);
    let groundColliderDesc = RAPIER.ColliderDesc.heightfield(
        segments - 1,
        segments - 1,
        heights,
        scale
    );
    game.WORLD.createCollider(groundColliderDesc, groundBody);

    let g = genHeightfieldGeometry(heights, segments - 1, scale);

    let geometry = new THREE.BufferGeometry();
    geometry.setIndex(Array.from(g.indices));
    geometry.setAttribute(
        "position",
        new THREE.BufferAttribute(g.vertices, 3),
    );

    let material = new THREE.MeshPhongMaterial({
        color: 0x888888,
        side: THREE.DoubleSide,
        flatShading: true,
    });

    // const textureLoader = new THREE.TextureLoader();
    // const groundTexture = textureLoader.load('./src/assets/ground_grid.png');
    // groundTexture.wrapS = THREE.RepeatWrapping; // Enable texture wrapping on S (horizontal) axis
    // groundTexture.wrapT = THREE.RepeatWrapping; // Enable texture wrapping on T (vertical) axis
    // groundTexture.repeat.set(50, 50); // Adjust the repeat to scale the texture
    // const groundMaterial = new THREE.MeshToonMaterial({ map: groundTexture });



    let ground = new THREE.Mesh(geometry, material);
    ground.receiveShadow = true;
    geometry.attributes.position.needsUpdate = true;
    geometry.computeVertexNormals();

    ground.position.set(position.x, position.y, position.z)

    game.SCENE.add(ground);

    // // const textureLoader = new THREE.TextureLoader();
    // // const grassTexture = textureLoader.load('./src/assets/Grass_005_BaseColor.jpg');
    // // grassTexture.wrapS = grassTexture.wrapT = THREE.RepeatWrapping;
    // // grassTexture.repeat.set(10, 10); // Adjust repeat for scaling

    // // material.map = grassTexture;
    // material.needsUpdate = true;

    // // function updateTerrain() {
    // //     for (let i = 0; i < vertices.length; i += 3) {
    // //         const x = vertices[i];
    // //         const z = vertices[i + 2];
    // //         const noise = fnNoise2D(x * settings.noiseScale, z * settings.noiseScale);
    // //         vertices[i + 1] = noise * settings.height;
    // //     }

    // //     geometry.attributes.position.needsUpdate = true;
    // //     geometry.computeVertexNormals(); // Update normals after vertex changes
    // // }

    // // const gui = new GUI();
    // // const settings = {
    // //     noiseScale: 0.05,
    // //     height: 10,
    // // };

    // // gui.add(settings, 'noiseScale', 0.01, 0.1).onChange(updateTerrain);
    // // gui.add(settings, 'height', 1, 20).onChange(updateTerrain);
}