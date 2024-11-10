import * as THREE from 'three';
import * as Constants from './constants';
import { AddLabel } from './function';
import { RectangularGrid } from './grid';
import { Game } from './game';

const mousePosition = new THREE.Vector2();
const rayCaster = new THREE.Raycaster();

export class TileSet {
    GRID_DIMENSION_X: number
    GRID_DIMENSION_Y: number
    GRID_DIMENSION_Z: number

    ALL_COLORS: Map<string, TileColor> = new Map<string, TileColor>([
        ['8aI', new TileColor('8aI', 0xff0000, 'Red')]
    ]);

    Tiles: Map<string, Tile> = new Map()

    constructor(x: number, y: number, z: number, tiles: Map<string, Tile>) {
        this.GRID_DIMENSION_X = x
        this.GRID_DIMENSION_Y = y
        this.GRID_DIMENSION_Z = z

        this.Tiles = tiles
    }

    updateAllColors(hash: string, name: string | null, color: number | null) {
        let allColors = this.ALL_COLORS.get(hash)
        if (allColors && name) {
            allColors.name = name
        }
        if (allColors && color) {
            allColors.color = color
        }
    }

    toJSON() {
        return {
            GRID_DIMENSION_X: this.GRID_DIMENSION_X,
            GRID_DIMENSION_Y: this.GRID_DIMENSION_Y,
            GRID_DIMENSION_Z: this.GRID_DIMENSION_Z,
            ALL_COLORS: Array.from(this.ALL_COLORS.entries()).map(([key, value]) => ({
                key,
                value
            })),
            Tiles: Array.from(this.Tiles.entries()).map(([key, value]) => ({
                key,
                value
            }))
        };
    }

    // Static method to create a TileSet instance from a JSON object
    static fromJSON(jsonString: any): TileSet {

        const json = JSON.parse(jsonString)

        const tileSet = new TileSet(
            json.GRID_DIMENSION_X,
            json.GRID_DIMENSION_Y,
            json.GRID_DIMENSION_Z,
            new Map()
        );

        // Reconstruct ALL_COLORS map
        tileSet.ALL_COLORS = new Map<string, TileColor>(
            json.ALL_COLORS.map((entry: { key: string; value: TileColor }) => [
                entry.key,
                entry.value
            ])
        );

        // Reconstruct Tiles map (assuming Tile has an appropriate constructor)
        tileSet.Tiles = new Map<string, Tile>(
            json.Tiles.map((entry: { key: string; value: Tile }) => [
                entry.key,
                entry.value
            ])
        );

        return tileSet;
    }
}

export class Tile {
    tiles: (UnitTile | null)[][][]

    hashLeft: string
    hashRight: string
    hashBack: string
    hashFront: string

    constructor(game: Game) {
        this.tiles = Array.from({ length: game.GRID_DIMENSION_X }, () =>
            Array.from({ length: game.GRID_DIMENSION_Y }, () =>
                Array.from({ length: game.GRID_DIMENSION_Z }, () => null)
            )
        )

        this.hashLeft = ""
        this.hashRight = ""
        this.hashBack = ""
        this.hashFront = ""

        const axesHelper = new THREE.AxesHelper(5);  // Length of the axes lines
        axesHelper.position.set(0, 0, 0)
    }

    get(x: number, y: number, z: number) {
        return this.tiles[Math.floor(x)][Math.floor(y)][Math.floor(z)]
    }
    set(game: Game, x: number, y: number, z: number, unittile: (UnitTile | null)) {
        this.tiles[Math.floor(x)][Math.floor(y)][Math.floor(z)] = unittile

        this.CalculateTileHash(game)
    }

