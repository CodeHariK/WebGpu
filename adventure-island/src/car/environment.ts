import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Game } from './game';

export function createGround(game: Game) {
    const groundGeometry = new THREE.PlaneGeometry(100, 100);
    const groundMaterial = new THREE.MeshToonMaterial({ color: 0x228b22 });
    const ground = new THREE.Mesh(groundGeometry, groundMaterial);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    const groundColliderDesc = RAPIER.ColliderDesc.cuboid(50, 0.1, 50).setTranslation(0, -.1, 0);
    game.WORLD.createCollider(groundColliderDesc);
    game.SCENE.add(ground);
}

export function spawnRandomObject(game: Game) {
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
        (Math.random() - 0.5) * 50, // X
        20 + Math.random() * 10,   // Y (above terrain)
        (Math.random() - 0.5) * 50 // Z
    );
    mesh.castShadow = true;
    mesh.receiveShadow = true;

    this.scene.add(mesh);

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