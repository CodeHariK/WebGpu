import { Physics } from './physics';
import { CarGui } from './gui';
import { Keyboard } from './keyboard';
import { AdvScene } from './scene';
import { Car } from './car';
import { Vector3 } from 'three';

let advscene = new AdvScene()

const k = new Keyboard()

const car = new Car(advscene.scene, new Vector3(0, 4, 0), 1, .4, 1.8);

const g = new CarGui()

// Animation Loop
function animate(): void {
    requestAnimationFrame(animate);

    // Update car position and rotation based on physics
    car.update();

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

animate();