    scaleX(game: Game) {
        let newTile = new Tile(game)

        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
                    let t = this.tiles[x][y][z]
                    if (t !== null) {
                        t.scaleX(game)
                        newTile.tiles[t.x][t.y][t.z] = t
                    }
                }
            }
        }
        this.tiles = newTile.tiles
        game.rerenderScene()
    }

    rotateY(game: Game) {
        let newTile = new Tile(game)

        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
                    let t = this.tiles[x][y][z]
                    if (t !== null) {
                        t.rotateY(game)
                        newTile.tiles[t.x][t.y][t.z] = t
                    }
                }
            }
        }
        this.tiles = newTile.tiles
        game.rerenderScene()
    }

    // //Y+ -> Z+
    // TileHashLeft_X(game: Game) {
    //     let hashhash = ""
    //     for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
    //         let hash = ""
    //         for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
    //             hash += (game.TILE.tiles[0][y][z]?.colorHash ?? "_") + "."
    //         }
    //         hashhash += hash
    //     }
    //     this.hashLeft = hashhash
    // }

    // //Y+ -> Z+
    // TileHashRight_X(game: Game) {
    //     let hashhash = ""
    //     for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
    //         let hash = ""
    //         for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
    //             hash += (game.TILE.tiles[game.GRID_DIMENSION_X - 1][y][z]?.colorHash ?? "_") + "."
    //         }
    //         hashhash += hash
    //     }
    //     this.hashRight = hashhash
    // }

    // //Y+ -> X+
    // TileHashBack_Z(game: Game) {
    //     let hashhash = ""
    //     for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
    //         let hash = ""
    //         for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
    //             hash += (game.TILE.tiles[x][y][0]?.colorHash ?? "_") + "."
    //         }
    //         hashhash += hash
    //     }
    //     this.hashBack = hashhash
    // }

    // //Y+ -> X+
    // TileHashFront_Z(game: Game) {
    //     let hashhash = ""
    //     for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
    //         let hash = ""
    //         for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
    //             hash += (game.TILE.tiles[x][y][game.GRID_DIMENSION_Z - 1]?.colorHash ?? "_") + "."
    //         }
    //         hashhash += hash
    //     }
    //     this.hashFront = hashhash
    // }

    CalculateTileHash(game: Game) {
        // this.TileHashLeft_X(game)
        // this.TileHashRight_X(game)
        // this.TileHashFront_Z(game)
        // this.TileHashBack_Z(game)

        // console.log({
        //     "Left": this.hashLeft,
        //     "Right": this.hashRight,
        //     "Back": this.hashBack,
        //     "Front": this.hashFront
        // })
    }

    delete(game: Game, x: number, y: number, z: number): boolean {
        let tile = game.TILE.get(x, y, z);

        if (tile) {
            game.TILE.set(game, x, y, z, null);

            game.rerenderScene()

            return true
        }
        return false
    }
}

export class TileColor {
    hash: string
    color: number
    name: string

    constructor(hash: string, color: number, name: string) {
        this.hash = hash
        this.color = color
        this.name = name
    }
}

export class UnitTile {
    x: number
    y: number
    z: number

    colorHash: string

    constructor(x: number, y: number, z: number, colorHash: string) {
        this.x = x
        this.y = y
        this.z = z
        this.colorHash = colorHash
    }

    scaleX(game: Game) {
        this.x = game.GRID_DIMENSION_X - 1 - this.x
    }

    rotateY(game: Game) {
        let x = this.x
        let z = this.z
        this.x = z
        this.z = game.GRID_DIMENSION_X - 1 - x
    }

    name = (game: Game) => {
        let color = game.TILESET.ALL_COLORS.get(this.colorHash)!.name
        return `box_${this.x}_${this.y}_${this.z}_${color}`
    }
    static name = (x: number, y: number, z: number, color: TileColor) => {
        return `box_${x}_${y}_${z}_${color.name}`
    }

    static generateTile = (game: Game, x: number, y: number, z: number, tilecolor: TileColor) => {
        let fx = Math.floor(x)
        let fy = Math.floor(y)
        let fz = Math.floor(z)

        UnitTile.renderCube(game, fx, fy, fz, tilecolor);

        let unittile = new UnitTile(fx, fy, fz, game.TILE_COLOR_HASH.hash);

        game.TILE.set(game, x, y, z, unittile)

        game.rerenderScene()
    }

    static renderCube(game: Game, fx: number, fy: number, fz: number, tilecolor: TileColor | null) {
        const boxGeometry = new THREE.SphereGeometry(.5, 100, 100);
        const boxMaterial = new THREE.MeshMatcapMaterial({
            color: tilecolor?.color ?? 0xffffff,
            // wireframe: true,
        });
        const boxMesh = new THREE.Mesh(boxGeometry, boxMaterial);
        boxMesh.name = tilecolor != null ? UnitTile.name(fx, fy, fz, tilecolor) : "center"
        AddLabel(boxMesh.name, boxMesh);
        game.SCENE3D.add(boxMesh);
        boxMesh.position.set(fx + 0.5, fy + 0.5, fz + 0.5);
        // boxMesh.rotation.set(Math.PI / 4, Math.PI / 4, Math.PI / 4);
        boxMesh.castShadow = true;
        return boxMesh;
    }

