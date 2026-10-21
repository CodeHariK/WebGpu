// Combat Sandbox — drawing the Assembly Plant's mechs and drones: their bodies (called by
// Map21Render with the canvas moved to the toy) and their tells on the floor layer — the welder's
// flame cone, the sniper's laser, the turret's sweeping beam, the engineer's deploy bar, the
// magnet's pull and the projector's shield. Pure drawing.
import { CHARGE_TIME, PULL_RANGE } from './Map21Drones';
import { BEAM_RANGE, beamLength, DEPLOY_TIME, IGNITE_TIME, TORCH_HALF, TORCH_RANGE, TRACK_TIME } from './Map21Mechs';
import type { Enemy, Sim } from './Map21State';

const TAU = Math.PI * 2;

// --- tells (world layer, under the bodies) -------------------------------------------------------

export function drawMechTells(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const e of sim.enemies) {
        if (e.spawnT > 0 || e.held) continue;
        if (e.kind === 'welder') welderCone(ctx, sim, e);
        else if (e.kind === 'sniper') sniperLaser(ctx, sim, e);
        else if (e.kind === 'turret') turretBeam(ctx, sim, e);
        else if (e.kind === 'engineer' && e.state === 'deploy') deployBar(ctx, e);
        else if (e.kind === 'magnet') magnetPull(ctx, sim, e);
        else if (e.kind === 'projector' && e.state === 'shield') projectorBeam(ctx, sim, e);
    }
    for (const e of sim.enemies) if (e.shielded) shieldBubble(ctx, sim, e);
}

function cone(ctx: CanvasRenderingContext2D, e: Enemy, r: number): void {
    ctx.beginPath();
    ctx.moveTo(e.pos.x, e.pos.y);
    ctx.arc(e.pos.x, e.pos.y, r, e.facing - TORCH_HALF, e.facing + TORCH_HALF);
    ctx.closePath();
}

function welderCone(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    if (e.state === 'ignite') {
        const k = Math.min(1, e.t / IGNITE_TIME);
        ctx.strokeStyle = `rgba(251,146,60,${0.4 + 0.5 * k})`;
        ctx.lineWidth = 0.06;
        ctx.setLineDash([0.3, 0.2]);
        cone(ctx, e, TORCH_RANGE);
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.fillStyle = `rgba(251,146,60,${0.12 * k})`;
        ctx.fill();
    } else if (e.state === 'burn') {
        const g = ctx.createRadialGradient(e.pos.x, e.pos.y, 0.3, e.pos.x, e.pos.y, TORCH_RANGE);
        const flick = 0.85 + 0.15 * Math.sin(sim.time * 40);
        g.addColorStop(0, `rgba(254,240,138,${0.9 * flick})`);
        g.addColorStop(0.5, `rgba(251,146,60,${0.7 * flick})`);
        g.addColorStop(1, 'rgba(239,68,68,0.05)');
        ctx.fillStyle = g;
        cone(ctx, e, TORCH_RANGE);
        ctx.fill();
    }
}

function sniperLaser(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    if (e.state !== 'track' || !e.aim) return;
    const k = Math.min(1, e.t / TRACK_TIME);
    const far = { x: e.pos.x + (e.aim.x - e.pos.x) * 1.6, y: e.pos.y + (e.aim.y - e.pos.y) * 1.6 }; // runs on past the point
    ctx.strokeStyle = `rgba(239,68,68,${0.35 + 0.55 * k})`;
    ctx.lineWidth = 0.025 + k * k * 0.14;
    ctx.beginPath(); ctx.moveTo(e.pos.x, e.pos.y); ctx.lineTo(far.x, far.y); ctx.stroke();
    ctx.fillStyle = `rgba(254,202,202,${0.6 + 0.4 * Math.sin(sim.time * 30)})`;
    ctx.beginPath(); ctx.arc(e.aim.x, e.aim.y, 0.12 + k * 0.15, 0, TAU); ctx.fill(); // the dot on you (or just behind)
}

