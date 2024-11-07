import * as THREE from 'three';
import { Meshes, Mesh } from './mesh';
import { AddLabel } from './function';
import { INPUT, InputKeys } from './input';

class MeshState {
    i: number;           // Position along the x-axis (or a custom index)
    j: number;           // Position along the y-axis (or a custom index)
    entropy: number = 99;
    current: Mesh | null;       // Reference to the current Mesh instance
    states: Set<Mesh>;      // Array holding possible states of Mesh

    constructor(i: number, j: number, current: Mesh | null, states: Set<Mesh>) {
        this.i = i;
        this.j = j;
        this.current = current;
        this.states = states;
    }
}

let MAPX = 12;
let MAPY = 12;

export let MAPGROUP = new THREE.Group();

// Assuming MeshMap is a 2D array of MeshState
const MeshMap: MeshState[][] = Array.from({ length: MAPX }, (_, xi) =>
    Array.from({ length: MAPY }, (_, yj) =>
        new MeshState(xi, yj, null, new Set<Mesh>) // Initialize with a default MeshState
    )
);

let NUM_UNKNOWN_STATES = MAPX * MAPY
let MIN_UNKNOWN_X: number | null = null
let MIN_UNKNOWN_Y: number | null = null
let MAX_ENTROPY = 99

export function GenerateMap(scene: THREE.Scene) {

    MeshMap[MAPY / 2][MAPX / 2].current = Meshes["Grass_****"]

    // for (let j = 0; j < 4; j++) {
    //     for (let i = 0; i < 4; i++) {

    //         let xi = Math.floor((MAPX / 4) * Math.floor(Math.random() * 4))
    //         let yj = Math.floor((MAPY / 4) * Math.floor(Math.random() * 4))

    //         if (Math.random() < .5) {
    //             MeshMap[yj][xi].current = Meshes["Water_****"]
    //         } else {
    //             MeshMap[yj][xi].current = Meshes["Grass_****"]
    //         }

    //         NUM_UNKNOWN_STATES--
    //     }
    // }


    for (let j = 0; j < MAPY; j++) {
        for (let i = 0; i < MAPX; i++) {
            SolveMeshState(i, j)
        }
    }

    FindMinEntropy()

    // for (let i = 0; i < 100; i++) {
    // SolveMap()
    // }

    RenderMap()

    scene.add(MAPGROUP)
}

function getRandomKey<K, V>(map: Map<K, V>): K | undefined {
    const keys = Array.from(map.keys()); // Convert keys to an array
    if (keys.length === 0) return undefined; // Return undefined if the map is empty

    const randomIndex = Math.floor(Math.random() * keys.length);
    return keys[randomIndex];
}

function calculateEntropy(meshSet: Set<Mesh>): number {
    // Extract probabilities from the Mesh set and apply the entropy formula
    return -[...meshSet].reduce((sum, mesh) => {
        const p = mesh.probability;
        return p > 0 ? sum + p * Math.log2(p) : sum;
    }, 0);
}

function SolveMeshState(i: number, j: number) {
    let m = MeshMap[j][i].current
    if (m) {

        //right
        if (i + 1 < MAPX) {
            let rightState = MeshMap[j][i + 1]
            if (!rightState.current) {
                if (rightState.states.size == 0) {
                    m.attachableRight.forEach(item => rightState.states.add(item));
                } else {
                    const intersection = new Set([...rightState.states].filter(item => m.attachableRight.has(item)));
                    rightState.states = intersection
                }
            }
        }

        //left
        if (i - 1 >= 0) {
            let leftState = MeshMap[j][i - 1]
            if (!leftState.current) {
                if (leftState.states.size == 0) {
                    m.attachableLeft.forEach(item => leftState.states.add(item));
                } else {
                    const intersection = new Set([...leftState.states].filter(item => m.attachableLeft.has(item)));
                    leftState.states = intersection
                }
            }
        }

        //bottom
        if (j - 1 >= 0) {
            let bottomState = MeshMap[j - 1][i]
            if (!bottomState.current) {
                if (bottomState.states.size == 0) {
                    m.attachableBottom.forEach(item => bottomState.states.add(item));
                } else {
                    const intersection = new Set([...bottomState.states].filter(item => m.attachableBottom.has(item)));
                    bottomState.states = intersection
                }
            }
        }

        //top
        if (j + 1 < MAPY) {

            let topState = MeshMap[j + 1][i]
            if (!topState.current) {
                if (topState.states.size == 0) {
                    m.attachableTop.forEach(item => topState.states.add(item));
                } else {
                    const intersection = new Set([...topState.states].filter(item => m.attachableTop.has(item)));
                    topState.states = intersection
                }
            }
        }
    }
}

