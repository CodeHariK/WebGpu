// Combat Sandbox — every tunable number and every "feel" switch in one place (the UI edits these).
// Turn a feel switch off to see how much it carries; tune numbers until the fight is fun.

export type Feel = {
    shake: boolean; // screen shake on hits, kills, explosions
    hitStop: boolean; // the game freezes for a few frames when a hit lands
    knockback: boolean; // hits shove enemies (and you)
    flash: boolean; // hit things flash white
    particles: boolean; // debris, sparks, bubble pops
    numbers: boolean; // floating damage numbers
    slowmo: boolean; // perfect dodge slows time for a moment
    recoil: boolean; // firing nudges the player back and kicks the view
};

export type Settings = Feel & {
    // player
    playerSpeed: number; // m/s
    dashDistance: number; // m
    dashTime: number; // s
    dashCooldown: number; // s after the dash ends
    iframes: number; // s invincible from the start of a dash
    fireRate: number; // bubbles per second while the button is held
    bubbleSpeed: number; // m/s
    bubbleDamage: number;
    spread: number; // degrees, random each shot
    popRadius: number; // m: the close-range pop (right click)
    popDamage: number;
    popCooldown: number; // s
    maxJoy: number; // ammo
    playerHp: number;
    // enemies
    enemySpeed: number; // m/s (riflemen; others scale from it)
    aimTell: number; // s a rifleman's sight line shows before a burst
    boltSpeed: number; // m/s enemy shots
    windTime: number; // s a rifleman stands winding its key (double damage)
    chargeSpeed: number; // m/s riot toy charge
    // pressure
    gasSpeed: number; // m/s the gas line creeps up the arena (0 = off)
    gasPushBack: number; // m the gas falls back when a wave is cleared
    // camera
    viewSpan: number; // metres the view shows across (the arena is 32)
    aimLead: number; // metres the camera leans toward where you aim
    // feel amounts
    shakeAmount: number; // 0 … 2
    hitStopMs: number; // ms per hit (kills ×3)
};

export const DEFAULT_SETTINGS: Settings = {
    shake: true,
    hitStop: true,
    knockback: true,
    flash: true,
    particles: true,
    numbers: true,
    slowmo: true,
    recoil: true,
    playerSpeed: 7.5,
    dashDistance: 4.5,
    dashTime: 0.16,
    dashCooldown: 0.35,
    iframes: 0.22,
    fireRate: 9,
    bubbleSpeed: 24,
    bubbleDamage: 1,
    spread: 5,
    popRadius: 2.8,
    popDamage: 4,
    popCooldown: 1.0,
    maxJoy: 60,
    playerHp: 6,
    enemySpeed: 4.0,
    aimTell: 0.5,
    boltSpeed: 13,
    windTime: 1.6,
    chargeSpeed: 15,
    gasSpeed: 0.35,
    gasPushBack: 5,
    viewSpan: 20,
    aimLead: 3,
    shakeAmount: 1,
    hitStopMs: 35,
};
