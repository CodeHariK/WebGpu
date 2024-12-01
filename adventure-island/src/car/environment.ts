import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Game } from './game';

export function createGround(game: Game, width: number, height: number, position: THREE.Vector3) {
    // Load the texture
    const textureLoader = new THREE.TextureLoader();
    const groundTexture = textureLoader.load('./src/assets/ground_grid.png');
    groundTexture.wrapS = THREE.RepeatWrapping; // Enable texture wrapping on S (horizontal) axis
    groundTexture.wrapT = THREE.RepeatWrapping; // Enable texture wrapping on T (vertical) axis
    groundTexture.repeat.set(50, 50); // Adjust the repeat to scale the texture
    const groundMaterial = new THREE.MeshToonMaterial({ map: groundTexture });

    // Create ground geometry and material
    const groundGeometry = new THREE.PlaneGeometry(width, height);

    const ground = new THREE.Mesh(groundGeometry, groundMaterial);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    const groundColliderDesc = RAPIER.ColliderDesc.cuboid(width / 2, 0.1, height / 2).setTranslation(0, position.y - .1, 0);
    game.WORLD.createCollider(groundColliderDesc);
    game.SCENE.add(ground);

    ground.position.set(position.x, position.y, position.z)
}

export function spawnRandomObject(game: Game, width: number, height: number) {
    const objectGeometries = [
        new THREE.SphereGeometry(1, 32, 32),
        new THREE.BoxGeometry(2, 2, 2),
        new THREE.ConeGeometry(1, 2, 32),
    ];
    const geometry = objectGeometries[Math.floor(Math.random() * objectGeometries.length)];
    const material = new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff });
    const mesh = new THREE.Mesh(geometry, material);

    // Random spawn position above terrain
    mesh.position.set(
        (Math.random() - 0.5) * width, // X
        20 + Math.random() * 10,   // Y (above terrain)
        (Math.random() - 0.5) * height // Z
    );
    mesh.castShadow = true;
    mesh.receiveShadow = true;

    game.SCENE.add(mesh);

    // Create RigidBody and Collider
    const bodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(
        mesh.position.x,
        mesh.position.y,
        mesh.position.z
    );
    const rigidBody = game.WORLD.createRigidBody(bodyDesc);

    let colliderDesc: RAPIER.ColliderDesc;
    if (geometry instanceof THREE.SphereGeometry) {
        colliderDesc = RAPIER.ColliderDesc.ball(1); // Sphere radius
    } else if (geometry instanceof THREE.BoxGeometry) {
        colliderDesc = RAPIER.ColliderDesc.cuboid(1, 1, 1); // Box half extents
    } else if (geometry instanceof THREE.ConeGeometry) {
        colliderDesc = RAPIER.ColliderDesc.cone(1, 1); // Cone height and radius
    }

    game.WORLD.createCollider(colliderDesc, rigidBody);

    mesh.userData = { name: "Obj", rigidBody: rigidBody };

    return { mesh, rigidBody };
}