function turretBeam(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const len = e.state === 'sweep' ? beamLength(sim, e) : Math.min(BEAM_RANGE, Math.hypot(sim.player.pos.x - e.pos.x, sim.player.pos.y - e.pos.y));
    const lock = e.state === 'lock' || e.state === 'fire';
    ctx.strokeStyle = lock ? `rgba(239,68,68,${0.6 + 0.4 * Math.sin(sim.time * 40)})` : 'rgba(250,204,21,0.45)';
    ctx.lineWidth = lock ? 0.09 : 0.05;
    ctx.beginPath(); ctx.moveTo(e.pos.x, e.pos.y); ctx.lineTo(e.pos.x + Math.cos(e.facing) * len, e.pos.y + Math.sin(e.facing) * len); ctx.stroke();
}

function deployBar(ctx: CanvasRenderingContext2D, e: Enemy): void {
    const k = Math.min(1, e.t / DEPLOY_TIME);
    const at = { x: e.pos.x + Math.cos(e.facing) * 1.3, y: e.pos.y + Math.sin(e.facing) * 1.3 };
    ctx.strokeStyle = 'rgba(250,204,21,0.8)';
    ctx.lineWidth = 0.08;
    ctx.beginPath(); ctx.arc(at.x, at.y, 0.6, -Math.PI / 2, -Math.PI / 2 + TAU * k); ctx.stroke();
    ctx.setLineDash([0.15, 0.15]);
    ctx.lineWidth = 0.03;
    ctx.beginPath(); ctx.arc(at.x, at.y, 0.6, 0, TAU); ctx.stroke();
    ctx.setLineDash([]);
}

function magnetPull(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    if (e.state === 'charge') { // rings closing in
        const k = Math.min(1, e.t / CHARGE_TIME);
        ctx.strokeStyle = `rgba(147,197,253,${0.3 + 0.6 * k})`;
        ctx.lineWidth = 0.06;
        for (let i = 0; i < 3; i++) {
            const r = ((1 - k + i / 3) % 1) * 3 + 0.5;
            ctx.beginPath(); ctx.arc(e.pos.x, e.pos.y - 0.9, r, 0, TAU); ctx.stroke();
        }
    } else if (e.state === 'pull') { // field lines toward it from you
        const p = sim.player.pos;
        const d = Math.hypot(p.x - e.pos.x, p.y - e.pos.y);
        if (d > PULL_RANGE) return;
        ctx.strokeStyle = 'rgba(147,197,253,0.6)';
        ctx.lineWidth = 0.05;
        ctx.setLineDash([0.3, 0.25]);
        ctx.lineDashOffset = sim.time * 3;
        for (const off of [-0.35, 0, 0.35]) {
            ctx.beginPath(); ctx.moveTo(p.x + off, p.y); ctx.quadraticCurveTo((p.x + e.pos.x) / 2 + off * 2, (p.y + e.pos.y) / 2, e.pos.x, e.pos.y - 0.9); ctx.stroke();
        }
        ctx.setLineDash([]);
        ctx.lineDashOffset = 0;
    }
}

function projectorBeam(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const ward = sim.enemies.find((o) => o.id === e.owner);
    if (!ward) return;
    ctx.strokeStyle = `rgba(125,211,252,${0.5 + 0.3 * Math.sin(sim.time * 12)})`;
    ctx.lineWidth = 0.07;
    ctx.beginPath(); ctx.moveTo(e.pos.x, e.pos.y - 0.9); ctx.lineTo(ward.pos.x, ward.pos.y); ctx.stroke();
}

function shieldBubble(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const r = 1.0 + Math.sin(sim.time * 5 + e.id) * 0.04;
    ctx.fillStyle = 'rgba(125,211,252,0.13)';
    ctx.beginPath(); ctx.arc(e.pos.x, e.pos.y, r, 0, TAU); ctx.fill();
    ctx.strokeStyle = 'rgba(186,230,253,0.75)';
    ctx.lineWidth = 0.06;
    ctx.stroke();
    ctx.strokeStyle = 'rgba(186,230,253,0.3)';
    ctx.lineWidth = 0.03;
    for (let i = 0; i < 6; i++) { // hex cells
        const a = sim.time * 0.4 + (i * TAU) / 6;
        ctx.beginPath(); ctx.arc(e.pos.x + Math.cos(a) * r * 0.55, e.pos.y + Math.sin(a) * r * 0.55, r * 0.38, 0, TAU); ctx.stroke();
    }
}