function FindMinEntropy() {
    MIN_UNKNOWN_X = null
    MIN_UNKNOWN_Y = null
    MAX_ENTROPY = 99
    for (let j = 0; j < MAPY; j++) {
        for (let i = 0; i < MAPX; i++) {
            let cur = MeshMap[j][i].current
            let s = MeshMap[j][i].states
            if (!cur && s.size != 0) {
                console.log("FindMinEntropy.........")
                MeshMap[j][i].entropy = calculateEntropy(s)
            }
            if (MeshMap[j][i].entropy != 0 && MeshMap[j][i].entropy < MAX_ENTROPY) {
                MIN_UNKNOWN_X = i
                MIN_UNKNOWN_Y = j
                MAX_ENTROPY = MeshMap[j][i].entropy
                console.log("Min Entropy :", MIN_UNKNOWN_Y, MIN_UNKNOWN_X, " -> ", MAX_ENTROPY)
            }
        }
    }

    console.log("FindMinEntropy", { "NumUnknownStates": NUM_UNKNOWN_STATES, "MinUnknownX": MIN_UNKNOWN_X, "MinUnknownY": MIN_UNKNOWN_Y })
}

export function RenderMap() {

    for (let j = 0; j < MAPY; j++) {
        for (let i = 0; i < MAPX; i++) {
            let mm = MeshMap[j][i]

            let s = MeshMap[j][i].states

            if (mm.current) {
                let m = mm.current.mesh.clone()
                m.position.set(2 * i, 0, -2 * j)
                AddLabel("" + j + "," + i, m)
                MAPGROUP.add(m)
            } else {
                const geometry = new THREE.BoxGeometry(1.2, 1.2, 1.2); // width, height, depth
                const material = new THREE.MeshBasicMaterial({ color: (MIN_UNKNOWN_X == i && MIN_UNKNOWN_Y == j) ? 0xff00000 : (s.size != 0 ? 0x885522 : 0x332233) }); // green color

                // 5. Create the cube mesh by combining geometry and material
                const cube = new THREE.Mesh(geometry, material);
                cube.position.set(2 * i, 0, -2 * j)

                AddLabel("" + j + "," + i + " - " + mm.states.size + " - " + mm.entropy.toFixed(1), cube)

                // 6. Add the cube to the scene
                MAPGROUP.add(cube);
            }
        }
    }
}

function FindMaxProbabilityMesh(minUnknown: MeshState): Mesh | null {
    let maxProbabilityMesh: Mesh | null = null
    for (let s of minUnknown.states) {
        if (!maxProbabilityMesh || maxProbabilityMesh.probability < s.probability) {
            maxProbabilityMesh = s
        }
    }

    const itemsArray = Array.from(minUnknown.states);
    const totalProbability = itemsArray.reduce((sum, item) => sum + item.probability, 0);

    // Step 2: Normalize probabilities
    const normalizedItems = itemsArray.map(item => ({
        item,
        normalizedProbability: item.probability / totalProbability
    }));

    // Step 3: Calculate cumulative probabilities
    let cumulativeProbability = 0;
    const cumulativeProbabilities = normalizedItems.map(({ item, normalizedProbability }) => {
        cumulativeProbability += normalizedProbability;
        return { item, cumulativeProbability };
    });

    // Step 4: Generate a random number between 0 and 1
    const random = Math.random();

    // Step 5: Select the item based on cumulative probability
    const selectedItem = cumulativeProbabilities.find(
        ({ cumulativeProbability }) => random <= cumulativeProbability
    )?.item ?? null;

    console.log("Selected item:", selectedItem?.name);

    return selectedItem

    return maxProbabilityMesh
}

export function SolveMap() {
    if (MIN_UNKNOWN_Y != null && MIN_UNKNOWN_X != null) {
        let minUnknown = MeshMap[MIN_UNKNOWN_Y][MIN_UNKNOWN_X]

        console.log("Initial State", minUnknown)

        if (minUnknown.states.size == 0) {
            console.log("Error No valid states")
            // break;
        }

        minUnknown.current = FindMaxProbabilityMesh(minUnknown)
        minUnknown.entropy = 0
        minUnknown.states.clear()
        NUM_UNKNOWN_STATES--
        SolveMeshState(MIN_UNKNOWN_X, MIN_UNKNOWN_Y)

        console.log("END State", minUnknown)

        console.log(MeshMap)
    }
    FindMinEntropy()
}