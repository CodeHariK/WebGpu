// Enemy Arena — every tunable number and every behaviour switch in one place (the UI edits these).
// Turn a behaviour off to see what it adds; keep only what earns its place.

export type Toggles = {
    hearing: boolean; // footsteps / noises make enemies come and look
    shout: boolean; // one spots you → everyone within shoutRadius joins
    leash: boolean; // too far from home → give up and walk back
    slots: boolean; // surround: each takes a slot on a ring round the player (off: run straight at you)
    takeTurns: boolean; // attack tokens: only `tokens` enemies attack at once (off: everyone hits whenever close)
    windup: boolean; // a visible wind-up before each hit (off: instant hits)
    cover: boolean; // ranged enemies hide at cover markers between shots
    retreat: boolean; // hurt (1 hp left) → back off for a while
    search: boolean; // lost you → go to the last seen spot and look around (off: straight home)
};

export type Settings = Toggles & {
    viewAngle: number; // degrees, whole cone
    viewRange: number; // metres
    detectTime: number; // seconds of full view to be spotted (close = faster)
    shoutRadius: number;
    leashRadius: number;
    slotRadius: number; // the melee ring
    slotCount: number;
    tokens: number;
    tokenRest: number; // seconds a returned token rests
    windupTime: number;
    enemySpeed: number; // m/s when fighting (patrol walks at half)
    playerSpeed: number;
    loseAfter: number; // seconds unseen → lost you
    searchTime: number;
    think: number; // seconds between decisions (movement is still every frame)
};

export const DEFAULT_SETTINGS: Settings = {
    hearing: true,
    shout: true,
    leash: true,
    slots: true,
    takeTurns: true,
    windup: true,
    cover: true,
    retreat: true,
    search: true,
    viewAngle: 110,
    viewRange: 12,
    detectTime: 0.9,
    shoutRadius: 14,
    leashRadius: 22,
    slotRadius: 2.6,
    slotCount: 8,
    tokens: 1,
    tokenRest: 0.6,
    windupTime: 0.5,
    enemySpeed: 4.2,
    playerSpeed: 5.5,
    loseAfter: 3,
    searchTime: 4,
    think: 0.25,
};