// --- bodies (canvas already at the toy) --------------------------------------------------------

const fill = (ctx: CanvasRenderingContext2D, white: boolean, c: string) => { ctx.fillStyle = white ? '#fff' : c; };

// The welder: a wide armoured walker, torch arm forward, a glowing tank on its exposed back.
export function drawWelder(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    const cool = e.state === 'cool';
    fill(ctx, white, '#3f3f46'); // legs
    const step = e.state === 'walk' ? Math.sin(sim.time * 8 + e.id) * 0.12 : 0;
    ctx.fillRect(-r * 0.5 + step, -r * 0.95, r * 0.6, r * 0.35);
    ctx.fillRect(-r * 0.5 - step, r * 0.6, r * 0.6, r * 0.35);
    fill(ctx, white, '#7c2d12'); // armoured front shell
    ctx.beginPath(); ctx.arc(0, 0, r, -Math.PI / 2, Math.PI / 2); ctx.fill();
    fill(ctx, white, cool ? '#fb923c' : '#57534e'); // back: the weak spot (glows while it cools)
    ctx.beginPath(); ctx.arc(0, 0, r * 0.85, Math.PI / 2, Math.PI * 1.5); ctx.fill();
    fill(ctx, white, cool ? '#fde047' : '#f97316'); // fuel tank
    ctx.fillRect(-r * 0.95, -r * 0.3, r * 0.4, r * 0.6);
    fill(ctx, white, '#a8a29e'); // torch arm
    ctx.fillRect(r * 0.4, r * 0.25, r * 0.9, 0.16);
    const lit = e.state === 'ignite' || e.state === 'burn';
    ctx.fillStyle = lit ? (Math.sin(sim.time * 40) > 0 ? '#fef08a' : '#fb923c') : '#44403c';
    ctx.beginPath(); ctx.arc(r * 1.35, r * 0.33, 0.12, 0, TAU); ctx.fill();
    ctx.fillStyle = '#facc15'; // visor
    ctx.fillRect(r * 0.55, -r * 0.35, 0.1, r * 0.5);
}

// The sniper: a lean bot with a long barrel and a single red lens.
export function drawSniper(ctx: CanvasRenderingContext2D, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    fill(ctx, white, '#1f2937');
    ctx.fillRect(0, -0.07, r + 1.0, 0.14); // barrel
    fill(ctx, white, '#4b5563');
    ctx.beginPath(); ctx.ellipse(0, 0, r, r * 0.75, 0, 0, TAU); ctx.fill();
    fill(ctx, white, '#374151');
    ctx.fillRect(-r * 0.9, -r * 0.4, r * 0.4, r * 0.8); // backpack
    const k = e.state === 'track' ? Math.min(1, e.t / TRACK_TIME) : 0;
    ctx.fillStyle = `rgb(${150 + 105 * k},40,40)`;
    ctx.beginPath(); ctx.arc(r * 0.45, -r * 0.2, 0.1 + 0.05 * k, 0, TAU); ctx.fill();
}

// The engineer: a squat bot carrying a folded turret on its back; a wrench arm.
export function drawEngineer(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    fill(ctx, white, '#a16207');
    ctx.beginPath(); ctx.arc(0, 0, r, 0, TAU); ctx.fill();
    fill(ctx, white, '#facc15');
    ctx.fillRect(-r * 0.6, -r * 0.6, r * 0.25, r * 1.2); // stripe
    fill(ctx, white, '#52525b'); // the folded turret on its back
    ctx.fillRect(-r * 1.15, -r * 0.45, r * 0.55, r * 0.9);
    const working = e.state === 'deploy';
    ctx.save();
    ctx.translate(r * 0.6, r * 0.45);
    ctx.rotate(working ? Math.sin(sim.time * 25) * 0.6 : 0.3);
    fill(ctx, white, '#d4d4d8');
    ctx.fillRect(0, -0.05, r * 0.8, 0.1);
    ctx.restore();
    ctx.fillStyle = '#0ea5e9';
    ctx.fillRect(r * 0.5, -r * 0.3, 0.1, r * 0.4);
}

