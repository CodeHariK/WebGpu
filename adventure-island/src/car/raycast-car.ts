import { CarGui } from './gui';
import { Keyboard } from './keyboard';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3 } from 'three';
import { createTerrain } from './terrain';
import { createGround, spawnRandomObject } from './environment';

let game = new Game()

new Keyboard()

createGround(game)

// createTerrain(game, 100, .04, new Vector3(100, 4, 100))

// // Spawn 4 objects
// for (let i = 0; i < 40; i++) {
//     spawnRandomObject(game);
// }

const car = new Car(
    game,
    new Vector3(0, 4, 0),
    10000,
    1,
    1500,
    .4,
    1.5,
    .3,
    3.75,
    1.6,
    30,
);

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