import { Physics } from './physics';
import { CarGui } from './gui';
import { Keyboard } from './keyboard';
import { AdvScene } from './scene';
import { Car } from './car';
import { Vector3 } from 'three';

let advscene = new AdvScene()

const k = new Keyboard()

const car = new Car(advscene.scene, new Vector3(0, 4, 0), 1, .4, 2);

const g = new CarGui()

let previousTime = performance.now();

// Animation Loop
function animate(): void {

    const currentTime = performance.now();
    const deltaTime = (currentTime - previousTime) / 1000; // Convert milliseconds to seconds
    previousTime = currentTime;


    // Update car position and rotation based on physics
    car.update(deltaTime);

    // Synchronize Three.js objects with Rapier bodies
    advscene.scene.children.forEach((child) => {

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


    advscene.update(car.mesh)
}

advscene.renderer.setAnimationLoop(animate);
