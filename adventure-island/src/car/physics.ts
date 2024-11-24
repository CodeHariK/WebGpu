import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';

export class Physics {
    static World = new RAPIER.World(new THREE.Vector3(0, -9.81, 0)); // Gravity

    static applyImpulse(rb: RAPIER.RigidBody, dir: THREE.Vector3) {
        dir.applyQuaternion(new THREE.Quaternion(
            rb.rotation().x,
            rb.rotation().y,
            rb.rotation().z,
            rb.rotation().w
        ));

        rb.applyImpulse(new RAPIER.Vector3(dir.x, dir.y, dir.z), true);
    }

    static add(ss: RAPIER.Vector3, vv: RAPIER.Vector3) {
        return new THREE.Vector3(ss.x + vv.x, ss.y + vv.y, ss.z + vv.z)
    }
    static toVec(ss: RAPIER.Vector3) {
        return new THREE.Vector3(ss.x, ss.y, ss.z)
    }
    static fromVec(ss: THREE.Vector3) {
        return new RAPIER.Vector3(ss.x, ss.y, ss.z)
    }

    static linearVelocityAtWorldPoint(rb: RAPIER.RigidBody, point: RAPIER.Vector) {
        // velocity = linearVelocity + angularVelocity * offsetFromCenter 

        return Physics.toVec(rb.linvel()).add(
            Physics.toVec(rb.angvel())
                .cross(
                    Physics.toVec(point).sub(rb.translation())))

    }
}