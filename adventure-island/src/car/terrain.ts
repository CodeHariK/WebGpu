import * as THREE from 'three';

import { createNoise2D } from '../libs/simplex-noise';

import RAPIER from '@dimforge/rapier3d';

import { Game } from './game';
import { UV_Shader } from '../shaders/uv';
import { Height_Shader } from '../shaders/height';
import { TextureSample_Shader } from '../shaders/texturesample';
import { HeightTextureBlend_Shader } from '../shaders/heightTextureBlend';

function genHeightfieldGeometry(heights: Float32Array<ArrayBuffer>, segments: number, scale: RAPIER.Vector3) {

    let nrows = segments
    let ncols = segments

    let vertices = [];
    let indices = [];
    const uvs = [];
    let eltWX = 1.0 / segments;
    let eltWY = 1.0 / segments;

    let i: number;
    let j: number;
    for (j = 0; j <= ncols; ++j) {
        for (i = 0; i <= nrows; ++i) {
            let x = (j * eltWX) * scale.x;
            let y = heights[j * (nrows + 1) + i] * scale.y;
            let z = (i * eltWY) * scale.z;

            vertices.push(x, y, z);

            const u = (j * scale.x / ncols); // Horizontal coordinate (0 to 1)
            const v = (i * scale.z / nrows); // Vertical coordinate (0 to 1)
            uvs.push(u, v);
        }
    }

    for (j = 0; j < ncols; ++j) {
        for (i = 0; i < nrows; ++i) {
            let i1 = (i + 0) * (ncols + 1) + (j + 0);
            let i2 = (i + 0) * (ncols + 1) + (j + 1);
            let i3 = (i + 1) * (ncols + 1) + (j + 0);
            let i4 = (i + 1) * (ncols + 1) + (j + 1);

            indices.push(i1, i2, i3);
            indices.push(i3, i2, i4);
        }
    }

    return {
        vertices: new Float32Array(vertices),
        indices: new Uint32Array(indices),
        uvs: new Float32Array(uvs),
    };
}

export type biomeColorType = { r: number, g: number, b: number, height: number, type: 'WATER' | 'SAND' | 'GRASS' | 'SNOW' }
export function strategySearch(water: number, sand: number, grass: number): (c: number) => biomeColorType {
    return (c: number): biomeColorType => {
        if (c < water) return { r: 0, g: 0, b: 255, height: -1, type: 'WATER' } // water
        if (c < sand) return { r: 246, g: 215, b: 176, height: 1, type: 'SAND' } // sand
        if (c < grass) return { r: 0, g: 255, b: 0, height: -1, type: 'GRASS' } // grass
        return { r: 230, g: 230, b: 230, height: 1, type: 'SNOW' } // snow
    }
}

export function createTerrainHeight(
    segments: number,
    size: number, flatness: number,
    position: THREE.Vector2,
    seedNoises: { seed: number, noiseScale: number }[],
    fixedStrategy: 'MIN' | 'MAX' | 'ADDITIVE',
    strategy?: Float32Array<ArrayBufferLike>,
    strategyFunction?: (c: number) => biomeColorType
): Float32Array {

    const heights = new Float32Array(segments * segments);
    if (strategy && strategyFunction) {
        for (let i = 0; i < segments; i++) {
            for (let j = 0; j < segments; j++) {
                heights[i * segments + j] = strategyFunction(strategy[i * segments + j]).height
            }
        }
    } else {
        heights.fill(fixedStrategy == 'MIN' ? 1 : (fixedStrategy == 'MAX' ? -1 : 0))
    }

    let maxHeight = 0

    for (let n = 0; n < seedNoises.length; n++) {

        // const fnNoise2D = createNoise2D(Math.random);
        const seededRandom = () => {
            let x = Math.sin(seedNoises[n].seed++) * 10000;
            return x - Math.floor(x);
        };
        const fnNoise2D = createNoise2D(seededRandom);

        const s = size / (segments - 1)

        for (let i = 0; i < segments; i++) {
            for (let j = 0; j < segments; j++) {
                // Calculate the world position of each grid point
                const worldX = seedNoises[n].noiseScale * (position.x * size + i * s);
                const worldY = seedNoises[n].noiseScale * (position.y * size + j * s);

                if (strategy && strategyFunction) {
                    let s = strategyFunction(strategy[i * segments + j]).type

                    if (s == 'SAND') {
                        heights[i * segments + j] = Math.min(
                            heights[i * segments + j],
                            fnNoise2D(worldX, worldY),
                        );
                    }
                    else if (s == 'GRASS') {
                        heights[i * segments + j] = Math.max(
                            heights[i * segments + j],
                            fnNoise2D(worldX, worldY),
                        );
                    }
                    else if (s == 'SNOW') {
                        heights[i * segments + j] += fnNoise2D(worldX, worldY)
                    }
                    else if (s == 'WATER') {
                        heights[i * segments + j] += fnNoise2D(worldX, worldY)
                    }
                }
                else {
                    if (fixedStrategy == 'MIN') {
                        heights[i * segments + j] = Math.min(
                            heights[i * segments + j],
                            fnNoise2D(worldX, worldY),
                        );
                    } else if (fixedStrategy == 'MAX') {
                        heights[i * segments + j] = Math.max(
                            heights[i * segments + j],
                            fnNoise2D(worldX, worldY),
                        );
                    } else {
                        heights[i * segments + j] += fnNoise2D(worldX, worldY)
                    }
                }

                if (n == seedNoises.length - 1) {
                    maxHeight = Math.max(heights[i * segments + j], maxHeight)
                }
            }
        }
    }

    maxHeight = maxHeight * flatness

    if (maxHeight < .1) {
        throw Error('Less than .1')
    }

    for (let i = 0; i < segments; i++) {
        for (let j = 0; j < segments; j++) {
            heights[i * segments + j] /= maxHeight
        }
    }
    return heights;
}

