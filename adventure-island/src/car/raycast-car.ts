import { Keyboard } from './keyboard';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, Line, LineBasicMaterial, BufferGeometry } from 'three';
import { createTerrain } from './terrain';
import { createGround, spawnRandomObject } from './environment';
import { CARMODELS, loadCar } from './model';
import { CAR_FERRAI as CAR_FERRARI, CAR_MONSTER_TRUCK, CAR_TOY_CAR, CAR_TURN } from './prefab';
import { createCheckbox } from './ui';

let game = new Game()

new Keyboard()

createGround(game, 500, 500)

// createTerrain(game, 100, .02, new Vector3(100, 4, 100), new Vector3(0, 2, 0))

// for (let i = 0; i < 40; i++) {
//     spawnRandomObject(game, 200, 200);
// }

// const curve = new CatmullRomCurve3([
//     new Vector3(-10, 0, 10),
//     new Vector3(-5, 5, 5),
//     new Vector3(0, 0, 0),
//     new Vector3(5, -5, 5),
//     new Vector3(10, 0, 10)
// ]);
// const points = curve.getPoints(50);
// const geometry = new BufferGeometry().setFromPoints(points);
// const material = new LineBasicMaterial({ color: 0xff0000 });
// const curveObject = new Line(geometry, material);
// game.SCENE.add(curveObject)

const car = CAR_TOY_CAR(game);

game.SCENE.traverse((object) => {
    // Check if the object is a Mesh
    if (object instanceof Mesh) {
        const axesHelper = new AxesHelper(1.2);
        object.add(axesHelper); // Add AxesHelper to the Mesh
    }
});

game.animate((deltaTime: number) => {
    car.update(game, deltaTime);
})
