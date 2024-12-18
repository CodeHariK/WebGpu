import { ColorController, GUI, NumberController } from 'three/addons/libs/lil-gui.module.min.js';
import { CreateNewCamera } from './camera';
import { Game } from './game';
import { RandomString } from './constants';
import { TileColor } from './tile';

export class Gui {

    static gui: GUI = new GUI()

    static CameraType: 'Perspective' | 'Orthographic' = 'Perspective'

    static xdimControl: NumberController<Game, "GRID_DIMENSION_X">
    static ydimControl: NumberController<Game, "GRID_DIMENSION_Y">
    static zdimControl: NumberController<Game, "GRID_DIMENSION_Z">
    static colorHashControl: ColorController<TileColor, "color">

    static newTile = (game: Game) => {
        game.newTile()
        Gui.AddGUI(game);
    }

    static reset = (game: Game) => {
        game.updateDimension(2, 2, 2)
        game.updateTileColor({ color: 0xff0000, name: 'Red', hash: '8aI' })

        Gui.xdimControl.updateDisplay();
        Gui.ydimControl.updateDisplay();
        Gui.zdimControl.updateDisplay();
        Gui.colorHashControl.updateDisplay();
    }

    static selectTile = (game: Game, tilehash: string) => {
        const selected = game.TILESET.Tiles.get(tilehash)
        if (selected) {
            game.TILE = selected
        }
        game.rerenderScene()
        Gui.AddGUI(game);
    }

    static DeleteTile = (game: Game) => {
        game.TILESET.Tiles.delete(game.TILE.codename)
        game.reset()
        Gui.AddGUI(game);
    }

    static mirrorX = (game: Game) => {
        game.TILE.MirrorX(game)
    }
    static rotateY = (game: Game) => {
        game.TILE.RotateY(game)
    }

    static save = (game: Game) => {
        console.clear()
        game.save()
        Gui.AddGUI(game);
    }

    static addColor = (game: Game) => {

        let newColor = Math.floor(Math.random() * (2 << 23))
        let newHash = RandomString(3)

        if (
            !Object.values(game.TILESET.ALL_COLORS).some(colorObj =>
                colorObj.color === newColor || colorObj.name === newHash || colorObj.hash === newHash
            )
        ) {
            game.TILESET.ALL_COLORS.set(newHash,
                new TileColor(newHash, newColor, newHash))

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


        const tileFolder = Gui.gui.addFolder(`Tiles`)

        const tileStats = { totalTiles: `Total tiles: ${game.TILESET.UniqueTiles.size}` };
        tileFolder.add(tileStats, 'totalTiles').name('Total Tiles').disable();

        game.TILESET.Tiles.forEach((t) => {
            tileFolder.add({ selectTile: () => Gui.selectTile(game, t.codename) }, 'selectTile').name(t.name + ' : (' + t.codename + ')');
        })

        Gui.gui.add(game.TILE, 'name').name('Tile Name')
        Gui.gui.add({ DeleteTile: () => Gui.DeleteTile(game) }, 'DeleteTile').name('Delete Tile');

        Gui.gui.add({ MirrorX: () => Gui.mirrorX(game) }, 'MirrorX').name('Mirror X');
        Gui.gui.add({ rotateY: () => Gui.rotateY(game) }, 'rotateY').name('Rotate Y');

        Gui.gui.add({ reset: () => Gui.reset(game) }, 'reset').name('Reset Tile');
        Gui.gui.add({ newTile: () => Gui.newTile(game) }, 'newTile').name('Save/New Tile');
        Gui.gui.add({ save: () => Gui.save(game) }, 'save').name('Save to Local');

        Gui.colorHashControl = Gui.gui.addColor(game.TILE_COLOR_HASH, 'color').name('Color').disable();

        const colorFolder = Gui.gui.addFolder(`Colors`)
        colorFolder.add({ addColor: () => Gui.addColor(game) }, 'addColor').name('Add Color');

        game.TILESET.ALL_COLORS.forEach((tilecolor, colorHash) => {

            const folder = colorFolder.addFolder(`Color ${tilecolor.name}`);

            // Add name input for the color
            folder.add(tilecolor, 'hash')
                .name('Hash')
                .disable();

            folder.add(tilecolor, 'name')
                .name('Name')
                .onChange((newName) => {
                    game.TILESET.updateAllColors(colorHash, newName, null)
                    game.rerenderScene()
                });

            // Create a color picker
            folder.addColor(tilecolor, 'color')
                .name('Color')
                .onChange((newColor) => {
                    game.TILESET.updateAllColors(colorHash, null, newColor)
                    game.rerenderScene()
                });

            folder.add({
                delete: () => {

                    let used = false

                    game.TILESET.Tiles.forEach((tile) => {
                        tile.unittiles.forEach((layer) => {
                            layer.forEach((row) => {
                                row.forEach((t) => {
                                    if (t?.colorHash == colorHash) {
                                        used = true
                                    }
                                })
                            })
                        })
                    })

                    game.TILE.unittiles.forEach((layer) => {
                        layer.forEach((row) => {
                            row.forEach((t) => {
                                if (t?.colorHash == colorHash) {
                                    used = true
                                }
                            })
                        })
                    })

                    if (!used) {
                        game.TILESET.ALL_COLORS.delete(colorHash);
                        Gui.AddGUI(game);
                    } else {
                        console.log("Cannot delete color in use")
                    }

                }
            }, 'delete').name('Delete Color');

            folder.add({
                select: () => {
                    game.updateTileColor(game.TILESET.ALL_COLORS.get(colorHash)!)

                    Gui.AddGUI(game);
                }
            }, 'select').name('Select Color');

            folder.open();
        });
    }
}
