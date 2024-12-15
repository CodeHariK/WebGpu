import { Keyboard } from './input';
import { Game } from './game';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, MeshStandardMaterial, BoxGeometry, TubeGeometry, MeshBasicMaterial, Vector2 } from 'three';
import { createTerrain } from './terrain';
import { CAR_TOY_CAR } from './prefab';
import { GenerateCanvasTrack, mapCanvasUpdate } from './canvas_track';

let game = new Game()


new Keyboard()

// createGround(game, 100, 100, new Vector3(0, 0, 0))

const segments = 400
const segmentsPerUnit = 2
const scale = new Vector3(segments / segmentsPerUnit, 20, segments / segmentsPerUnit)

const raceTrackMap = GenerateCanvasTrack(segments)

function createSubGrids(gridsize: number, heights: Float32Array<ArrayBufferLike>, row: number, col: number, segments: number, invresolution: number) {

    let subgridsize = Math.floor(gridsize / invresolution) + 1;

    let subgrid = new Float32Array(subgridsize * subgridsize).fill(0);

    for (let r = 0; r < subgridsize; r++) {
        for (let c = 0; c < subgridsize; c++) {

            let h = heights[
                row * segments * gridsize +
                r * invresolution * segments +
                col * gridsize +
                c * invresolution
            ];

            if (!h) {
                h = .5;
            }

            subgrid[r * subgridsize + c] = h;
        }
    }
    return { subgrid, subgridsize };
}

function createGrid(heights: Float32Array, segments: number, division: number, scale: Vector3) {

    const gridsize = segments / division

    for (let row = 0; row < division; row++) {
        for (let col = 0; col < division; col++) {

            let l = createSubGrids(gridsize, heights, row, col, segments, 4);
            let m = createSubGrids(gridsize, heights, row, col, segments, 2);
            let h = createSubGrids(gridsize, heights, row, col, segments, 1);

            createTerrain(game,
                l.subgrid, l.subgridsize,
                m.subgrid, m.subgridsize,
                h.subgrid, h.subgridsize,
                new Vector3(scale.x / division, scale.y, scale.z / division),
                new Vector3(row * (scale.x / division), scale.y / 2, col * (scale.z / division))
            )

        }
    }
}

createGrid(raceTrackMap.heights, segments, 10, scale)

// for (let i = 0; i < 40; i++) {
//     spawnRandomObject(game, new Vector3(s, 30, s));
// }

const car = CAR_TOY_CAR(game);


game.SCENE.traverse((object) => {
    // Check if the object is a Mesh
    if (object instanceof Mesh) {
        const axesHelper = new AxesHelper(1.2);
        object.add(axesHelper); // Add AxesHelper to the Mesh
    }
});

game.animate((deltaTime: number) => {
    car.update(game, deltaTime);

    mapCanvasUpdate(raceTrackMap, car, scale, segments, 100)
})


{
    let points = []

    raceTrackMap.track.samples.forEach((d) => {
        let checkpointGeometry = new BoxGeometry(5, 5, .2)
        const checkpointMaterial = new MeshStandardMaterial({ color: Math.random() * 0xffffff });
        const checkpointMesh = new Mesh(checkpointGeometry, checkpointMaterial);

        let sss = segments
        let x = d.pos.x * (scale.x / sss)
        let z = d.pos.y * (scale.z / sss)

        let y = (raceTrackMap.heights[Math.floor(d.pos.y) + Math.floor(d.pos.x) * sss] + 0.5) * scale.y + 1

        let a = Math.atan2(d.dir.x, d.dir.y)
        checkpointMesh.rotateY(a)

        checkpointMesh.position.set(x, y, z)

        let baricadeGeometry = new BoxGeometry(.5, .5, .5)
        const baricadeMaterial = new MeshStandardMaterial({ color: Math.random() * 0xffffff });
        const baricadeMesh1 = new Mesh(baricadeGeometry, baricadeMaterial);
        const baricadeMesh2 = new Mesh(baricadeGeometry, baricadeMaterial);

        let t = d.dir.clone().rotateAround(new Vector2(0, 0), Math.PI / 2).multiplyScalar(2)
        baricadeMesh1.position.set(x + t.x, y, z + t.y)
        baricadeMesh2.position.set(x - t.x, y, z - t.y)

        points.push(new Vector3(x, y, z))
        game.SCENE.add(checkpointMesh)
        game.SCENE.add(baricadeMesh1)
        game.SCENE.add(baricadeMesh2)
    })

    const tubeGeometry = new TubeGeometry(
        new CatmullRomCurve3(points), // Curve from points
        100, // Number of segments
        .5, // Radius
        8,   // Radial segments
        true // Closed
    );
    const tubeMaterial = new MeshBasicMaterial({ color: 0x00ff00 });
    const tube = new Mesh(tubeGeometry, tubeMaterial);
    game.SCENE.add(tube);
}
