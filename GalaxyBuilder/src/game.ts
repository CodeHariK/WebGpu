import * as THREE from 'three';

import Stats from 'three/addons/libs/stats.module.js';

import { INPUT, InputKeys } from './input.ts';
import { CreateCSS2dRenderer, CreateHUDCamera, CreateOrbitControls, CreateRenderer, ResizeReset } from './camera.ts';
import { TileColor, CreateScene, Tile, TileSet } from './tile.ts';
import { AddLight } from './light.ts';
import { Gui } from './gui.ts';
import { CreateHUDView } from './hud.ts';
import { Test_TileJson } from './serialization_test.ts';


export class Game {
    GRID_DIMENSION_X: number = 2
    GRID_DIMENSION_Y: number = 2
    GRID_DIMENSION_Z: number = 2

    TILE_COLOR_HASH: TileColor = { color: 0xff0000, name: 'Red', hash: '8aI' }

    GRID_DIMENSION_HALF_X = Math.floor(this.GRID_DIMENSION_X / 2) + (this.GRID_DIMENSION_X % 2 === 1 ? 0.5 : 0)
    GRID_DIMENSION_HALF_Z = Math.floor(this.GRID_DIMENSION_Z / 2) + (this.GRID_DIMENSION_Z % 2 === 1 ? 0.5 : 0)

    ASPECT_RATIO: number = window.innerWidth / window.innerHeight

    ACTIVE_CAMERA: THREE.PerspectiveCamera | THREE.OrthographicCamera = new THREE.PerspectiveCamera(75, this.ASPECT_RATIO, 0.1, 1000);

    HUD_ORTHOGRAPHIC_CAMERA = CreateHUDCamera(this.ASPECT_RATIO);

    LABEL_RENDERER = CreateCSS2dRenderer()
    RENDERER = CreateRenderer()
    ORBIT_CONTROLS = CreateOrbitControls(this)

    TILE: Tile = new Tile(this)

    TILESET: TileSet = new TileSet(this.GRID_DIMENSION_X, this.GRID_DIMENSION_Y, this.GRID_DIMENSION_Z, new Map());

    Intersects: THREE.Intersection[] = [];

    SCENE3D = new THREE.Scene();
    SCENEHUD = new THREE.Scene();

    constructor() {
        INPUT.RegisterKeys()

        this.ACTIVE_CAMERA.position.set(-2, 5, 5);
        this.ACTIVE_CAMERA.lookAt(0, 0, 0);

        CreateHUDView(this.SCENEHUD, this.ASPECT_RATIO)

        AddLight(this.SCENE3D)

        let KeyPressed = false

        ResizeReset(this)

        Gui.AddGUI(this)

        CreateScene(this)

        console.log("Serialization Working", JSON.stringify(TileSet.fromJSON(Test_TileJson)) == Test_TileJson)

        //--------------------------------------------
        const stats = new Stats()
        document.body.appendChild(stats.dom)

        const animate = (time: number) => {
            if (INPUT.current == InputKeys.ArrowDOWN && !KeyPressed) {
                document.querySelectorAll(".label").forEach((label) => {
                    label.remove();
                });
            }

            this.ORBIT_CONTROLS.update();

            this.LABEL_RENDERER.render(this.SCENE3D, this.ACTIVE_CAMERA);

            // Clear the whole canvas
            this.RENDERER.clear();

            // Render the 3D scene with the perspective camera
            this.RENDERER.render(this.SCENE3D, this.ACTIVE_CAMERA);

            // Clear depth to render HUD elements on top
            this.RENDERER.clearDepth();

            // Render the HUD scene with the orthographic camera
            this.RENDERER.render(this.SCENEHUD, this.HUD_ORTHOGRAPHIC_CAMERA);

            if (INPUT.current) {
                KeyPressed = true
            } else {
                KeyPressed = false
            }

            stats.update()
        };
        this.RENDERER.setAnimationLoop(animate);
    }

    updateDimension(x: number | null, y: number | null, z: number | null) {
        if (x) {
            this.GRID_DIMENSION_X = x
        }
        if (y) {
            this.GRID_DIMENSION_Y = y
        }
        if (z) {
            this.GRID_DIMENSION_Z = z
        }

        this.GRID_DIMENSION_HALF_X = Math.floor(this.GRID_DIMENSION_X / 2) + (this.GRID_DIMENSION_X % 2 === 1 ? 0.5 : 0)
        this.GRID_DIMENSION_HALF_Z = Math.floor(this.GRID_DIMENSION_Z / 2) + (this.GRID_DIMENSION_Z % 2 === 1 ? 0.5 : 0)

        this.reset()
    }

    updateTileColor(color: TileColor) {
        this.TILE_COLOR_HASH = color
        this.rerenderScene()

    }

    rerenderScene() {
        this.SCENE3D.clear()

        document.querySelectorAll(".label").forEach((label) => {
            label.remove();
        });

        this.SCENE3D.children.forEach((child) => {
            child.removeFromParent()
        })
        CreateScene(this)
    }

    reset() {
        this.TILE = new Tile(this)

        this.rerenderScene()
    }

    save() {
        this.TILESET.Tiles.set(this.TILE.toString(), this.TILE)

        let jsonString = JSON.stringify(this.TILESET, null, 2)

        console.log(this.TILESET)
        console.log(jsonString)

        // saveJSONToFile(jsonString, 'unitTileMap.json');
    }
}

function saveJSONToFile(json: string, filename: string) {
    const blob = new Blob([json], { type: 'application/json' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}