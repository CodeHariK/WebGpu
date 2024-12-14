import { Keyboard } from './input';
import { Game } from './game';
import { Car } from './car';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, Line, LineBasicMaterial, BufferGeometry, BufferAttribute, MeshStandardMaterial, Vector2, MathUtils, PlaneGeometry, MeshPhysicalMaterial, LinearMipMapLinearFilter, LinearFilter, RepeatWrapping, TextureLoader, BoxGeometry } from 'three';
import { createTerrainHeight, createTerrain } from './terrain';
import { createGround, spawnRandomObject } from './environment';
import { CARMODELS, loadCar } from './model';
import { CAR_MONSTER_TRUCK, CAR_TOY_CAR, CAR_TURN } from './prefab';
import { createCheckbox } from './ui';
import { ColliderDesc, RigidBodyDesc } from '@dimforge/rapier3d';
import { UNIT_Z } from './physics';
import { GenerateCanvasTrack, mapCanvasUpdate } from './canvas_track';

let game = new Game()


new Keyboard()

// createGround(game, 100, 100, new Vector3(0, 0, 0))

const noiseScale = 0.1
const lowresSegments = 400
const highresSegments = 400
const scale = new Vector3(100, 10, 100)

const raceTrackMap = GenerateCanvasTrack(lowresSegments)

createTerrain(game,
    raceTrackMap.heights, lowresSegments,
    raceTrackMap.heights, highresSegments,
    scale,
    new Vector3(0, scale.y / 2, 0)
)

// for (let i = 0; i < 40; i++) {
//     spawnRandomObject(game, new Vector3(s, 30, s));
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

    mapCanvasUpdate(raceTrackMap, car, scale, lowresSegments, 100)
})

raceTrackMap.track.samples.forEach((d) => {
    let geometry = new BoxGeometry(1, 1, 1)
    const material = new MeshStandardMaterial({ color: Math.random() * 0xffffff });
    const mesh = new Mesh(geometry, material);

    let x = d.pos.x * (scale.x / lowresSegments)
    let z = d.pos.y * (scale.z / lowresSegments)
    let y = (raceTrackMap.heights[Math.floor(d.pos.y) + Math.floor(d.pos.x) * lowresSegments] + 1) * scale.y / 2 + 1
    mesh.position.set(x - scale.x / 2, y, z - scale.z / 2)


    // raceTrackMap.heights[]
    game.SCENE.add(mesh)
})
