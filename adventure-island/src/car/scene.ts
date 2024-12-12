import { Keyboard } from './input';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, Line, LineBasicMaterial, BufferGeometry, BufferAttribute, MeshStandardMaterial, Vector2, MathUtils } from 'three';
import { createTerrainHeight, createTerrain } from './terrain';
import { createGround, spawnRandomObject } from './environment';
import { CARMODELS, loadCar } from './model';
import { CAR_FERRAI as CAR_FERRARI, CAR_MONSTER_TRUCK, CAR_TOY_CAR, CAR_TURN } from './prefab';
import { createCheckbox } from './ui';
import { RaceTrackBlendCanvas, raceTrackMap, RaceTrackMinimapCanvas, Track } from './canvas_track';
import { ColliderDesc, RigidBodyDesc } from '@dimforge/rapier3d';
import { UNIT_Z } from './physics';

let game = new Game()

new Keyboard()

// createGround(game, 100, 100, new Vector3(0, 0, 0))

const noiseScale = 0.1
const lowresSegments = 10
const highresSegments = 40
const s = 100
const scale = new Vector3(s, 10, s)

createTerrain(game,
    raceTrackMap, 400,
    raceTrackMap, 400,
    scale,
    new Vector3(0, scale.y / 2, 0)
)

for (let i = 0; i < 40; i++) {
    spawnRandomObject(game, new Vector3(s, 30, s));
}

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

    mapCanvasUpdate()
})

const mapCanvasUpdate = () => {

    const Car_Z_Local_Forward = UNIT_Z().applyQuaternion(car.rotation);
    let car_rot = Math.atan2(Car_Z_Local_Forward.z, Car_Z_Local_Forward.x);

    RaceTrackBlendCanvas.drawCircle(new Vector2(car.position.x * 400 / s + 200, car.position.z * 400 / s + 200), 1, true, 0, 'rgb(230,30,30)', null)

    let ctx = RaceTrackMinimapCanvas.context;

    // Clear the minimap canvas
    RaceTrackMinimapCanvas.clearRect();

    // Calculate the car's position in the source canvas
    const carX = car.position.x * 400 / s + 200; // Car's X position on the source canvas
    const carZ = car.position.z * 400 / s + 200; // Car's Z position on the source canvas

    // Dimensions of the cropped area
    const cropSize = 200;
    const canvasWidth = RaceTrackBlendCanvas.canvasElement.width;
    const canvasHeight = RaceTrackBlendCanvas.canvasElement.height;

    // Determine the cropping area, ensuring it stays within bounds
    let sourceX = carX - cropSize / 2;
    let sourceY = carZ - cropSize / 2;

    // Adjust if cropping area goes out of bounds
    if (sourceX < 0) sourceX = 0;
    if (sourceY < 0) sourceY = 0;
    if (sourceX + cropSize > canvasWidth) sourceX = canvasWidth - cropSize;
    if (sourceY + cropSize > canvasHeight) sourceY = canvasHeight - cropSize;

    // Save the context state
    ctx.save();

    // Translate to the car's position in the minimap (center of the canvas)
    const centerX = RaceTrackMinimapCanvas.width() / 2;
    const centerY = RaceTrackMinimapCanvas.height() / 2;
    ctx.translate(centerX, centerY);

    // Rotate the canvas around the car's position
    // ctx.rotate(-car_rot);

    // Translate back to align the cropped area for drawing
    ctx.translate(-centerX, -centerY);

    // Draw the cropped area from the source canvas
    ctx.drawImage(
        RaceTrackBlendCanvas.canvasElement, // Source canvas
        sourceX, // Adjusted source X
        sourceY, // Adjusted source Y
        cropSize, // Source width
        cropSize, // Source height
        0, // Destination X on minimap
        0, // Destination Y on minimap
        RaceTrackMinimapCanvas.width(), // Destination width (scales the cropped area)
        RaceTrackMinimapCanvas.height() // Destination height
    );

    // Restore the context state
    ctx.restore();

    // Calculate the car's position within the minimap
    const carMinimapX = carX - sourceX; // Car's X position relative to the cropped area
    const carMinimapY = carZ - sourceY; // Car's Y position relative to the cropped area

    ctx.save();
    ctx.translate(carMinimapX * (RaceTrackMinimapCanvas.width() / cropSize), carMinimapY * (RaceTrackMinimapCanvas.height() / cropSize));
    RaceTrackMinimapCanvas.drawTriangle(new Vector2(0, 0), 5, 8, car_rot + Math.PI / 2, 0, 'rgb(0,230,30)')
    ctx.restore();
}