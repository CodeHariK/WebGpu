import { CarGui } from './gui';
import { Keyboard } from './keyboard';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3 } from 'three';
import { createTerrain } from './terrain';
import { createGround } from './environment';

let game = new Game()

new Keyboard()

createGround(game)

// createTerrain(game, 100, .04, new Vector3(100, 4, 100))

// // Spawn 4 objects
// for (let i = 0; i < 40; i++) {
//     this.spawnRandomObject();
// }

const car = new Car(
    game,
    new Vector3(0, 4, 0),
    100,
    1,
    500,
    .4,
    2,
    .2);

new CarGui()

game.SCENE.traverse((object) => {
    // Check if the object is a Mesh
    if (object instanceof Mesh) {
        const axesHelper = new AxesHelper(1.2);
        object.add(axesHelper); // Add AxesHelper to the Mesh
    }
});

game.animate((deltaTime: number) => {
    game.cameraUpdate(car.mesh)
    car.update(game, deltaTime);
})