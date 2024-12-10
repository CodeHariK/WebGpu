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
    scale: THREE.Vector3,
    position: THREE.Vector2,
    seedNoises: { seed: number, noiseScale: number }[],
    strategy: 'MIN' | 'MAX' | 'ADDITIVE'
): Float32Array {

    const heights = new Float32Array(segments * segments);
    heights.fill(strategy == 'MIN' ? scale.y : (strategy == 'MAX' ? -scale.y : 0))

    for (let n = 0; n < seedNoises.length; n++) {

        // const fnNoise2D = createNoise2D(Math.random);
        const seededRandom = () => {
            let x = Math.sin(seedNoises[n].seed++) * 10000;
            return x - Math.floor(x);
        };
        const fnNoise2D = createNoise2D(seededRandom);


        const s = scale.x / (segments - 1)

        for (let i = 0; i < segments; i++) {
            for (let j = 0; j < segments; j++) {
                // Calculate the world position of each grid point
                const worldX = seedNoises[n].noiseScale * (position.x * scale.x + i * s);
                const worldY = seedNoises[n].noiseScale * (position.y * scale.x + j * s);

                if (strategy == 'MIN') {
                    heights[i * segments + j] = Math.min(
                        heights[i * segments + j],
                        fnNoise2D(worldX, worldY),
                    );
                } else if (strategy == 'MAX') {
                    heights[i * segments + j] = Math.max(
                        heights[i * segments + j],
                        fnNoise2D(worldX, worldY),
                    );
                } else {
                    heights[i * segments + j] += fnNoise2D(worldX, worldY)
                }
            }
        }
    }
    return heights;
}

export function createTerrain(game: Game,
    lowresHeights: Float32Array<ArrayBuffer>, lowresSegments: number,
    highresHeights: Float32Array<ArrayBuffer>, highresSegments: number,
    scale: THREE.Vector3, position: THREE.Vector3
) {

    let groundBodyDesc = RAPIER.RigidBodyDesc.fixed().setTranslation(position.x, position.y, position.z);
    let groundBody = game.WORLD.createRigidBody(groundBodyDesc);
    let groundColliderDesc = RAPIER.ColliderDesc.heightfield(
        lowresSegments - 1,
        lowresSegments - 1,
        lowresHeights,
        scale
    );
    game.WORLD.createCollider(groundColliderDesc, groundBody);

    let lowresObject = genHeightfieldGeometry(lowresHeights, lowresSegments - 1, scale);
    let highresObject = genHeightfieldGeometry(highresHeights, highresSegments - 1, scale);

    let lowresGround = generateTerrainMesh(lowresObject)
    let highresGround = generateTerrainMesh(highresObject)

    let lod = new THREE.LOD()
    lod.position.set(position.x, position.y, position.z)
    lod.addLevel(lowresGround, 2 * scale.x);
    lod.addLevel(highresGround, 0);

    game.SCENE.add(lod)
    game.LODS.push(lod)
}

let generateTerrainMesh = (
    object: {
        vertices: Float32Array<ArrayBuffer>;
        indices: Uint32Array<ArrayBuffer>;
    }
) => {

    let material = new THREE.MeshPhongMaterial({
        color: 0x888888,
        side: THREE.DoubleSide,
        flatShading: true,
        // wireframe: true,
    });

    let higresGeometry = new THREE.BufferGeometry();
    higresGeometry.setIndex(Array.from(object.indices));
    higresGeometry.setAttribute(
        "position",
        new THREE.BufferAttribute(object.vertices, 3),
    );
    let ground = new THREE.Mesh(higresGeometry, material);
    ground.receiveShadow = true;
    higresGeometry.attributes.position.needsUpdate = true;
    higresGeometry.computeVertexNormals();
    return ground
}
