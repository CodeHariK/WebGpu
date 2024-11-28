import { CarGui } from './gui';
import { Keyboard } from './keyboard';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, Line, LineBasicMaterial, BufferGeometry } from 'three';
import { createTerrain } from './terrain';
import { createGround, spawnRandomObject } from './environment';
import { CARMODELS, loadCar } from './model';
import { CAR_FERRAI as CAR_FERRARI, CAR_MONSTER_TRUCK } from './prefab';

let game = new Game()

new Keyboard()

createGround(game, 500, 500)

// createTerrain(game, 100, .02, new Vector3(100, 4, 100), new Vector3(0, 2, 0))

// Spawn 4 objects
for (let i = 0; i < 40; i++) {
    spawnRandomObject(game, 200, 200);
}


//Create a closed wavey loop
const curve = new CatmullRomCurve3([
    new Vector3(-10, 0, 10),
    new Vector3(-5, 5, 5),
    new Vector3(0, 0, 0),
    new Vector3(5, -5, 5),
    new Vector3(10, 0, 10)
]);

const points = curve.getPoints(50);
const geometry = new BufferGeometry().setFromPoints(points);

const material = new LineBasicMaterial({ color: 0xff0000 });

// Create the final object to add to the scene
const curveObject = new Line(geometry, material);

game.SCENE.add(curveObject)

let i = 0
for (const car of CARMODELS) {
    loadCar(game, car, new Vector3(i, 0, 0))
    i += 2
}

const car = CAR_FERRARI(game);

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