    renderTile(game: Game) {
        UnitTile.renderCube(game, this.x, this.y, this.z, game.TILESET.ALL_COLORS.get(this.colorHash)!)
    }
}

export function CreateScene(game: Game) {
    const axesHelper = new THREE.AxesHelper(5);  // Length of the axes lines
    axesHelper.position.set(0, 0, 0)
    game.SCENE3D.add(axesHelper);

    const grid = RectangularGrid(game.GRID_DIMENSION_X, game.GRID_DIMENSION_Z)
    grid.position.set(game.GRID_DIMENSION_HALF_X, 0.01, game.GRID_DIMENSION_HALF_Z)
    grid.name = "grid"
    game.SCENE3D.add(grid)

    const planeMesh = new THREE.Mesh(
        new THREE.PlaneGeometry(game.GRID_DIMENSION_X, game.GRID_DIMENSION_Z),
        new THREE.MeshMatcapMaterial({
            color: 0xffffff,
            side: THREE.DoubleSide,
            visible: false,
        })
    );
    game.SCENE3D.add(planeMesh);
    planeMesh.rotation.x = -0.5 * Math.PI;
    planeMesh.receiveShadow = true;
    planeMesh.position.set(game.GRID_DIMENSION_HALF_X, 0, game.GRID_DIMENSION_HALF_Z)
    planeMesh.name = "ground";

    const highlight_planeMesh = new THREE.Mesh(
        new THREE.SphereGeometry(.2, 10),
        new THREE.MeshMatcapMaterial({
            color: 0xffffff,
            visible: false,
        })
    );
    game.SCENE3D.add(highlight_planeMesh);
    highlight_planeMesh.name = "highlight";
    highlight_planeMesh.rotation.x = -Constants.PI2;
    highlight_planeMesh.receiveShadow = true;
    highlight_planeMesh.position.set(0.5, 0, 0.5);

    game.TILE.tiles.forEach((layer) => {
        layer.forEach((row) => {
            row.forEach((tile) => {
                if (tile instanceof UnitTile) {
                    tile.renderTile(game)
                }
            });
        });
    });

    UnitTile.renderCube(game, game.GRID_DIMENSION_X / 2 - 0.5, game.GRID_DIMENSION_Y / 2 - 0.5, game.GRID_DIMENSION_Z / 2 - 0.5, null)

    window.onmousemove = (event) => { MouseMove(event, game, highlight_planeMesh) }
    window.ondblclick = () => { AddTile(game, highlight_planeMesh) }
    window.oncontextmenu = (event) => { DeleteTile(event, game, highlight_planeMesh) }
}

function AddTile(game: Game, highlight_planeMesh: THREE.Mesh<THREE.SphereGeometry, THREE.MeshMatcapMaterial, THREE.Object3DEventMap>) {
    for (let i = 0; i < game.Intersects.length; i++) {
        const dintersect = game.Intersects[i];

        if (dintersect.object.name.startsWith("box")) {

            let rotX = dintersect.normal?.dot(Constants.UNITX) ?? 0;
            let rotY = dintersect.normal?.dot(Constants.UNITY) ?? 0;
            let rotZ = dintersect.normal?.dot(Constants.UNITZ) ?? 0;

            const x = dintersect.object.position.x + rotX;
            const y = dintersect.object.position.y + rotY;
            const z = dintersect.object.position.z + rotZ;

            if (x >= game.GRID_DIMENSION_X || y >= game.GRID_DIMENSION_Y || z >= game.GRID_DIMENSION_Z ||
                x < 0 || y < 0 || z < 0
            ) {
                return undefined
            }

            // let tile = game.TILE.tiles[Math.floor(x)][Math.floor(y)][Math.floor(z)];
            let tile = game.TILE.get(x, y, z);

            if (!tile) {
                UnitTile.generateTile(game, x, y, z, game.TILE_COLOR_HASH);

                highlight_planeMesh.material.color.setHex(0xff0000);
            }
            break;
        }

        if (dintersect.object.name === "ground") {

            let { x, z } = new THREE.Vector3()
                .copy(dintersect.point)
                .floor()
                .addScalar(0.5);

            // let tile = game.TILE.tiles[Math.floor(x)][0][Math.floor(z)];
            let tile = game.TILE.get(x, 0, z);

            if (!tile) {
                UnitTile.generateTile(game, x, 0.5, z, game.TILE_COLOR_HASH);

                highlight_planeMesh.material.color.setHex(0xff0000);
            }
        }
    }
}

