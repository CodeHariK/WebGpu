import * as THREE from 'three';

import Stats from 'three/addons/libs/stats.module.js';

import { INPUT, InputKeys } from './input.ts';
import { CreateCSS2dRenderer, CreateHUDCamera, CreateOrbitControls, CreateRenderer, ResizeReset } from './camera.ts';
import { RegisterMouseMove } from './tile.ts';
import { AddLight } from './light.ts';
import { PI2 } from './constants.ts';
import { AddGUI } from './gui.ts';
import { CreateHUDView } from './hud.ts';
import { RectangularGrid } from './grid.ts';

export class Game {
  GRID_DIMENTION_X: number = 4
  GRID_DIMENTION_Y: number = 4
  GRID_DIMENTION_Z: number = 4

  GRID_DIMENTION_HALF_X = Math.floor(this.GRID_DIMENTION_X / 2) + (this.GRID_DIMENTION_X % 2 === 1 ? 0.5 : 0)
  GRID_DIMENTION_HALF_Z = Math.floor(this.GRID_DIMENTION_Z / 2) + (this.GRID_DIMENTION_Z % 2 === 1 ? 0.5 : 0)

  ASPECT_RATIO: number = window.innerWidth / window.innerHeight

  ACTIVE_CAMERA: THREE.PerspectiveCamera | THREE.OrthographicCamera = new THREE.PerspectiveCamera(75, this.ASPECT_RATIO, 0.1, 1000);

  HUD_ORTHOGRAPHIC_CAMERA = CreateHUDCamera(this.ASPECT_RATIO);

  LABEL_RENDERER = CreateCSS2dRenderer()
  RENDERER = CreateRenderer()
  ORBIT_CONTROLS = CreateOrbitControls(this.ACTIVE_CAMERA, this.RENDERER)

  collection: { [key: string]: THREE.Mesh } = {};
  intersects: THREE.Intersection[] = [];

  scene3D = new THREE.Scene();
  sceneHUD = new THREE.Scene();

  constructor() {
    INPUT.RegisterKeys()

    this.ACTIVE_CAMERA.position.set(-2, 5, 5);
    this.ACTIVE_CAMERA.lookAt(0, 0, 0);


    const axesHelper = new THREE.AxesHelper(5);  // Length of the axes lines
    axesHelper.position.set(0, 0, 0)
    this.scene3D.add(axesHelper);

    const grid = RectangularGrid(this.GRID_DIMENTION_X, this.GRID_DIMENTION_Z)
    grid.position.set(this.GRID_DIMENTION_HALF_X, 0.01, this.GRID_DIMENTION_HALF_Z)
    this.scene3D.add(grid)

    const planeMesh = new THREE.Mesh(
      new THREE.PlaneGeometry(this.GRID_DIMENTION_X, this.GRID_DIMENTION_Z),
      new THREE.MeshMatcapMaterial({
        color: 0xffffff,
        side: THREE.DoubleSide,
        visible: true,
      })
    );
    this.scene3D.add(planeMesh);
    planeMesh.rotation.x = -0.5 * Math.PI;
    planeMesh.receiveShadow = true;
    planeMesh.position.set(this.GRID_DIMENTION_HALF_X, 0, this.GRID_DIMENTION_HALF_Z)
    planeMesh.name = "ground";


    CreateHUDView(this.sceneHUD, this.ASPECT_RATIO)

    AddLight(this.scene3D)

    let KeyPressed = false

    // Initialize renderer and set autoClear to false
    this.RENDERER.autoClear = false;

    // Adjust camera and renderer on window resize

    ResizeReset(this.ACTIVE_CAMERA, this.ASPECT_RATIO, this.RENDERER)

    AddGUI(this, this.ACTIVE_CAMERA, this.ORBIT_CONTROLS)

    RegisterMouseMove(this, this.scene3D, this.ACTIVE_CAMERA)

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

      this.LABEL_RENDERER.render(game.scene3D, game.ACTIVE_CAMERA);

      // Clear the whole canvas
      this.RENDERER.clear();

      // Render the 3D scene with the perspective camera
      this.RENDERER.render(game.scene3D, game.ACTIVE_CAMERA);

      // Clear depth to render HUD elements on top
      this.RENDERER.clearDepth();

      // Render the HUD scene with the orthographic camera
      this.RENDERER.render(game.sceneHUD, game.HUD_ORTHOGRAPHIC_CAMERA);

      if (INPUT.current) {
        KeyPressed = true
      } else {
        KeyPressed = false
      }

      stats.update()
    };
    this.RENDERER.setAnimationLoop(animate);
  }

}

const game = new Game()
