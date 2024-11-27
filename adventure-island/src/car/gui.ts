import { GUI } from 'three/addons/libs/lil-gui.module.min.js';

export class CarGui {

    static gui = new GUI();
    static panelSettings = {
        cameraMode: 'TPS', // Default camera mode
        directionalLightIntensity: 2,
        ambientLightIntensity: 1.5,
    };

    constructor() {
        // Camera mode dropdown
        CarGui.gui.add(CarGui.panelSettings, 'cameraMode', ['TPS', 'Orbit']).name('Camera Mode');

        // // Lighting controls
        // gui.add(panelSettings, 'directionalLightIntensity', 0, 5, 0.1)
        //     .name('Directional Light')
        //     .onChange((value: number) => {
        //         directionalLight.intensity = value;
        //     });

        // gui.add(panelSettings, 'ambientLightIntensity', 0, 5, 0.1)
        //     .name('Ambient Light')
        //     .onChange((value: number) => {
        //         ambientLight.intensity = value;
        //     });
    }
} 