function MouseMove(e: MouseEvent, game: Game, highlight_planeMesh: THREE.Mesh<THREE.SphereGeometry, THREE.MeshMatcapMaterial, THREE.Object3DEventMap>) {
    mousePosition.x = (e.clientX / window.innerWidth) * 2 - 1;
    mousePosition.y = -(e.clientY / window.innerHeight) * 2 + 1;
    rayCaster.setFromCamera(mousePosition, game.ACTIVE_CAMERA);
    game.Intersects = rayCaster.intersectObjects(game.SCENE3D.children);
    let found = false;
    for (let i = 0; i < game.Intersects.length; i++) {
        const dintersect = game.Intersects[i];
        if (dintersect.object.name.startsWith("box")) {
            found = true;

            let rotX = dintersect.normal?.dot(Constants.UNITX) ?? 0;
            let rotY = dintersect.normal?.dot(Constants.UNITY) ?? 0;
            let rotZ = dintersect.normal?.dot(Constants.UNITZ) ?? 0;

            highlight_planeMesh.rotation.set(Constants.PI2 + rotZ * Constants.PI2, rotX * Constants.PI2, 0);

            highlight_planeMesh.position.set(dintersect.object.position.x + rotX / 2, dintersect.object.position.y + Math.abs(rotY) * .5, dintersect.object.position.z + +rotZ / 2);

            highlight_planeMesh.material.color.setHex(
                0x00ccff
                // collection[[x, z].toString()] ? 0xff0000 : 0x00ff00
            );
            break;
        }
        if (dintersect.object.name === "ground") {
            found = true;
            let { x, z } = new THREE.Vector3()
                .copy(dintersect.point)
                .floor()
                .addScalar(0.5);
            highlight_planeMesh.rotation.set(Constants.PI2, 0, 0);
            highlight_planeMesh.position.set(x, 0, z);
            highlight_planeMesh.material.color.setHex(
                0x00ff00
                // collection[[x, z].toString()] ? 0xff0000 : 0x00ff00
            );
            break;
        }
    }
    highlight_planeMesh.material.visible = found;
}

function DeleteTile(e: MouseEvent, game: Game, highlight_planeMesh: THREE.Mesh<THREE.SphereGeometry, THREE.MeshMatcapMaterial, THREE.Object3DEventMap>) {

    e.preventDefault();
    for (let i = 0; i < game.Intersects.length; i++) {
        const dintersect = game.Intersects[i];

        // console.log(dintersect.object.name, dintersect.object.position)

        if (dintersect.object.name.startsWith("box")) {

            const x = dintersect.object.position.x, y = dintersect.object.position.y, z = dintersect.object.position.z;

            // let tile = game.TILE.tiles[Math.floor(x)][Math.floor(y)][Math.floor(z)];
            // let tile = game.TILE.get(x, y, z);

            // console.log(tile)

            // if (tile) {
            //     tile.mesh.children.forEach((c) => {
            //         c.removeFromParent();
            //     });
            //     game.SCENE3D.remove(tile.mesh);
            //     highlight_planeMesh.material.color.setHex(0x00ff00);
            //     // game.TILE.tiles[Math.floor(x)][Math.floor(y)][Math.floor(z)] = null;
            //     game.TILE.set(game, x, y, z, null);
            //     break;
            // }

            if (game.TILE.delete(game, x, y, z)) {
                highlight_planeMesh.material.color.setHex(0x00ff00);
                // console.log("Delete Box", x, y, z)
                break;
            }
        }

        if (dintersect.object.name === "ground") {
            let { x, z } = new THREE.Vector3()
                .copy(dintersect.point)
                .floor()
                .addScalar(0.5);

            // // let tile = game.TILE.tiles[Math.floor(x)][0][Math.floor(z)];
            // let tile = game.TILE.get(x, 0, z);

            // if (tile) {
            //     game.SCENE3D.remove(tile.mesh);
            //     // game.TILE.tiles[Math.floor(x)][0][Math.floor(z)] = null;
            //     game.TILE.set(game, x, 0, z, null);
            //     highlight_planeMesh.material.color.setHex(0x00ff00);
            //     break;
            // }

            if (game.TILE.delete(game, x, 0, z)) {
                highlight_planeMesh.material.color.setHex(0x00ff00);
                break;
            }
        }
    }
}
