import * as THREE from 'three';
import { Vector3, RigidBody, Quaternion } from '@dimforge/rapier3d';

export class Physics {

    // static applyImpulse(rb: RigidBody, dir: THREE.Vector3) {
    //     dir.applyQuaternion(new THREE.Quaternion(
    //         rb.rotation().x,
    //         rb.rotation().y,
    //         rb.rotation().z,
    //         rb.rotation().w
    //     ));

    //     rb.applyImpulse(new Vector3(dir.x, dir.y, dir.z), true);
    // }


    // The linear velocity at a point  P  in local space is:

    // vP = vcenter + omega * l

    // Where:
    // 	•	 vP : Linear velocity at the point  P .
    // 	•	 vcenter : Linear velocity of the rigid body’s center of mass.
    // 	•	 omega : Angular velocity of the rigid body (world space).
    // 	•	 local : Position of the point  P (in local space).
    // static linearVelocityAtLocalPoint(rb: RigidBody, point: Vector3) {
    //     return rVecAdd(rb.linvel(), rVec(point).cross(rb.angvel()))
    // }
    static linearVelocityAtWorldPoint(rb: RigidBody, point: Vector3) {
        return rVecAdd(
            rb.linvel(),
            rVecSub(point, rb.translation())
                .cross(rb.angvel())
        )
    }
}

export function rVecString(ss: Vector3) {
    return "" + ss.x.toFixed(2) + ", " + ss.y.toFixed(2) + ", " + ss.z.toFixed(2) + " - " + rVec(ss).length().toFixed(2)
}
export function rVec(ss: Vector3) {
    return new THREE.Vector3(ss.x, ss.y, ss.z)
}
export function rVecMag(ss: Vector3) {
    return rVec(ss).length()
}
export function rVecDot(ss: Vector3, vv: Vector3) {
    return rVec(ss).dot(rVec(vv))
}
export function rVecAddd(vectors: THREE.Vector3[]): THREE.Vector3 {
    const result = vectors.reduce((v, vv) => v.add(vv), new THREE.Vector3(0, 0, 0));
    return result;
}
export function rVecAdd(ss: Vector3, vv: Vector3) {
    return new THREE.Vector3(ss.x + vv.x, ss.y + vv.y, ss.z + vv.z)
}
export function rVecSub(ss: Vector3, vv: Vector3) {
    return new THREE.Vector3(ss.x - vv.x, ss.y - vv.y, ss.z - vv.z)
}
export function rVecMul(ss: Vector3, vv: Vector3) {
    return new THREE.Vector3(ss.x * vv.x, ss.y * vv.y, ss.z * vv.z)
}
export function rVecScale(ss: Vector3, vv: number) {
    return new THREE.Vector3(ss.x * vv, ss.y * vv, ss.z * vv)
}


export function rQuat(ss: Quaternion) {
    return new THREE.Quaternion(ss.x, ss.y, ss.z, ss.w)
}
export function rQuatMul(ss: Quaternion, vv: Quaternion) {
    return rQuat(ss).multiply(rQuat(vv))
}

export function clamp(ss: number, min: number, max: number) {
    return ss < min ? min : (ss > max ? max : ss)
}

export const ZERO = () => new THREE.Vector3(0, 0, 0)

export const UNIT_X = () => new THREE.Vector3(1, 0, 0)
export const UNIT_XN = () => new THREE.Vector3(-1, 0, 0)

export const UNIT_Y = () => new THREE.Vector3(0, 1, 0)
export const UNIT_YN = () => new THREE.Vector3(0, -1, 0)

export const UNIT_Z = () => new THREE.Vector3(0, 0, 1)
export const UNIT_ZN = () => new THREE.Vector3(0, 0, -1)