// A turret: a round base with a swivel head and a barrel.
export function drawTurret(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    fill(ctx, white, '#3f3f46');
    for (let i = 0; i < 3; i++) { // tripod feet
        const a = (i * TAU) / 3 + 0.3;
        ctx.fillRect(Math.cos(a) * r * 0.9 - 0.08, Math.sin(a) * r * 0.9 - 0.08, 0.16, 0.16);
    }
    fill(ctx, white, '#52525b');
    ctx.beginPath(); ctx.arc(0, 0, r * 0.8, 0, TAU); ctx.fill();
    ctx.rotate(e.facing);
    fill(ctx, white, '#27272a');
    ctx.fillRect(0, -0.1, r + 0.35, 0.2);
    fill(ctx, white, '#71717a');
    ctx.beginPath(); ctx.arc(0, 0, r * 0.5, 0, TAU); ctx.fill();
    const lock = e.state === 'lock' || e.state === 'fire';
    ctx.fillStyle = lock ? (Math.sin(sim.time * 40) > 0 ? '#ef4444' : '#7f1d1d') : '#facc15';
    ctx.beginPath(); ctx.arc(r * 0.2, 0, 0.1, 0, TAU); ctx.fill();
}

// The magnet drone: a horseshoe magnet on rotors; the poles glow blue as it charges.
export function drawMagnet(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    const hot = e.state === 'charge' || e.state === 'pull';
    ctx.rotate(e.facing - Math.PI / 2);
    ctx.strokeStyle = white ? '#fff' : '#dc2626';
    ctx.lineWidth = 0.22;
    ctx.beginPath(); ctx.arc(0, -r * 0.2, r * 0.7, Math.PI, 0, true); ctx.stroke();
    ctx.fillStyle = hot ? `rgba(147,197,253,${0.6 + 0.4 * Math.sin(sim.time * 30)})` : '#e5e7eb';
    ctx.fillRect(-r * 0.85, -r * 0.3, 0.3, 0.3);
    ctx.fillRect(r * 0.55, -r * 0.3, 0.3, 0.3);
    ctx.strokeStyle = 'rgba(226,232,240,0.55)';
    ctx.lineWidth = 0.05;
    const a = sim.time * 35;
    ctx.beginPath(); ctx.moveTo(Math.cos(a) * 0.6, r * 0.6 + Math.sin(a) * 0.2); ctx.lineTo(-Math.cos(a) * 0.6, r * 0.6 - Math.sin(a) * 0.2); ctx.stroke();
}

// The projector: a small round drone with a dish on top.
export function drawProjector(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    fill(ctx, white, '#0e7490');
    ctx.beginPath(); ctx.arc(0, 0, r, 0, TAU); ctx.fill();
    ctx.rotate(e.facing);
    fill(ctx, white, '#a5f3fc');
    ctx.beginPath(); ctx.arc(r * 0.3, 0, r * 0.55, -1.1, 1.1); ctx.fill();
    ctx.fillStyle = e.state === 'shield' ? '#ecfeff' : '#155e75';
    ctx.beginPath(); ctx.arc(r * 0.45, 0, 0.09, 0, TAU); ctx.fill();
    ctx.strokeStyle = 'rgba(226,232,240,0.5)';
    ctx.lineWidth = 0.05;
    const a = sim.time * 40;
    ctx.beginPath(); ctx.moveTo(-Math.cos(a) * 0.55, Math.sin(a) * 0.55); ctx.lineTo(Math.cos(a) * 0.55, -Math.sin(a) * 0.55); ctx.stroke();
}
