import * as THREE from 'three';
import { Meshes, Mesh } from './mesh';
import { AddLabel } from './function';

class MeshState {
    i: number;           // Position along the x-axis (or a custom index)
    j: number;           // Position along the y-axis (or a custom index)
    current: Mesh | null;       // Reference to the current Mesh instance
    states: Set<Mesh>;      // Array holding possible states of Mesh

    constructor(i: number, j: number, current: Mesh | null, states: Set<Mesh>) {
        this.i = i;
        this.j = j;
        this.current = current;
        this.states = states;
    }
}

export function GenerateMap() {
    let x = 10;
    let y = 10;

    // Assuming MeshMap is a 2D array of MeshState
    const MeshMap: MeshState[][] = Array.from({ length: x }, (_, xi) =>
        Array.from({ length: y }, (_, yj) =>
            new MeshState(xi, yj, null, new Set<Mesh>) // Initialize with a default MeshState
        )
    );


    for (let j = 0; j < 4; j++) {
        for (let i = 0; i < 4; i++) {

            let xi = Math.floor((x / 4) * Math.floor(Math.random() * 4))
            let yj = Math.floor((y / 4) * Math.floor(Math.random() * 4))

            if (Math.random() < .5) {
                MeshMap[yj][xi].current = Meshes["Water_****"]
            } else {
                MeshMap[yj][xi].current = Meshes["Grass_****"]
            }
        }
    }



    let group = new THREE.Group();

    for (let j = 0; j < y; j++) {
        for (let i = 0; i < x; i++) {
            let m = MeshMap[j][i].current
            if (m) {

                //right
                if (i + 1 < x) {
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
                if (j + 1 < y) {
                    let bottomState = MeshMap[j + 1][i]
                    if (!bottomState.current) {

                    }
                }

                //top
                if (j - 1 >= 0) {
                    let topState = MeshMap[j - 1][i]
                    if (!topState.current) {

                    }
                }

            }
        }
    }

    console.log(MeshMap)

    for (let j = 0; j < y; j++) {
        for (let i = 0; i < x; i++) {
            let mm = MeshMap[j][i]
            if (mm.current) {
                let m = mm.current.mesh.clone()
                m.position.set(2 * i, 0, -2 * j)
                AddLabel("" + i + "," + j, m)
                group.add(m)
            } else {
                const geometry = new THREE.BoxGeometry(1.2, 1.2, 1.2); // width, height, depth
                const material = new THREE.MeshBasicMaterial({ color: 0x445522 }); // green color

                // 5. Create the cube mesh by combining geometry and material
                const cube = new THREE.Mesh(geometry, material);
                cube.position.set(2 * i, 0, -2 * j)

                AddLabel("" + i + "," + j + " - " + mm.states.size, cube)

                // 6. Add the cube to the scene
                group.add(cube);
            }
        }
    }
    return group
}

function getRandomKey<K, V>(map: Map<K, V>): K | undefined {
    const keys = Array.from(map.keys()); // Convert keys to an array
    if (keys.length === 0) return undefined; // Return undefined if the map is empty

    const randomIndex = Math.floor(Math.random() * keys.length);
    return keys[randomIndex];
}