export function createTerrain(game: Game,
    lowresHeights: Float32Array<ArrayBuffer>, lowresSegments: number,
    midresHeights: Float32Array<ArrayBuffer>, midresSegments: number,
    highresHeights: Float32Array<ArrayBuffer>, highresSegments: number,
    scale: THREE.Vector3, position: THREE.Vector3
) {

    let groundBodyDesc = RAPIER.RigidBodyDesc.fixed().setTranslation(position.x + scale.x / 2, position.y, position.z + scale.z / 2);
    let groundBody = game.WORLD.createRigidBody(groundBodyDesc);
    let groundColliderDesc = RAPIER.ColliderDesc.heightfield(
        lowresSegments - 1,
        lowresSegments - 1,
        lowresHeights,
        scale
    );
    game.WORLD.createCollider(groundColliderDesc, groundBody);

    let lowresObject = genHeightfieldGeometry(lowresHeights, lowresSegments - 1, scale);
    let midresObject = genHeightfieldGeometry(midresHeights, midresSegments - 1, scale);
    let highresObject = genHeightfieldGeometry(highresHeights, highresSegments - 1, scale);

    let lowresGround = generateTerrainMesh(lowresObject)
    let midresGround = generateTerrainMesh(midresObject)
    let highresGround = generateTerrainMesh(highresObject)

    let lod = new THREE.LOD()
    lod.position.set(position.x, position.y, position.z)
    lod.addLevel(lowresGround, 4 * scale.x);
    lod.addLevel(midresGround, 2 * scale.x);
    lod.addLevel(highresGround, 0);

    game.SCENE.add(lod)
    game.LODS.push(lod)
}

let generateTerrainMesh = (
    object: {
        vertices: Float32Array<ArrayBuffer>;
        indices: Uint32Array<ArrayBuffer>;
        uvs: Float32Array<ArrayBuffer>;
    }
) => {

    const textureLoader = new THREE.TextureLoader();

    const diffuseTexture = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_basecolor.jpg');
    const normalMap = textureLoader.load('src/assets/texture/sand/Stylized_Sand_001_normal.jpg');

    diffuseTexture.wrapS = THREE.RepeatWrapping;
    diffuseTexture.wrapT = THREE.RepeatWrapping;
    normalMap.wrapS = THREE.RepeatWrapping;
    normalMap.wrapT = THREE.RepeatWrapping;

    diffuseTexture.minFilter = THREE.LinearMipMapLinearFilter;
    diffuseTexture.magFilter = THREE.LinearFilter;
    normalMap.minFilter = THREE.LinearMipMapLinearFilter;
    normalMap.magFilter = THREE.LinearFilter;

    let material = new THREE.MeshPhysicalMaterial({
        // color: 0xd7b5a0,
        // side: THREE.DoubleSide,
        // flatShading: true,
        // wireframe: true,
        map: diffuseTexture,
        // normalMap: normalMap,
    });

    let geometry = new THREE.BufferGeometry();
    geometry.setIndex(Array.from(object.indices));
    geometry.setAttribute(
        "position",
        new THREE.BufferAttribute(object.vertices, 3),
    );
    geometry.setAttribute(
        "uv",
        new THREE.BufferAttribute(object.uvs, 2) // 2 components per UV coordinate (u, v)
    );

    let ground = new THREE.Mesh(geometry, material);
    // let ground = new THREE.Mesh(geometry, UV_Shader);
    // let ground = new THREE.Mesh(geometry, TextureSample_Shader);
    // let ground = new THREE.Mesh(geometry, Height_Shader(0, 10));
    // let ground = new THREE.Mesh(geometry, HeightTextureBlend_Shader(0, 10));

    ground.receiveShadow = true;
    geometry.attributes.position.needsUpdate = true;
    geometry.computeVertexNormals();
    return ground
}
