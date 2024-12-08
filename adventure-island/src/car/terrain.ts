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

export function createMaxTerrainHeight(
    segments: number,
    scalexz: number,
    scaley: number,
    noiseScale: number,
    position: THREE.Vector2,
    seeds: number[],
): Float32Array {

    const heights = new Float32Array(segments * segments);
    heights.fill(-scaley)

    for (let n = 0; n < seeds.length; n++) {

        const fnNoise2D = createNoise2D(Math.random);
        // const fnNoise2D = createNoise2D(() => seeds[n]);

        const s = scalexz / (segments - 1)

        for (let i = 0; i < segments; i++) {
            for (let j = 0; j < segments; j++) {
                // Calculate the world position of each grid point
                const worldX = noiseScale * (position.x + i * s);
                const worldY = noiseScale * (position.y + j * s);

                // Get the height from the noise function
                heights[i * segments + j] = Math.max(
                    heights[i * segments + j],
                    fnNoise2D(worldX, worldY),
                );
            }
        }
    }
    return heights;
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

    let ground = new THREE.Mesh(geometry, material);
    ground.receiveShadow = true;
    geometry.attributes.position.needsUpdate = true;
    geometry.computeVertexNormals();

    ground.position.set(position.x, position.y, position.z)

    game.SCENE.add(ground);
}
