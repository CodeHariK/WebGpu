import * as THREE from 'three';
import { PI2, UNITX, UNITY, UNITZ } from './constants';
import { AddLabel } from './function';
import { Game } from './main';

const mousePosition = new THREE.Vector2();
const rayCaster = new THREE.Raycaster();

export function RegisterMouseMove(game: Game, scene3D: THREE.Scene, ACTIVE_CAMERA: THREE.PerspectiveCamera | THREE.OrthographicCamera) {

    const highlight_planeMesh = new THREE.Mesh(
        new THREE.SphereGeometry(.2, 10),
        new THREE.MeshMatcapMaterial({
            color: 0xffffff,
            visible: false,
        })
    );
    game.scene3D.add(highlight_planeMesh);
    highlight_planeMesh.name = "highlight";
    highlight_planeMesh.rotation.x = -PI2;
    highlight_planeMesh.receiveShadow = true;
    highlight_planeMesh.position.set(0.5, 0, 0.5);

    window.addEventListener("mousemove", (e) => {
        mousePosition.x = (e.clientX / window.innerWidth) * 2 - 1;
        mousePosition.y = -(e.clientY / window.innerHeight) * 2 + 1;
        rayCaster.setFromCamera(mousePosition, ACTIVE_CAMERA);
        game.intersects = rayCaster.intersectObjects(scene3D.children);
        let found = false;
        for (let i = 0; i < game.intersects.length; i++) {
            const dintersect = game.intersects[i];
            if (dintersect.object.name.startsWith("box")) {
                found = true;

                let rotX = dintersect.normal?.dot(UNITX) ?? 0
                let rotY = dintersect.normal?.dot(UNITY) ?? 0
                let rotZ = dintersect.normal?.dot(UNITZ) ?? 0

                highlight_planeMesh.rotation.set(PI2 + rotZ * PI2, rotX * PI2, 0)

                highlight_planeMesh.position.set(dintersect.object.position.x + rotX / 2, dintersect.object.position.y + Math.abs(rotY) * .5, dintersect.object.position.z + + rotZ / 2);

                highlight_planeMesh.material.color.setHex(
                    0x00ccff
                    // collection[[x, z].toString()] ? 0xff0000 : 0x00ff00
                );
                break;
            }
            if (dintersect.object.name === "ground") {
                found = true;
                let { x, z } = new THREE.Vector3()
                    .copy(dintersect.point)
                    .floor()
                    .addScalar(0.5);
                // x += GRID_ALIGNMENT_CONSTANT_X
                // z += GRID_ALIGNMENT_CONSTANT_Z
                highlight_planeMesh.rotation.set(PI2, 0, 0)
                highlight_planeMesh.position.set(x, 0, z);
                highlight_planeMesh.material.color.setHex(
                    0x00ff00
                    // collection[[x, z].toString()] ? 0xff0000 : 0x00ff00
                );
                break;
            }
        }
        highlight_planeMesh.material.visible = found;
    });


    const GenerateCube = (x: number, y: number, z: number): THREE.Mesh | undefined => {
        let fx = Math.floor(x)
        let fy = Math.floor(y)
        let fz = Math.floor(z)

        if (fx >= game.GRID_DIMENTION_X || fy >= game.GRID_DIMENTION_Y || fz >= game.GRID_DIMENTION_Z ||
            fx < 0 || fy < 0 || fz < 0
        ) {
            return undefined
        }

        const boxGeometry = new THREE.BoxGeometry(1, 1, 1);
        const boxMaterial = new THREE.MeshMatcapMaterial({
            color: 0xBF40BF,
            // wireframe: true,
        });
        const boxMesh = new THREE.Mesh(boxGeometry, boxMaterial);
        boxMesh.name = `box_${fx}_${fy}_${fz}`
        AddLabel(boxMesh.name, boxMesh)
        scene3D.add(boxMesh);
        boxMesh.position.set(x, y, z);
        // boxMesh.rotation.set(Math.PI / 4, Math.PI / 4, Math.PI / 4);
        boxMesh.castShadow = true;
        return boxMesh;
    };
    //Create
    window.addEventListener("dblclick", () => {
        for (let i = 0; i < game.intersects.length; i++) {
            const dintersect = game.intersects[i];

            if (dintersect.object.name.startsWith("box")) {


                // highlight_planeMesh.position.set(dintersect.object.position.x, dintersect.object.position.y + .5, dintersect.object.position.z);
                // highlight_planeMesh.material.color.setHex(
                //   0x00ccff
                //   // collection[[x, z].toString()] ? 0xff0000 : 0x00ff00
                // );

                let rotX = dintersect.normal?.dot(UNITX) ?? 0
                let rotY = dintersect.normal?.dot(UNITY) ?? 0
                let rotZ = dintersect.normal?.dot(UNITZ) ?? 0

                const x = dintersect.object.position.x + rotX, y = dintersect.object.position.y + rotY, z = dintersect.object.position.z + rotZ
                let key = [Math.floor(x), Math.floor(y), Math.floor(z)].toString()
                if (!game.collection[key]) {

                    const genMesh = GenerateCube(x, y, z)
                    if (genMesh) {
                        game.collection[key] = genMesh;
                    }

                    highlight_planeMesh.material.color.setHex(0xff0000);
                }
                break;
            }

            if (dintersect.object.name === "ground") {
                let { x, z } = new THREE.Vector3()
                    .copy(dintersect.point)
                    .floor()
                    .addScalar(0.5);
                // x += GRID_ALIGNMENT_CONSTANT_X
                // z += GRID_ALIGNMENT_CONSTANT_Z
                let key = [Math.floor(x), 0, Math.floor(z)].toString()
                if (!game.collection[key]) {

                    const genMesh = GenerateCube(x, 0.5, z)
                    if (genMesh) {
                        game.collection[key] = genMesh;
                    }

                    highlight_planeMesh.material.color.setHex(0xff0000);
                }
            }
        }
    });
    //Delete
    window.addEventListener("contextmenu", (e) => {
        e.preventDefault();
        for (let i = 0; i < game.intersects.length; i++) {
            const dintersect = game.intersects[i];

            if (dintersect.object.name.startsWith("box")) {

                const x = dintersect.object.position.x, y = dintersect.object.position.y, z = dintersect.object.position.z
                let key = [Math.floor(x), Math.floor(y), Math.floor(z)].toString()

                if (game.collection[key]) {
                    game.collection[key].children.forEach((c) => {
                        c.removeFromParent()
                    })
                    scene3D.remove(game.collection[key]);
                    highlight_planeMesh.material.color.setHex(0x00ff00);
                    delete game.collection[key];
                    break;
                }
                break;
            }

            if (dintersect.object.name === "ground") {
                let { x, z } = new THREE.Vector3()
                    .copy(dintersect.point)
                    .floor()
                    .addScalar(0.5);
                // x += GRID_ALIGNMENT_CONSTANT_X
                // z += GRID_ALIGNMENT_CONSTANT_Z
                if (game.collection[[x, z].toString()]) {
                    scene3D.remove(game.collection[[x, z].toString()]);
                    highlight_planeMesh.material.color.setHex(0x00ff00);
                    delete game.collection[[x, z].toString()];
                    break;
                }
            }
        }
    });
}