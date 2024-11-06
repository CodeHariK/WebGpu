import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { AddLabel } from './function';

const waterMesh = "Water_****";
const grassMesh = "Grass_****";

export const base = [
    "Water",
    "Grass"
]

export const side = [
    "Wall",
    "Stone",
    "Fence",
    "Door"
]
export const corner = [
    "Wall_c",
    "Stone_c",
    "Fence_c"
]

export const foliage = [
    "Tree",
    "Flower"
]

export class Mesh {
    mesh: THREE.Mesh;
    right: boolean;
    left: boolean;
    top: boolean;
    bottom: boolean;
    name: string;
    attachableLeft: Set<Mesh> = new Set();
    attachableRight: Set<Mesh> = new Set();
    attachableTop: Set<Mesh> = new Set();
    attachableBottom: Set<Mesh> = new Set();
    probabitly: number = 0;

    constructor(mesh: THREE.Mesh, right: boolean, left: boolean, top: boolean, bottom: boolean) {
        this.mesh = mesh;
        this.right = right;
        this.left = left;
        this.top = top;
        this.bottom = bottom;
        this.name = mesh.name.split("_")[0] + "_" + this.getName();
    }

    getName(): string {
        return (this.left ? "l" : "*")
            + (this.bottom ? "b" : "*")
            + (this.right ? "r" : "*")
            + (this.top ? "t" : "*")
    }
}

export const THREEMESHES: { [key: string]: THREE.Mesh } = {};
export const Meshes: { [key: string]: Mesh } = {};

export function LoadMeshes(scene: THREE.Scene, AfterMeshLoaded: () => void) {

    const loader = new GLTFLoader();
    const spacing = 2.5;

    loader.load(
        'public/TileSet.glb',
        (gltf) => {

            gltf.scene.traverse((child) => {
                if (child.isObject3D && child.name != 'Scene') {

                    // Clone each mesh and set a unique position for each one
                    const mesh = child.clone() as THREE.Mesh;

                    THREEMESHES[mesh.name] = mesh
                    console.log(child)
                }
            });

            const showcaseGroup = new THREE.Group();
            showcaseGroup.position.setX(25)

            for (const meshName in THREEMESHES) {

                const mesh = THREEMESHES[meshName];

                if (side.includes(meshName)) {

                    for (let i = 0; i < 4; i++) {
                        const meshClone = mesh.clone(); // Clone the mesh for each new instance
                        meshClone.rotateY(i * Math.PI / 2); // Rotate each clone as needed

                        const m = new Mesh(meshClone, i == 2, i == 0, i == 3, i == 1)
                        Meshes[m.name] = m
                    }
                } else if (corner.includes(meshName)) {

                    for (let i = 0; i < 4; i++) {
                        const meshClone = mesh.clone(); // Clone the mesh for each new instance
                        meshClone.rotateY(i * Math.PI / 2); // Rotate each clone as needed

                        const m = new Mesh(meshClone, i == 1 || i == 2, i == 0 || i == 3, i == 2 || i == 3, i == 0 || i == 1)
                        Meshes[m.name] = m
                    }
                } else {
                    const m = new Mesh(mesh, false, false, false, false)
                    Meshes[m.name] = m
                }
            }

            let i = 0
            for (const meshName in Meshes) {
                let m = Meshes[meshName]
                let mm = m.mesh.clone()
                mm.position.set(0, 0, i * spacing);
                showcaseGroup.add(mm)
                AddLabel(meshName, mm)
                i++

                let j = 1
                for (const attachable in Meshes) {
                    let a = Meshes[attachable]
                    if ((
                        (a.name == waterMesh || a.name == grassMesh) && m.right == false && m.top == false && m.bottom == false)
                        ||
                        (a.mesh.name.split("_")[0] == m.mesh.name.split("_")[0])
                        && (a.left == false && m.right == false && a.top == m.top && a.bottom == m.bottom)
                    ) {
                        //Right
                        let aa = a.mesh.clone()
                        aa.position.set(j * spacing, 0, (i - 1) * spacing);
                        showcaseGroup.add(aa)
                        AddLabel(a.name, aa)

                        m.attachableRight.add(a)
                        a.attachableLeft.add(m)
                        j++
                    }
                }

                j = 1
                for (const attachable in Meshes) {
                    let a = Meshes[attachable]
                    if (((a.name == waterMesh || a.name == grassMesh) && m.left == false && m.top == false && m.bottom == false)
                        ||
                        a.mesh.name.split("_")[0] == m.mesh.name.split("_")[0]
                        && (a.right == false && m.left == false && a.top == m.top && a.bottom == m.bottom)) {
                        //Left
                        let aa = a.mesh.clone()
                        aa.position.set(12 + j * spacing, 0, (i - 1) * spacing);
                        showcaseGroup.add(aa)
                        AddLabel(a.name, aa)

                        m.attachableLeft.add(a)
                        a.attachableRight.add(m)
                        j++
                    }
                }

                j = 1
                for (const attachable in Meshes) {
                    let a = Meshes[attachable]
                    if (((a.name == waterMesh || a.name == grassMesh) && m.top == false && m.left == false && m.right == false)
                        ||
                        a.mesh.name.split("_")[0] == m.mesh.name.split("_")[0]
                        && (a.bottom == false && m.top == false && a.left == m.left && a.right == m.right)) {
                        //Top
                        let aa = a.mesh.clone()
                        aa.position.set(24 + j * spacing, 0, (i - 1) * spacing);
                        showcaseGroup.add(aa)
                        AddLabel(a.name, aa)

                        m.attachableTop.add(a)
                        a.attachableBottom.add(m)
                        j++
                    }
                }

                j = 1
                for (const attachable in Meshes) {
                    let a = Meshes[attachable]
                    if (((a.name == waterMesh || a.name == grassMesh) && m.bottom == false && m.left == false && m.right == false)
                        ||
                        a.mesh.name.split("_")[0] == m.mesh.name.split("_")[0]
                        && (a.top == false && m.bottom == false && a.left == m.left && a.right == m.right)) {
                        //Bottom
                        let aa = a.mesh.clone()
                        aa.position.set(36 + j * spacing, 0, (i - 1) * spacing);
                        showcaseGroup.add(aa)
                        AddLabel(a.name, aa)

                        m.attachableBottom.add(a)
                        a.attachableTop.add(m)
                        j++
                    }
                }
            }

            console.log(Meshes)

            let totalRef = 0
            for (const ms in Meshes) {
                let mm = Meshes[ms]
                console.log(mm.name, mm.attachableLeft.size, mm.attachableRight.size, mm.attachableTop.size, mm.attachableBottom.size)
                totalRef += mm.attachableLeft.size + mm.attachableRight.size + mm.attachableTop.size + mm.attachableBottom.size
            }
            let totalProbability = 0
            for (const ms in Meshes) {
                let mm = Meshes[ms]
                mm.probabitly = (mm.attachableLeft.size + mm.attachableRight.size + mm.attachableTop.size + mm.attachableBottom.size) / totalRef
                console.log(mm.name, mm.probabitly)
                totalProbability += mm.probabitly
            }
            console.log(totalProbability)


            scene.add(showcaseGroup)

            AfterMeshLoaded()

        },
        undefined,
        (error) => {
            console.error('An error occurred while loading the model:', error);
        },
    );

}