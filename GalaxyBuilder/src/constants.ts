import * as THREE from 'three';

export const UNITX = new THREE.Vector3(1, 0, 0);
export const UNITY = new THREE.Vector3(0, 1, 0);
export const UNITZ = new THREE.Vector3(0, 0, 1);

export const PI2 = THREE.MathUtils.DEG2RAD * 90

export function RandomString(length: number) {
    return (Math.random() + 1).toString(36).substring(7, 7 + length)
}