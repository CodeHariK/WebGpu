// Islands — an island's height: noise (4 sizes added together, each half the size and half as
// strong) plus a hill that is 1 in the island's middle and falls to 0 at its size away
// (1 − (distance / size)²). Land is where the height is above sea level (0.5): near the middle the
// hill keeps it land, near the edge only the noise's high spots poke up (headlands, islets), the low
// spots sink (bays). `noiseStrength` says how much the noise counts against the hill. Each island
// reads the noise at its own offset (salt).
import type { NoiseFunction2D } from 'simplex-noise';
import type { Hill } from './Map25Parts';
import type { Settings } from './Map25Settings';

const SEA_LEVEL = 0.5;

// 4 sizes of noise added, -1..1.
function fbm(p_noise: NoiseFunction2D, p_x: number, p_y: number): number {
    let sum = 0;
    let strength = 0.5;
    let scale = 1;
    for (let i = 0; i < 4; i++) {
        sum += p_noise(p_x * scale, p_y * scale) * strength;
        strength *= 0.5;
        scale *= 2;
    }
    return sum / 0.9375;
}

// Roughly pixels to the coast (negative on land): the height below sea level, scaled to pixels. Only
// its sign matters much: the real distance is measured afterwards (Map25Shape).
export function hillDistance(p_x: number, p_y: number, p_hill: Hill, p_settings: Settings, p_noise: NoiseFunction2D): number {
    const r = Math.hypot(p_x - p_hill.x, p_y - p_hill.y) / p_hill.size;
    const falloff = 1 - r * r;
    const noise = fbm(p_noise, p_x / p_settings.noiseScale + p_hill.salt, p_y / p_settings.noiseScale - p_hill.salt);
    const height = falloff * (1 - p_settings.noiseStrength) + (0.5 + 0.5 * noise) * p_settings.noiseStrength;
    return (SEA_LEVEL - height) * p_hill.size;
}
