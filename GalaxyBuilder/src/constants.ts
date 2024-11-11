import * as THREE from 'three';

export const UNITX = new THREE.Vector3(1, 0, 0);
export const UNITY = new THREE.Vector3(0, 1, 0);
export const UNITZ = new THREE.Vector3(0, 0, 1);

export const PI2 = THREE.MathUtils.DEG2RAD * 90

const allCharacters = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
const charactersLength = allCharacters.length;
export function RandomString(length: number): string {
    let result = '';
    for (let i = 0; i < length; i++) {
        const randomIndex = Math.floor(Math.random() * charactersLength);
        result += allCharacters[randomIndex];
    }
    return result;
}