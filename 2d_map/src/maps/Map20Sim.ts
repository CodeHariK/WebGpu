// Enemy Arena — one simulation step, in this order:
//   1. the player moves / swings / makes noise
//   2. the slot ring is rebuilt round the player (slotCount rays); every `think` seconds the
//      melee fighters re-pick their slots
//   3. each enemy: can it see you (one ray), then its state machine
//   4. the group: anyone who sees you tells the rest of the fight; free tokens go to whoever
//      is in place, best shot first
//   5. noises and shots age
import { makeRandom } from './Map20Math';
import { makeArena, PLAYER_START } from './Map20Arena';
import type { Settings } from './Map20Settings';
import type { Enemy, Input, Sim } from './Map20State';
import { updatePlayer } from './Map20Player';
import { canSee } from './Map20Senses';
import { startWindup, updateEnemy } from './Map20Enemy';
import { assignSlots, fighting, handOutAttacks, shareSighting, updateSlots, updateTokens } from './Map20Group';

let thinkLeft = 0;

export function createSim(settings: Settings, seed = 1): Sim {
    const { world, camps } = makeArena();
    const enemies: Enemy[] = camps.map((c, id) => ({
        id,
        kind: c.kind,
        pos: { ...c.home },
        vel: { x: 0, y: 0 },
        facing: (id * 2.1) % (Math.PI * 2),
        home: c.home,
        patrol: c.patrol,
        patrolIndex: 0,
        state: 'patrol',
        timer: 0,
        thinkLeft: 0,
        meter: 0,
        lastSeen: null,
        unseen: 0,
        goal: c.patrol[0],
        slot: -1,
        hasToken: false,
        lungeDir: { x: 1, y: 0 },
        aim: { x: 0, y: 0 },
        hitDone: false,
        hp: c.kind === 'melee' ? 3 : 2,
        bark: '',
        barkLeft: 0,
    }));
    return {
        world,
        settings,
        player: { pos: { ...PLAYER_START }, facing: -Math.PI / 2, hp: 10, attackLeft: 0, swingLeft: 0, hurtLeft: 0, sneaking: false },
        enemies,
        noises: [],
        shots: [],
        slots: [],
        tokens: { holders: [], resting: [] },
        time: 0,
        rand: makeRandom(seed),
        stats: { hitsTaken: 0, hitsGiven: 0, kills: 0 },
    };
}

export function step(sim: Sim, input: Input, dt: number): void {
    sim.time += dt;
    updatePlayer(sim, input, dt);
    updateSlots(sim);
    thinkLeft -= dt;
    if (thinkLeft <= 0) {
        thinkLeft = sim.settings.think;
        assignSlots(sim);
    }
    updateTokens(sim, dt);
    const seers = new Set<number>();
    for (const e of sim.enemies) {
        const sees = e.state !== 'dead' && canSee(sim, e);
        if (sees && fighting(e)) seers.add(e.id);
        updateEnemy(sim, e, dt, sees);
    }
    shareSighting(sim, seers);
    handOutAttacks(sim, startWindup);
    for (const n of sim.noises) n.age += dt;
    sim.noises = sim.noises.filter((n) => n.age < 0.8);
    for (const s of sim.shots) s.age += dt;
    sim.shots = sim.shots.filter((s) => s.age < 0.4);
}

