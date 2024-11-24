import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import * as RAPIER from '@dimforge/rapier3d';
import { createTerrain } from './terrain';
import { Physics } from './physics';
import { Car } from './car';
import { CarGui } from './gui';
import { Keyboard } from './keyboard';
import { CarScene } from './scene';

let scene = CarScene.create()

const k = new Keyboard()

const car = new Car(scene, 0.8);

const g = new CarGui()

// Animation Loop
function animate(): void {
    requestAnimationFrame(animate);

    // Handle keyboard input
    car.handleKeyboardInput();

    // Update car position and rotation based on physics
    car.update();

    // Synchronize Three.js objects with Rapier bodies
    scene.children.forEach((child) => {

        if (child.userData.rigidBody) {
            const rigidBody = child.userData.rigidBody;
            const position = rigidBody.translation();
            const rotation = rigidBody.rotation();
            child.position.set(position.x, position.y, position.z);
            child.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
        }
    });

    // Step the physics world
    Physics.World.step();


    CarScene.update(car.mesh)
}

animate();