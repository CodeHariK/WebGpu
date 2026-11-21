// Street Map — seeded randomness: a random number generator, a hash for whole numbers, and noise.
import { createNoise2D, type NoiseFunction2D } from 'simplex-noise';

// mulberry32: the same seed gives the same map.
export function makeRandom(p_seed: number): () => number {
    let t = p_seed >>> 0;
    return () => {
        t = (t + 0x6d2b79f5) >>> 0;
        let r = Math.imul(t ^ (t >>> 15), t | 1);
        r ^= r + Math.imul(r ^ (r >>> 7), r | 61);
        return ((r ^ (r >>> 14)) >>> 0) / 4294967296;
    };
}

// A random number 0..1 for three whole numbers (the same numbers, the same answer).
export function hash(p_a: number, p_b: number, p_c: number): number {
    let h = Math.imul(p_a | 0, 374761393) + Math.imul(p_b | 0, 668265263) + Math.imul(p_c | 0, 1442695041);
    h = Math.imul(h ^ (h >>> 13), 1274126177);
    h = Math.imul(h ^ (h >>> 16), 2246822519);
    return ((h ^ (h >>> 13)) >>> 0) / 4294967296;
}

export function makeNoise(p_random: () => number): NoiseFunction2D {
    return createNoise2D(p_random);
}

// p_octaves sizes of noise added (each half the size, half as strong), 0..1.
export function fbm(p_noise: NoiseFunction2D, p_x: number, p_y: number, p_octaves: number): number {
    let sum = 0;
    let strength = 0.5;
    let total = 0;
    let scale = 1;
    for (let i = 0; i < p_octaves; i++) {
        sum += p_noise(p_x * scale, p_y * scale) * strength;
        total += strength;
        strength *= 0.5;
        scale *= 2;
    }
    return 0.5 + 0.5 * (sum / total);
}
