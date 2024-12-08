import { Keyboard } from './keyboard';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, Line, LineBasicMaterial, BufferGeometry, BufferAttribute, MeshStandardMaterial, Vector2 } from 'three';
import { createMaxTerrainHeight, createTerrain } from './terrain';
import { createGround, spawnRandomObject } from './environment';
import { CARMODELS, loadCar } from './model';
import { CAR_FERRAI as CAR_FERRARI, CAR_MONSTER_TRUCK, CAR_TOY_CAR, CAR_TURN } from './prefab';
import { createCheckbox } from './ui';
import { Track } from './canvas_track';
import { ColliderDesc, RigidBodyDesc } from '@dimforge/rapier3d';

let game = new Game()

new Keyboard()

createGround(game, 100, 100, new Vector3(0, 0, 0))

const noiseScale = 0.1
const lowresSegments = 10
const highresSegments = 100
const s = 10
const scale = new Vector3(s, 1, s)

for (let row = 0; row < 2; row++) {
    for (let col = 0; col < 2; col++) {
        let lowresHeights = createMaxTerrainHeight(
            lowresSegments,
            scale,
            new Vector2(row, col),
            [
                { seed: 17, noiseScale },
                { seed: 1700, noiseScale },
            ],
            'MIN'
        )
        let highresHeights = createMaxTerrainHeight(
            highresSegments,
            scale,
            new Vector2(row, col),
            [
                { seed: 17, noiseScale },
                { seed: 1700, noiseScale },
            ],
            'MIN'
        )

        createTerrain(game,
            lowresHeights, lowresSegments,
            highresHeights, highresSegments,
            scale,
            new Vector3(row * s, 3, col * s)
        )
    }
}

// let terrainHeights0_s = createTerrainHeight(segments, s, noiseScale, new Vector2(0, s))

// const track = new Track(
//     500, 500, 6,
//     20, 50,
//     60, 150,
//     0
// );
// let raceTrackHeights = track.canvas.getImageData(true)

// if (terrainHeights.length != raceTrackHeights.length) {
//     console.log('Lenght Mismatch')
// }

// let heights = new Float32Array(raceTrackHeights.length)
// for (let i = 0; i < raceTrackHeights.length; i++) {
//     heights[i] = Math.min(0, (-raceTrackHeights[i] + terrainHeights[i]) / 2)
// }

// createTerrain(game, terrainHeights00, segments, scale, new Vector3(0, 0, 0))


// for (let i = 0; i < 40; i++) {
//     spawnRandomObject(game, 50, 50);
// }

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
