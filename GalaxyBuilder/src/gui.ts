import { ColorController, GUI, NumberController } from 'three/addons/libs/lil-gui.module.min.js';
import { CreateNewCamera } from './camera';
import { Game } from './game';

export class Gui {

    static gui: GUI = new GUI()

    static CameraType: 'Perspective' | 'Orthographic' = 'Perspective'

    static xdimControl: NumberController<Game, "GRID_DIMENSION_X">
    static ydimControl: NumberController<Game, "GRID_DIMENSION_Y">
    static zdimControl: NumberController<Game, "GRID_DIMENSION_Z">
    static colorHashControl: ColorController<Game, "TILE_COLOR_HASH">

    static reset = (game: Game) => {
        game.updateDimension(2, 2, 2)
        game.updateTileColor({ color: 0xff0000, name: 'Red', hash: 0xff0000 })

        Gui.xdimControl.updateDisplay();
        Gui.ydimControl.updateDisplay();
        Gui.zdimControl.updateDisplay();
        Gui.colorHashControl.updateDisplay();
    }

    static save = (game: Game) => {
        game.save()
    }

    static genColor = (game: Game) => {

        let newColor = Math.floor(Math.random() * (2 << 23))

        if (
            !Object.values(game.TILESET.ALL_COLORS).some(colorObj =>
                colorObj.color === newColor || colorObj.name === newColor.toString() || colorObj.hash === newColor
            )
        ) {
            game.TILESET.ALL_COLORS[newColor] = {
                color: newColor,
                hash: newColor,
                name: newColor.toString(),
            }

            Gui.AddGUI(game)
        }
    }

    static AddGUI(game: Game) {
        Gui.gui.destroy()
        Gui.gui = new GUI()

        Gui.gui.add(Gui, 'CameraType', ['Perspective', 'Orthographic'])
            .onChange((value) => {
                CreateNewCamera(game, value === 'Perspective');
            });

        Gui.xdimControl = Gui.gui.add(game, 'GRID_DIMENSION_X', 2, 8).onChange((x) => { game.updateDimension(x, null, null) }).name('X Dimension').step(1);
        Gui.ydimControl = Gui.gui.add(game, 'GRID_DIMENSION_Y', 2, 8).onChange((y) => { game.updateDimension(null, y, null) }).name('Y Dimension').step(1);
        Gui.zdimControl = Gui.gui.add(game, 'GRID_DIMENSION_Z', 2, 8).onChange((z) => { game.updateDimension(null, null, z) }).name('Z Dimension').step(1);

        Gui.colorHashControl = Gui.gui.addColor(game, 'TILE_COLOR_HASH').name('color')

        Gui.gui.add({ reset: () => Gui.reset(game) }, 'reset').name('Reset Tile');
        Gui.gui.add({ save: () => Gui.save(game) }, 'save').name('Save Tile');
        Gui.gui.add({ genColor: () => Gui.genColor(game) }, 'genColor').name('Add Color');

        Object.entries(game.TILESET.ALL_COLORS).forEach(([key, value], index) => {

            const keyCode = parseInt(key);

            // Create a folder for each color
            const folder = Gui.gui.addFolder(`Color ${value.name}`);

            // Add name input for the color
            folder.add(value, 'name')
                .name('Name')
                .onChange((newName) => {
                    game.TILESET.ALL_COLORS[keyCode].name = newName;
                    // game.clearScene()
                });

            // Create a color picker
            folder.addColor(value, 'color')
                .name('Color')
                .onChange((newColor) => {
                    game.TILESET.ALL_COLORS[keyCode].color = newColor;
                    game.clearScene()
                });

            folder.add({
                delete: () => {
                    delete game.TILESET.ALL_COLORS[keyCode];
                    Gui.AddGUI(game);
                }
            }, 'delete').name('Delete Color');

            folder.add({
                select: () => {
                    game.updateTileColor(game.TILESET.ALL_COLORS[keyCode])
                    Gui.AddGUI(game);
                }
            }, 'select').name('Select Color');

            folder.open();
        });
    }
}
