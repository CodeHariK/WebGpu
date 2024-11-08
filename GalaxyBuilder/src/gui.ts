import * as THREE from 'three';
import { GUI } from 'three/addons/libs/lil-gui.module.min.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { Game } from './main';
import { CreateOrbitControls } from './camera';

export function AddGUI(game: Game, ACTIVE_CAMERA: THREE.PerspectiveCamera | THREE.OrthographicCamera, ORBIT_CONTROLS: OrbitControls) {

    const material = new THREE.MeshBasicMaterial({ color: 0xff0000 });

    const gui = new GUI();
    const GalaxySettings = {
        CameraType: 'Perspective',

        XDim: 5,
        YDim: 5,
        ZDim: 5,

        color: material.color.getHex(),
    };

    gui.add(GalaxySettings, 'CameraType', ['Perspective', 'Orthographic'])
        .onChange((value) => {

            ACTIVE_CAMERA.clear()

            // Switch active camera based on GUI selection
            if (value === 'Perspective') {
                ACTIVE_CAMERA = new THREE.PerspectiveCamera(75, game.ASPECT_RATIO, 0.1, 1000);
                ACTIVE_CAMERA.position.set(-2, 5, 5);
            } else {
                const viewSize = 3;
                ACTIVE_CAMERA = new THREE.OrthographicCamera(
                    -viewSize * game.ASPECT_RATIO, viewSize * game.ASPECT_RATIO,
                    viewSize, -viewSize,
                    0.1, 1000
                );
                ACTIVE_CAMERA.position.set(3, 3, 6);
            }
            ACTIVE_CAMERA.lookAt(0, 0, 0);

            ORBIT_CONTROLS.dispose()

            ORBIT_CONTROLS = CreateOrbitControls(ACTIVE_CAMERA, game.RENDERER)
        });

    function updateDimensions() {
        game.GRID_DIMENTION_X = GalaxySettings.XDim
        game.GRID_DIMENTION_Y = GalaxySettings.YDim
        game.GRID_DIMENTION_Z = GalaxySettings.ZDim

        game.GRID_DIMENTION_HALF_X = Math.floor(game.GRID_DIMENTION_X / 2) + (game.GRID_DIMENTION_X % 2 === 1 ? 0.5 : 0)
        game.GRID_DIMENTION_HALF_Z = Math.floor(game.GRID_DIMENTION_Z / 2) + (game.GRID_DIMENTION_Z % 2 === 1 ? 0.5 : 0)
    }

    // Function to update cube color
    function updateColor(value: number) {
        material.color.set(value);
    }

    // GUI controls for dimensions
    gui.add(GalaxySettings, 'XDim', 2, 8).onChange(updateDimensions).name('X Dimension').step(1);
    gui.add(GalaxySettings, 'YDim', 2, 8).onChange(updateDimensions).name('Y Dimension').step(1);
    gui.add(GalaxySettings, 'ZDim', 2, 8).onChange(updateDimensions).name('Z Dimension').step(1);

    gui.addColor(GalaxySettings, 'color').onChange(updateColor).name('Color');

}