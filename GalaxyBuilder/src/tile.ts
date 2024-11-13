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
    UniqueTiles: Map<string, Tile> = new Map()

    HashLeft: Map<string, Map<string, number>> = new Map()
    HashRight: Map<string, Map<string, number>> = new Map()
    HashFront: Map<string, Map<string, number>> = new Map()
    HashBack: Map<string, Map<string, number>> = new Map()

    constructor(x: number, y: number, z: number, tiles: Map<string, Tile>) {
        this.GRID_DIMENSION_X = x
        this.GRID_DIMENSION_Y = y
        this.GRID_DIMENSION_Z = z

        this.Tiles = tiles
        this.UniqueTiles = new Map()
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

        const HashLeft: Record<string, Record<string, number>> = {};
        this.HashLeft.forEach((innerMap, outerKey) => {
            HashLeft[outerKey] = Object.fromEntries(innerMap);
        });
        const HashRight: Record<string, Record<string, number>> = {};
        this.HashRight.forEach((innerMap, outerKey) => {
            HashRight[outerKey] = Object.fromEntries(innerMap);
        });
        const HashFront: Record<string, Record<string, number>> = {};
        this.HashFront.forEach((innerMap, outerKey) => {
            HashFront[outerKey] = Object.fromEntries(innerMap);
        });
        const HashBack: Record<string, Record<string, number>> = {};
        this.HashBack.forEach((innerMap, outerKey) => {
            HashBack[outerKey] = Object.fromEntries(innerMap);
        });

        return {
            GRID_DIMENSION_X: this.GRID_DIMENSION_X,
            GRID_DIMENSION_Y: this.GRID_DIMENSION_Y,
            GRID_DIMENSION_Z: this.GRID_DIMENSION_Z,
            ALL_COLORS: Array.from(this.ALL_COLORS.entries()).map(([key, value]) => ({
                key,
                value
            })),
            Tiles: Array.from(this.Tiles.entries()).map(([key, tile]) => ({
                key,
                tile
            })),
            UniqueTiles: Array.from(this.UniqueTiles.entries()).map(([key, tile]) => {
                return {
                    key,
                    tile
                }
            }),
            HashLeft: HashLeft,
            HashRight: HashRight,
            HashFront: HashFront,
            HashBack: HashBack,
        };
    }

    // Static method to create a TileSet instance from a JSON object
    static fromJSON(jsonString: any, game: Game): TileSet {

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

        tileSet.Tiles = new Map<string, Tile>(
            json.Tiles.map((entry: { key: string; tile: any }) => [
                entry.key,
                Tile.fromJSON(entry.tile, game)
            ])
        );

        tileSet.UniqueTiles = new Map<string, Tile>(
            json.UniqueTiles.map((entry: { key: string; tile: any }) => [
                entry.key,
                Tile.fromJSON(entry.tile, game)
            ])
        );

        {
            const HashLeft = new Map<string, Map<string, number>>();
            for (const outerKey in json.HashLeft) {
                if (Object.prototype.hasOwnProperty.call(json.HashLeft, outerKey)) {
                    const innerObj = json.HashLeft[outerKey];
                    const innerMap = new Map<string, number>(Object.entries(innerObj));
                    HashLeft.set(outerKey, innerMap);
                }
            }
            tileSet.HashLeft = HashLeft
        }

        {
            const HashRight = new Map<string, Map<string, number>>();
            for (const outerKey in json.HashRight) {
                if (Object.prototype.hasOwnProperty.call(json.HashRight, outerKey)) {
                    const innerObj = json.HashRight[outerKey];
                    const innerMap = new Map<string, number>(Object.entries(innerObj));
                    HashRight.set(outerKey, innerMap);
                }
            }
            tileSet.HashRight = HashRight
        }
        {
            const HashFront = new Map<string, Map<string, number>>();
            for (const outerKey in json.HashFront) {
                if (Object.prototype.hasOwnProperty.call(json.HashFront, outerKey)) {
                    const innerObj = json.HashFront[outerKey];
                    const innerMap = new Map<string, number>(Object.entries(innerObj));
                    HashFront.set(outerKey, innerMap);
                }
            }
            tileSet.HashFront = HashFront
        }
        {
            const HashBack = new Map<string, Map<string, number>>();
            for (const outerKey in json.HashBack) {
                if (Object.prototype.hasOwnProperty.call(json.HashBack, outerKey)) {
                    const innerObj = json.HashBack[outerKey];
                    const innerMap = new Map<string, number>(Object.entries(innerObj));
                    HashBack.set(outerKey, innerMap);
                }
            }
            tileSet.HashBack = HashBack
        }

        return tileSet;
    }
}

export class Tile {
    unittiles: (UnitTile | null)[][][]

    name: string = Constants.RandomString(6)
    codename: string = Constants.RandomString(6)
    hash: string = ""

