import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { Game } from "./game";
import { Vector3 } from "three";

export function loadCar(game: Game, name: string, position: Vector3) {
    // Load the GLTF model
    const loader = new GLTFLoader();
    loader.load(
        'src/assets/carkit/' + name,
        (gltf) => {
            const model = gltf.scene;
            model.position.set(position.x, position.y, position.z)
            game.SCENE.add(model);
            console.log('Model loaded:', model);
        },
        (xhr) => {
            console.log(`Loading progress: ${(xhr.loaded / xhr.total) * 100}%`);
        },
        (error) => {
            console.error('An error occurred:', error);
        }
    );
}

export const CARMODELS = [
    'ambulance.glb',
    'box.glb',
    'cone-flat.glb',
    'cone.glb',
    'debris-bolt.glb',
    'debris-bumper.glb',
    'debris-door-window.glb',
    'debris-door.glb',
    'debris-drivetrain-axle.glb',
    'debris-drivetrain.glb',
    'debris-nut.glb',
    'debris-plate-a.glb',
    'debris-plate-b.glb',
    'debris-plate-small-a.glb',
    'debris-plate-small-b.glb',
    'debris-spoiler-a.glb',
    'debris-spoiler-b.glb',
    'debris-tire.glb',
    'delivery-flat.glb',
    'delivery.glb',
    'firetruck.glb',
    'garbage-truck.glb',
    'hatchback-sports.glb',
    'police.glb',
    'race-future.glb',
    'race.glb',
    'sedan-sports.glb',
    'sedan.glb',
    'suv-luxury.glb',
    'suv.glb',
    'taxi.glb',
    'tractor-police.glb',
    'tractor-shovel.glb',
    'tractor.glb',
    'truck-flat.glb',
    'truck.glb',
    'van.glb',
    'wheel-dark.glb',
    'wheel-default.glb',
    'wheel-racing.glb',
    'wheel-tractor-back.glb',
    'wheel-tractor-dark-back.glb',
    'wheel-tractor-dark-front.glb',
    'wheel-tractor-front.glb',
    'wheel-truck.glb'
]