// Puzzle Islands — environment mechanics: lasers, mirrors, receivers, pressure plates.
//
//   Emitter  > < v u   wall-mounted, always on; fires along its arrow
//            ) ( w n   same directions but PULSING on the beat clock (on PULSE_ON s, off the rest)
//            1 2 3 4   same directions as > < v u, BATTERY SLOT, empty (off)
//            5 6 7 8   battery slot with a battery in (on). Face it and press E to put a
//                      battery in or take it out — batteries move between emitters.
//   Mirror   / \       a box: push it like a crate; face it and press E to flip / ↔ \ (90°)
//   Receiver O         wall-mounted; every receiver lit by an always-on beam opens the H gates
//   Plate    _         floor; a crate on EVERY plate opens the J gates
//
// Always-on beams are walls of death for the solver (you may not step into one) and are what
// lights receivers. Pulsing beams never light receivers; they are timing hazards in real time
// only (the solver ignores them — you wait for the gap). Beams stop at walls, crates, closed
// gates and the switch gates H / J; mirrors always reflect.
import { boxAt, boxCell, BOX, isMirrorBox, DX, DY, tileAt, type Level, type State } from './Map19Rules';

export type Emitter = { cell: number; dir: number; pulse: boolean; slot: boolean };
// index % 4 = dir (0 +x, 1 -x, 2 +y, 3 -y); 4..7 pulse, 8..11 empty battery slot, 12..15 slot with battery
export const EMITTER_CHARS = '><vu)(wn12345678';
export const MIRROR_CHARS = '/\\';

export const PULSE_PERIOD = 2.4; // beat clock: seconds per pulse cycle
export const PULSE_ON = 1.2; // first part of the cycle is ON
export const PULSE_WARN = 0.45; // flicker this long before switching on

/** '/' : +x→-y, -x→+y, +y→-x, -y→+x · '\' : +x→+y, -x→-y, +y→+x, -y→-x */
const REFLECT = [
    [3, 2, 1, 0],
    [2, 3, 0, 1],
];

export type Beam = { emitter: Emitter; path: number[]; stop: number; lit: number };

/** Trace one beam: cells it crosses in order (floor and mirrors), the cell that stopped it, the receiver it lit. */
export function traceBeam(L: Level, s: State, e: Emitter): Beam {
    const path: number[] = [];
    const seen = new Set<number>();
    let c = e.cell;
    let dir = e.dir;
    for (;;) {
        const x = (c % L.w) + DX[dir];
        const y = Math.floor(c / L.w) + DY[dir];
        if (x < 0 || y < 0 || x >= L.w || y >= L.h) return { emitter: e, path, stop: -1, lit: -1 };
        c = y * L.w + x;
        if (seen.has(c * 4 + dir)) return { emitter: e, path, stop: -1, lit: -1 }; // mirror loop
        seen.add(c * 4 + dir);
        const b = tileAt(L, s, c);
        if (b === 'O') return { emitter: e, path, stop: c, lit: c };
        const box = boxAt(s, c);
        if (box === BOX.mirrorSlash || box === BOX.mirrorBack) {
            path.push(c);
            dir = REFLECT[box === BOX.mirrorBack ? 1 : 0][dir];
            continue;
        }
        const gateOpen = b === 'L' && (s.keys & (1 << L.gates.get(c)!)) !== 0;
        // Thin wooden walls (F) let beams through.
        const clear = b === '.' || b === 'K' || b === '_' || b === 'F' || gateOpen;
        if (!clear || box >= 0) return { emitter: e, path, stop: c, lit: -1 };
        path.push(c);
    }
}

export type Env = { beam: Uint8Array | null; openChannels: number; plateOpen: boolean; beams: Beam[] };

const NONE: Env = { beam: null, openChannels: 0, plateOpen: false, beams: [] };

/** A laser gate opens when every receiver on its channel is lit. */
export const laserGateOpen = (L: Level, s: State, cell: number) => ((envOf(L, s).openChannels >> (L.channel.get(cell) ?? 0)) & 1) === 1;
const cache = new WeakMap<State, Env>();

/** Static (solver) view of a state: always-on beam cells, whether H and J gates are open. Cached per State object. */
export function envOf(L: Level, s: State): Env {
    if (!L.emitters.length && !L.plates.length) return NONE;
    let env = cache.get(s);
    if (env) return env;
    const beams = L.emitters.filter((e, i) => !e.pulse && (!e.slot || (s.powered >> i) & 1)).map((e) => traceBeam(L, s, e));
    let beam: Uint8Array | null = null;
    if (beams.length) {
        beam = new Uint8Array(L.w * L.h);
        for (const b of beams) for (const c of b.path) beam[c] = 1;
    }
    const lit = new Set(beams.map((b) => b.lit));
    env = {
        beam,
        beams,
        openChannels: [0, 1, 2].reduce((m, ch) => {
            const rs = L.receivers.filter((r) => L.channel.get(r) === ch);
            return rs.length && rs.every((r) => lit.has(r)) ? m | (1 << ch) : m;
        }, 0),
        plateOpen: L.plates.length > 0 && L.plates.every((p) => boxAt(s, p) >= 0),
    };
    cache.set(s, env);
    return env;
}

/** Pulse phase at `time`: 'on', 'warn' (flickering, about to fire) or 'off'. */
export function pulsePhase(time: number): 'on' | 'warn' | 'off' {
    const t = ((time % PULSE_PERIOD) + PULSE_PERIOD) % PULSE_PERIOD;
    if (t < PULSE_ON) return 'on';
    return t > PULSE_PERIOD - PULSE_WARN ? 'warn' : 'off';
}

/** Every beam that can hurt right now (always-on + pulsing ones in their ON phase). */
export function liveBeams(L: Level, s: State, time: number): Beam[] {
    const out = [...envOf(L, s).beams];
    if (pulsePhase(time) === 'on') for (const e of L.emitters) if (e.pulse) out.push(traceBeam(L, s, e));
    return out;
}

const bits = (v: number) => {
    let n = 0;
    for (; v; v &= v - 1) n++;
    return n;
};

/** Batteries in hand: picked up + taken out of slots − put into slots. */
export const freeBatteries = (L: Level, s: State) => bits(s.batteries) + L.preloaded - bits(s.powered);

/** Battery-slot emitter at `cell` (index into L.emitters), or -1. */
export function slotAt(L: Level, cell: number): number {
    const i = L.emitters.findIndex((e) => e.cell === cell);
    return i >= 0 && L.emitters[i].slot ? i : -1;
}

export const isPowered = (s: State, i: number) => ((s.powered >> i) & 1) === 1;
/** Put a battery into slot emitter `i` / take it out. */
export const powerEmitter = (s: State, i: number): State => ({ ...s, powered: s.powered | (1 << i) });
export const unpowerEmitter = (s: State, i: number): State => ({ ...s, powered: s.powered & ~(1 << i) });

/** Flip the mirror box on `cell`: / ↔ \. */
export function rotateMirror(s: State, cell: number): State {
    return { ...s, boxes: s.boxes.map((b) => (boxCell(b) === cell && isMirrorBox(b) ? b ^ 1 : b)) };
}
