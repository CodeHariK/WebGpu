// Combat Sandbox — drawing the grabber arms (Assembly Plant): the gantry rail overhead, the claw
// hanging from it on a cable, its shadow on the floor and the red reticle when it's coming down.
// Pure drawing: reads the Sim, changes nothing.
import { armLowered, clawHeight, LOWER_SECONDS } from './Map21Arms';
import type { Arm, Sim } from './Map21State';

const TAU = Math.PI * 2;
const RAIL_UP = 1.5; // how far "up" the rail is drawn (screen offset for height)
const CLAW_UP = 1.1;
const CLAW_SCALE = 1.6; // the claw at full height

/** Floor marks: the claw's shadow and, while it drops, the reticle. Drawn under everyone. */
export function drawArmShadows(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const arm of sim.arena.arms) {
        const h = clawHeight(arm);
        const { x } = arm;
        const y = arm.def.y;
        ctx.fillStyle = `rgba(0,0,0,${0.45 - h * 0.25})`;
        ctx.beginPath(); ctx.ellipse(x, y + 0.1, 0.5 + (1 - h) * 0.3, 0.3 + (1 - h) * 0.15, 0, 0, TAU); ctx.fill();
        if (arm.state === 'lower') {
            const k = Math.min(1, arm.t / LOWER_SECONDS);
            const pulse = Math.sin(sim.time * 30) > 0;
            ctx.strokeStyle = pulse ? 'rgba(248,113,113,0.95)' : 'rgba(248,113,113,0.5)';
            ctx.lineWidth = 0.1;
            ctx.beginPath(); ctx.arc(x, y, 1.7 - k * 0.8, 0, TAU); ctx.stroke();
            ctx.beginPath(); // cross hairs
            for (const a of [0, Math.PI / 2, Math.PI, Math.PI * 1.5]) {
                ctx.moveTo(x + Math.cos(a) * (1.9 - k * 0.8), y + Math.sin(a) * (1.9 - k * 0.8));
                ctx.lineTo(x + Math.cos(a) * (1.3 - k * 0.8), y + Math.sin(a) * (1.3 - k * 0.8));
            }
            ctx.stroke();
        }
    }
}

/** The rails, cables and claws: drawn over everyone (they hang above the line). */
export function drawArms(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    for (const arm of a.arms) drawRail(ctx, sim, arm);
    for (const arm of a.arms) drawClaw(ctx, sim, arm);
}

function drawRail(ctx: CanvasRenderingContext2D, sim: Sim, arm: Arm): void {
    const press = sim.arena.crushers[arm.def.press];
    const pressX = press.x + press.w / 2;
    const x0 = Math.min(arm.def.x0, pressX) - 0.6;
    const x1 = Math.max(arm.def.x1, pressX) + 0.6;
    const y = arm.def.y - RAIL_UP;
    ctx.fillStyle = 'rgba(0,0,0,0.25)'; // its shadow on the floor
    ctx.fillRect(x0, arm.def.y - 0.15, x1 - x0, 0.3);
    ctx.fillStyle = '#18181b'; // legs down to the floor at each end
    ctx.fillRect(x0 - 0.25, y - 0.3, 0.35, RAIL_UP + 0.3);
    ctx.fillRect(x1 - 0.1, y - 0.3, 0.35, RAIL_UP + 0.3);
    ctx.fillStyle = '#3f3f46';
    ctx.fillRect(x0, y - 0.28, x1 - x0, 0.56);
    ctx.fillStyle = '#52525b';
    ctx.fillRect(x0, y - 0.28, x1 - x0, 0.14);
    ctx.fillStyle = '#facc15';
    for (let x = x0; x < x1; x += 0.8) ctx.fillRect(x, y + 0.14, 0.4, 0.1);
}

function drawClaw(ctx: CanvasRenderingContext2D, sim: Sim, arm: Arm): void {
    const h = clawHeight(arm);
    const x = arm.x;
    const top = arm.def.y - RAIL_UP;
    const cy = arm.def.y - 0.25 - h * CLAW_UP; // the claw's body
    const off = arm.state === 'off';
    const jam = arm.state === 'jam';
    const white = arm.flash > 0;
    // Trolley on the rail and the cable down.
    ctx.fillStyle = off ? '#52525b' : '#a16207';
    ctx.fillRect(x - 0.45, top - 0.3, 0.9, 0.6);
    ctx.strokeStyle = '#27272a';
    ctx.lineWidth = 0.08;
    ctx.beginPath(); ctx.moveTo(x, top + 0.3); ctx.lineTo(x, cy - 0.3); ctx.stroke();
    ctx.save(); // the claw itself, drawn big enough to read
    ctx.translate(x, cy);
    ctx.scale(CLAW_SCALE, CLAW_SCALE);
    ctx.translate(-x, -cy);
    // Prongs: open while hunting, shut round what it holds.
    const closed = arm.state === 'carry';
    const spread = closed ? 0.25 : arm.state === 'lower' ? 0.55 + 0.1 * Math.sin(sim.time * 20) : 0.5;
    ctx.strokeStyle = white ? '#fff' : off ? '#71717a' : '#d4d4d8';
    ctx.lineWidth = 0.13;
    ctx.lineCap = 'round';
    for (const s of [-1, 1]) {
        ctx.beginPath();
        ctx.moveTo(x + s * 0.25, cy);
        ctx.lineTo(x + s * spread * 1.2, cy + 0.35);
        ctx.lineTo(x + s * spread * 0.7, cy + 0.65);
        ctx.stroke();
    }
    ctx.lineCap = 'butt';
    // Body.
    ctx.fillStyle = white ? '#fff' : off ? '#3f3f46' : jam ? '#78716c' : '#ca8a04';
    ctx.fillRect(x - 0.45, cy - 0.35, 0.9, 0.45);
    ctx.fillStyle = off || jam ? '#27272a' : armLowered(arm) || arm.state === 'carry' ? '#ef4444' : '#22c55e';
    ctx.beginPath(); ctx.arc(x, cy - 0.12, 0.1, 0, TAU); ctx.fill(); // status lamp
    if (jam) { // sparking
        ctx.strokeStyle = '#facc15';
        ctx.lineWidth = 0.05;
        const s = Math.floor(sim.time * 12);
        for (let i = 0; i < 3; i++) {
            const ang = ((s * 7 + i * 13) % 360) * (Math.PI / 180);
            ctx.beginPath(); ctx.moveTo(x, cy); ctx.lineTo(x + Math.cos(ang) * 0.6, cy + Math.sin(ang) * 0.6); ctx.stroke();
        }
    }
    ctx.restore();
    if (arm.who === 'player' && arm.state === 'carry') { // struggle pips above you
        for (let i = 0; i < 3; i++) {
            ctx.fillStyle = i < sim.player.struggle ? '#a5f3fc' : 'rgba(255,255,255,0.25)';
            ctx.beginPath(); ctx.arc(x - 0.4 + i * 0.4, cy - 1.0, 0.12, 0, TAU); ctx.fill();
        }
    }
}
