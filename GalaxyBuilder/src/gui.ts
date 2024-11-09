import { GUI } from 'three/addons/libs/lil-gui.module.min.js';
import { CreateNewCamera } from './camera';
import { Game } from './game';

export function AddGUI(game: Game) {
    const gui = new GUI();
    const GalaxySettings = {
        CameraType: 'Perspective',

        XDim: game.GRID_DIMENSION_X,
        YDim: game.GRID_DIMENSION_Y,
        ZDim: game.GRID_DIMENSION_Z,

        color: 0xff0000,

        reset: () => {
            GalaxySettings.XDim = 2
            GalaxySettings.YDim = 2
            GalaxySettings.ZDim = 2
            GalaxySettings.color = 0xff0000;

            game.updateDimension(GalaxySettings.XDim, GalaxySettings.YDim, GalaxySettings.ZDim)
            game.updateTileColor(GalaxySettings.color)

            xdimControl.updateDisplay();
            ydimControl.updateDisplay();
            zdimControl.updateDisplay();
            colorControl.updateDisplay();
        },

        save: () => {
            game.save()
        }
    };

    gui.add(GalaxySettings, 'CameraType', ['Perspective', 'Orthographic'])
        .onChange((value) => {
            CreateNewCamera(game, value === 'Perspective');
        });

    // GUI controls for dimensions
    const xdimControl = gui.add(GalaxySettings, 'XDim', 2, 8).onChange((x) => { game.updateDimension(x, null, null) }).name('X Dimension').step(1);
    const ydimControl = gui.add(GalaxySettings, 'YDim', 2, 8).onChange((y) => { game.updateDimension(null, y, null) }).name('Y Dimension').step(1);
    const zdimControl = gui.add(GalaxySettings, 'ZDim', 2, 8).onChange((z) => { game.updateDimension(null, null, z) }).name('Z Dimension').step(1);

    const colorControl = gui.addColor(GalaxySettings, 'color').onChange((color) => { game.updateTileColor(color) }).name('Color');

    gui.add(GalaxySettings, 'reset').name('Reset Tile');

    gui.add(GalaxySettings, 'save').name('Save Tile');
}