    hashLeft: string = ""
    hashRight: string = ""
    hashBack: string = ""
    hashFront: string = ""

    mirrorX: string = ""
    mirrorZ: string = ""
    rotateY: string = ""
    rotateYrotateY: string = ""
    rotateYrotateYrotateY: string = ""

    constructor(game: Game) {
        this.unittiles = Array.from({ length: game.GRID_DIMENSION_X }, () =>
            Array.from({ length: game.GRID_DIMENSION_Y }, () =>
                Array.from({ length: game.GRID_DIMENSION_Z }, () => null)
            )
        )
    }

    static fromJSON(data: any, game: Game): Tile {
        const tile = new Tile(game);

        tile.unittiles = data.unittiles.map((layer: any[][]) =>
            layer.map((row: any[]) =>
                row.map((unitTileData: any) =>
                    unitTileData ? UnitTile.fromJSON(unitTileData) : null
                )
            )
        );

        tile.name = data.name;
        tile.codename = data.codename;

        tile.hash = data.hash;
        tile.hashLeft = data.hashLeft;
        tile.hashRight = data.hashRight;
        tile.hashBack = data.hashBack;
        tile.hashFront = data.hashFront;

        tile.rotateY = data.rotateY
        tile.rotateYrotateY = data.rotateYrotateY
        tile.rotateYrotateYrotateY = data.rotateYrotateYrotateY
        tile.mirrorX = data.mirrorX
        tile.mirrorZ = data.mirrorZ

        return tile;
    }

    get(x: number, y: number, z: number) {
        return this.unittiles[Math.floor(x)][Math.floor(y)][Math.floor(z)]
    }
    set(x: number, y: number, z: number, unittile: (UnitTile | null)) {
        this.unittiles[Math.floor(x)][Math.floor(y)][Math.floor(z)] = unittile
    }

    updateOne(game: Game) {
        {
            this.CalculateTileHash(game)
            game.TILESET.UniqueTiles.set(this.hash, this)
        }

        {
            const rotateY = Tile.RotateY(game, this)
            rotateY.name = this.name + '_rotateY'
            rotateY.CalculateTileHash(game)
            this.rotateY = rotateY.hash
            game.TILESET.UniqueTiles.set(this.rotateY, rotateY)
        }

        {
            const rotateYrotateY = Tile.RotateY(game, Tile.RotateY(game, this))
            rotateYrotateY.name = this.name + '_rotateYrotateY'
            rotateYrotateY.CalculateTileHash(game)
            this.rotateYrotateY = rotateYrotateY.hash
            game.TILESET.UniqueTiles.set(this.rotateYrotateY, rotateYrotateY)
        }

        {
            const rotateYrotateYrotateY = Tile.RotateY(game, Tile.RotateY(game, Tile.RotateY(game, this)))
            rotateYrotateYrotateY.name = this.name + '_rotateYrotateYrotateY'
            rotateYrotateYrotateY.CalculateTileHash(game)
            this.rotateYrotateYrotateY = rotateYrotateYrotateY.hash
            game.TILESET.UniqueTiles.set(this.rotateYrotateYrotateY, rotateYrotateYrotateY)
        }

        {
            const mirrorX = Tile.MirrorX(game, this)
            mirrorX.name = this.name + '_mirrorX'
            mirrorX.CalculateTileHash(game)
            this.mirrorX = mirrorX.hash
            game.TILESET.UniqueTiles.set(this.mirrorX, mirrorX)
        }

        {
            const mirrorZ = Tile.MirrorZ(game, this)
            mirrorZ.name = this.name + '_mirrorZ'
            mirrorZ.CalculateTileHash(game)
            this.mirrorZ = mirrorZ.hash
            game.TILESET.UniqueTiles.set(this.mirrorZ, mirrorZ)
        }
    }

    static update(game: Game) {

        game.TILESET.UniqueTiles = new Map()

        game.TILESET.HashLeft = new Map()
        game.TILESET.HashRight = new Map()
        game.TILESET.HashFront = new Map()
        game.TILESET.HashBack = new Map()

        game.TILESET.Tiles.forEach((tile) => {
            tile.updateOne(game)
        })
        game.TILE.updateOne(game)

        game.TILESET.UniqueTiles.forEach((tile, _) => {
            {
                let l = game.TILESET.HashLeft.get(tile.hashLeft) ?? new Map<string, number>()
                let count = (l.get(tile.hash) ?? 0) + 1
                l.set(tile.hash, count)
                game.TILESET.HashLeft.set(tile.hashLeft, l)
            }
            {
                let l = game.TILESET.HashRight.get(tile.hashRight) ?? new Map<string, number>()
                let count = (l.get(tile.hash) ?? 0) + 1
                l.set(tile.hash, count)
                game.TILESET.HashRight.set(tile.hashRight, l)
            }
            {
                let l = game.TILESET.HashFront.get(tile.hashFront) ?? new Map<string, number>()
                let count = (l.get(tile.hash) ?? 0) + 1
                l.set(tile.hash, count)
                game.TILESET.HashFront.set(tile.hashFront, l)
            }
            {
                let l = game.TILESET.HashBack.get(tile.hashBack) ?? new Map<string, number>()
                let count = (l.get(tile.hash) ?? 0) + 1
                l.set(tile.hash, count)
                game.TILESET.HashBack.set(tile.hashBack, l)
            }
        });
    }

    isEmpty(): boolean {
        let empty = true
        this.unittiles.forEach((layer) => {
            layer.forEach((row) => {
                row.forEach((t) => {
                    if (t !== null) {
                        empty = false
                    }
                })
            })
        })
        return empty
    }

    static MirrorX(game: Game, tile: Tile): Tile {
        let newTile = new Tile(game)

        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
                    let t = tile.unittiles[x][y][z]?.mirrorX(game)
                    if (t) {
                        newTile.unittiles[t.x][t.y][t.z] = t
                    }
                }
            }
        }
        return newTile
    }

    MirrorX(game: Game) {
        this.unittiles = Tile.MirrorX(game, this).unittiles
        game.rerenderScene()
    }

    static MirrorZ(game: Game, tile: Tile): Tile {
        let newTile = new Tile(game)

        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
                    let t = tile.unittiles[x][y][z]?.mirrorZ(game)
                    if (t) {
                        newTile.unittiles[t.x][t.y][t.z] = t
                    }
                }
            }
        }
        return newTile
    }

    MirrorZ(game: Game) {
        this.unittiles = Tile.MirrorZ(game, this).unittiles
        game.rerenderScene()
    }

    static RotateY(game: Game, tile: Tile): Tile {
        let newTile = new Tile(game)

        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {

                    let t = tile.unittiles[x][y][z]?.rotateY(game)
                    if (t) {
                        newTile.unittiles[t.x][t.y][t.z] = t
                    }
                }
            }
        }
        return newTile
    }

    RotateY(game: Game) {
        this.unittiles = Tile.RotateY(game, this).unittiles
        game.rerenderScene()
    }

    CalculateHash(game: Game) {
        let hash = ""

        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
                    let t = this.unittiles[x][y][z]
                    hash += t?.colorHash != null ? game.TILESET.ALL_COLORS.get(t.colorHash)!.hash + '.' : "___."
                }
                hash += '*'
            }
            hash += '+'
        }

        this.hash = hash
    }

    //Y+ -> Z+
    TileHashLeft_X(game: Game) {
        let hashhash = ""
        for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
            let hash = ""
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                hash += (this.unittiles[0][y][z]?.colorHash ?? "___") + "."
            }
            hashhash += hash
        }
        this.hashLeft = hashhash
    }

    //Y+ -> Z+
    TileHashRight_X(game: Game) {
        let hashhash = ""
        for (let z = 0; z < game.GRID_DIMENSION_Z; z++) {
            let hash = ""
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                hash += (this.unittiles[game.GRID_DIMENSION_X - 1][y][z]?.colorHash ?? "___") + "."
            }
            hashhash += hash
        }
        this.hashRight = hashhash
    }

    //Y+ -> X+
    TileHashBack_Z(game: Game) {
        let hashhash = ""
        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            let hash = ""
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                hash += (this.unittiles[x][y][0]?.colorHash ?? "___") + "."
            }
            hashhash += hash
        }
        this.hashBack = hashhash
    }

    //Y+ -> X+
    TileHashFront_Z(game: Game) {
        let hashhash = ""
        for (let x = 0; x < game.GRID_DIMENSION_X; x++) {
            let hash = ""
            for (let y = 0; y < game.GRID_DIMENSION_Y; y++) {
                hash += (this.unittiles[x][y][game.GRID_DIMENSION_Z - 1]?.colorHash ?? "___") + "."
            }
            hashhash += hash
        }
        this.hashFront = hashhash
    }

    CalculateTileHash(game: Game) {

        this.CalculateHash(game)

        this.TileHashLeft_X(game)
        this.TileHashRight_X(game)
        this.TileHashFront_Z(game)
        this.TileHashBack_Z(game)
    }

    delete(game: Game, x: number, y: number, z: number): boolean {
        let tile = game.TILE.get(x, y, z);

        if (tile) {
            game.TILE.set(x, y, z, null);

            game.rerenderScene()

            return true
        }
        return false
    }

    renderOne(game: Game, scene: THREE.Scene, dx: number, dy: number, name: string) {

        const axesHelper = new THREE.AxesHelper(game.GRID_DIMENSION_X + 1);
        axesHelper.position.set(dx, dy, 0)
        scene.add(axesHelper)

        const grid = RectangularGrid(game.GRID_DIMENSION_X, game.GRID_DIMENSION_Z)
        grid.position.set(dx + game.GRID_DIMENSION_HALF_X, 0.01 + dy, game.GRID_DIMENSION_HALF_Z)
        grid.name = "grid"
        scene.add(grid)

        const boxMesh = new THREE.Mesh();
        AddLabel(name + "\n" +
            this.hash + "\n" +
            "Left : " + this.hashLeft + "\n" +
            "Right : " + this.hashRight + "\n" +
            "Back : " + this.hashBack + "\n" +
            "Front : " + this.hashFront + "\n",
            boxMesh);
        scene.add(boxMesh);
        boxMesh.position.set(dx + 3 * game.GRID_DIMENSION_X, dy + game.GRID_DIMENSION_Y, 0);

        this.unittiles.forEach((layer) => {
            layer.forEach((row) => {
                row.forEach((tile) => {
                    if (tile instanceof UnitTile) {
                        tile.renderTile(game, scene, dx, dy)
                    }
                });
            });
        });
    }

    render(game: Game, scene: THREE.Scene) {
        Tile.update(game)

        this.renderOne(game, scene, 0, 0, "")

        game.TILESET.UniqueTiles.get(this.rotateY!)?.renderOne(game, scene, 0, game.GRID_DIMENSION_Y + 3, "rotateY")
        game.TILESET.UniqueTiles.get(this.rotateYrotateY!)?.renderOne(game, scene, 0, 2 * (game.GRID_DIMENSION_Y + 3), "rotateYrotateY")
        game.TILESET.UniqueTiles.get(this.rotateYrotateYrotateY!)?.renderOne(game, scene, 0, 3 * (game.GRID_DIMENSION_Y + 3), "rotateYrotateYrotateY")
        game.TILESET.UniqueTiles.get(this.mirrorX!)?.renderOne(game, scene, 0, 4 * (game.GRID_DIMENSION_Y + 3), "mirrorX")
        game.TILESET.UniqueTiles.get(this.mirrorZ!)?.renderOne(game, scene, 0, 5 * (game.GRID_DIMENSION_Y + 3), "mirrorZ")
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

    static fromJSON(data: any): UnitTile {
        return new UnitTile(data.x, data.y, data.z, data.colorHash);
    }

    mirrorX(game: Game): UnitTile {
        let u = new UnitTile(this.x, this.y, this.z, this.colorHash)
        u.x = game.GRID_DIMENSION_X - 1 - this.x
        return u
    }

    mirrorZ(game: Game): UnitTile {
        let u = new UnitTile(this.x, this.y, this.z, this.colorHash)
        u.z = game.GRID_DIMENSION_Z - 1 - this.z
        return u
    }

    rotateY(game: Game): UnitTile {
        let u = new UnitTile(this.x, this.y, this.z, this.colorHash)
        u.x = this.z
        u.z = game.GRID_DIMENSION_X - 1 - this.x
        return u
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

        UnitTile.renderCube(game.SCENE3D, fx, fy, fz, tilecolor);

        let unittile = new UnitTile(fx, fy, fz, game.TILE_COLOR_HASH.hash);

        game.TILE.set(x, y, z, unittile)

        game.rerenderScene()
    }

    static renderCube(scene: THREE.Scene, fx: number, fy: number, fz: number, tilecolor: TileColor | null) {
        const boxGeometry = new THREE.SphereGeometry(.5, 100, 100);
        const boxMaterial = new THREE.MeshMatcapMaterial({
            color: tilecolor?.color ?? 0xffffff,
            // wireframe: true,
        });
        const boxMesh = new THREE.Mesh(boxGeometry, boxMaterial);
        boxMesh.name = tilecolor != null ? UnitTile.name(fx, fy, fz, tilecolor) : "center"
        AddLabel(boxMesh.name, boxMesh);
        scene.add(boxMesh);
        boxMesh.position.set(fx + 0.5, fy + 0.5, fz + 0.5);
        // boxMesh.rotation.set(Math.PI / 4, Math.PI / 4, Math.PI / 4);
        boxMesh.castShadow = true;
        return boxMesh;
    }

    renderTile(game: Game, scene: THREE.Scene, dx: number, dy: number) {
        UnitTile.renderCube(scene, this.x + dx, this.y + dy, this.z, game.TILESET.ALL_COLORS.get(this.colorHash)!)
    }
}

export function CreateScene(game: Game) {
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

    game.TILE.render(game, game.SCENE3D)

    // UnitTile.renderCube(game.SCENE3D, game.GRID_DIMENSION_X / 2 - 0.5, game.GRID_DIMENSION_Y / 2 - 0.5, game.GRID_DIMENSION_Z / 2 - 0.5, null)

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
