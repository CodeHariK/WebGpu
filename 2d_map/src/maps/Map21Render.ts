// Combat Sandbox — drawing (canvas 2D, top-down, cute shapes over a grimy factory). Pure drawing:
// reads the Sim, changes nothing. The place is drawn by Map21RenderWorld; this file draws the
// toys, the player, shots, bombs, effects and the HUD, and moves the camera (shake, kick).
import { BOMB_RADIUS, bombPos } from './Map21Blast';
import { flies, RADIUS } from './Map21Enemies';
import { BANNER_LIFE } from './Map21Juice';
import { invincible, PLAYER_RADIUS } from './Map21Player';
import { drawBelts, drawCore, drawCover, drawCrushers, drawFloor, drawGas, drawLamps, drawLifts, drawPipes, drawPools, drawVents, drawWalls } from './Map21RenderWorld';
import { drawCoreShield, drawDoors, drawExit, drawHudKey, drawObjectives, drawPickups, drawSupplies } from './Map21RenderObjectives';
import { KEY_COLOR, nextTargets } from './Map21Objectives';
import { drawArms, drawArmShadows } from './Map21RenderArms';
import { drawCables, drawLandings, drawRigbot, drawRigger, drawSpringer } from './Map21RenderBots';
import { drawEngineer, drawMagnet, drawMechTells, drawProjector, drawSniper, drawTurret, drawWelder } from './Map21RenderMechs';
import { hopHeight } from './Map21Bots';
import { drawInnerWalls, drawPainted, drawSprayers, drawStamps, drawStations } from './Map21RenderStations';
import type { Enemy, Sim, Vec } from './Map21State';
import { THEMES } from './Map21Themes';

const C = {
    player: '#f9a8d4', face: '#fff7ed', nose: '#ef4444',
    tin: '#8a8f6a', tinDark: '#5b5f45', visor: '#ef4444',
    riot: '#64748b', shield: '#cbd5e1',
    drone: '#6d5f8a', diver: '#7c6f9b', grenadier: '#7a6a4f', mouse: '#9ca3af',
    bubble: 'rgba(165,243,252,0.85)',
    joy: '#fde047',
};

export function drawSim(ctx: CanvasRenderingContext2D, sim: Sim, mouse: Vec): void {
    const v = sim.view;
    const k = ctx.canvas.width / v.span; // pixels per metre
    const shake = sim.juice.shake * sim.juice.shake;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    const sx = (Math.random() - 0.5) * 2 * shake * 0.6 + sim.juice.kick.x;
    const sy = (Math.random() - 0.5) * 2 * shake * 0.6 + sim.juice.kick.y;
    ctx.setTransform(k, 0, 0, k, (sx - (v.x - v.span / 2)) * k, (sy - (v.y - v.span / 2)) * k);
    ctx.rotate(shake * 0.012 * (Math.random() - 0.5));

    drawFloor(ctx, sim);
    drawBelts(ctx, sim);
    drawStations(ctx, sim);
    drawPools(ctx, sim);
    drawArmShadows(ctx, sim);
    drawVents(ctx, sim);
    drawLifts(ctx, sim);
    drawWalls(ctx, sim);
    drawExit(ctx, sim);
    drawPipes(ctx, sim);
    for (const e of sim.enemies) drawShadow(ctx, e.pos, RADIUS[e.kind] * (airborne(e) ? 0.7 : 1), airborne(e) ? 0.9 : 0.15);
    drawShadow(ctx, sim.player.pos, PLAYER_RADIUS, 0.12);
    drawCore(ctx, sim);
    drawCoreShield(ctx, sim);
    drawObjectives(ctx, sim);
    drawSupplies(ctx, sim);
    drawCover(ctx, sim);
    drawInnerWalls(ctx, sim);
    drawDoors(ctx, sim);
    drawTells(ctx, sim);
    drawLandings(ctx, sim);
    drawCables(ctx, sim);
    drawMechTells(ctx, sim);
    drawPickups(ctx, sim);
    for (const sp of sim.spirits) drawSpirit(ctx, sp.pos, sim.time);
    for (const e of sim.enemies) drawEnemy(ctx, sim, e);
    drawPlayer(ctx, sim);
    drawPainted(ctx, sim);
    drawSprayers(ctx, sim);
    drawCrushers(ctx, sim);
    drawStamps(ctx, sim);
    drawArms(ctx, sim);
    drawShots(ctx, sim);
    drawBombs(ctx, sim);
    drawParticles(ctx, sim);
    drawLamps(ctx, sim);
    drawGas(ctx, sim);
    drawFloaters(ctx, sim);
    drawCrosshair(ctx, mouse);

    ctx.setTransform(1, 0, 0, 1, 0, 0);
    drawVignette(ctx);
    drawOffscreen(ctx, sim, k);
    drawHud(ctx, sim);
}

const airborne = (e: Enemy) => flies(e);

function drawShadow(ctx: CanvasRenderingContext2D, p: Vec, r: number, drop: number): void {
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.beginPath(); ctx.ellipse(p.x + drop * 0.5, p.y + drop + 0.15, r * 1.05, r * 0.6, 0, 0, Math.PI * 2); ctx.fill();
}

// Warnings: sight lines, charge lanes, dive lines, arrival circles, the spotter's light.
function drawTells(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player.pos;
    for (const e of sim.enemies) {
        if (e.held) continue; // in a claw: no threat
        if (e.spawnT > 0) {
            const k = 1 - e.spawnT / 0.8;
            ctx.strokeStyle = `rgba(248,113,113,${0.4 + k * 0.5})`;
            ctx.lineWidth = 0.08;
            ctx.beginPath(); ctx.arc(e.pos.x, e.pos.y, RADIUS[e.kind] * (2 - k), 0, Math.PI * 2); ctx.stroke();
            continue;
        }
        if (e.kind === 'rifleman' && e.state === 'aim' && e.t > 0.05) {
            const k = Math.min(1, e.t / (sim.settings.aimTell * (sim.marked ? 0.5 : 1)));
            ctx.strokeStyle = `rgba(248,113,113,${0.2 + k * 0.7})`;
            ctx.lineWidth = 0.03 + k * 0.08;
            ctx.setLineDash([0.3, 0.2]);
            ctx.beginPath(); ctx.moveTo(e.pos.x, e.pos.y); ctx.lineTo(p.x, p.y); ctx.stroke();
            ctx.setLineDash([]);
        }
        if (e.kind === 'riot' && e.state === 'windup') {
            const k = Math.min(1, e.t / 0.7);
            ctx.fillStyle = `rgba(248,113,113,${0.15 + k * 0.25})`;
            ctx.save();
            ctx.translate(e.pos.x, e.pos.y);
            ctx.rotate(e.facing);
            ctx.fillRect(0, -RADIUS.riot, sim.settings.chargeSpeed * 0.8 * k, RADIUS.riot * 2);
            ctx.restore();
        }
        if (e.kind === 'diver' && e.state === 'lock') {
            const k = Math.min(1, e.t / 0.6);
            ctx.strokeStyle = `rgba(248,113,113,${0.3 + 0.6 * k})`;
            ctx.lineWidth = 0.06 + k * 0.1;
            ctx.beginPath(); ctx.moveTo(e.pos.x, e.pos.y); ctx.lineTo(e.pos.x + Math.cos(e.facing) * 17, e.pos.y + Math.sin(e.facing) * 17); ctx.stroke();
        }
        if (e.kind === 'spotter' && e.state === 'spot') {
            const a = Math.atan2(p.y - e.pos.y, p.x - e.pos.x);
            const d = Math.hypot(p.x - e.pos.x, p.y - e.pos.y);
            const g = ctx.createLinearGradient(e.pos.x, e.pos.y, p.x, p.y);
            g.addColorStop(0, 'rgba(253,224,71,0.45)');
            g.addColorStop(1, 'rgba(253,224,71,0.12)');
            ctx.fillStyle = g;
            ctx.beginPath();
            ctx.moveTo(e.pos.x, e.pos.y);
            ctx.lineTo(e.pos.x + Math.cos(a - 0.12) * (d + 1), e.pos.y + Math.sin(a - 0.12) * (d + 1));
            ctx.lineTo(e.pos.x + Math.cos(a + 0.12) * (d + 1), e.pos.y + Math.sin(a + 0.12) * (d + 1));
            ctx.fill();
        }
    }
}

function drawEnemy(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const r = RADIUS[e.kind];
    const lift = e.spawnT > 0 ? e.spawnT * 6 : 0; // arriving from above
    const hover = e.kind === 'spotter' || e.kind === 'rigger' || e.kind === 'magnet' || e.kind === 'projector' ? 0.9 + Math.sin(sim.time * 4 + e.id) * 0.1 : e.kind === 'springer' ? hopHeight(e) * 1.6 : airborne(e) ? 0.7 : 0;
    const y = e.pos.y - lift - hover;
    const white = e.flash > 0;
    ctx.save();
    ctx.translate(e.pos.x, y);
    switch (e.kind) {
        case 'rifleman': drawRifleman(ctx, sim, e, r, white); break;
        case 'spotter': drawSpotter(ctx, sim, e, r, white); break;
        case 'riot': drawRiot(ctx, sim, e, r, white); break;
        case 'diver': drawDiver(ctx, sim, e, r, white); break;
        case 'grenadier': drawGrenadier(ctx, sim, e, r, white); break;
        case 'mouse': drawMouse(ctx, sim, e, r, white); break;
        case 'rigger': drawRigger(ctx, sim, e, r, white); break;
        case 'rigbot': drawRigbot(ctx, sim, e, r, white); break;
        case 'springer': drawSpringer(ctx, sim, e, r, white); break;
        case 'welder': drawWelder(ctx, sim, e, r, white); break;
        case 'sniper': drawSniper(ctx, e, r, white); break;
        case 'engineer': drawEngineer(ctx, sim, e, r, white); break;
        case 'turret': drawTurret(ctx, sim, e, r, white); break;
        case 'magnet': drawMagnet(ctx, sim, e, r, white); break;
        case 'projector': drawProjector(ctx, sim, e, r, white); break;
    }
    ctx.restore();
    if (e.hp < e.maxHp && e.spawnT <= 0 && e.kind !== 'mouse') { // health bar
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fillRect(e.pos.x - 0.5, y - r - 0.45, 1, 0.12);
        ctx.fillStyle = '#f87171';
        ctx.fillRect(e.pos.x - 0.5, y - r - 0.45, Math.max(0, e.hp / e.maxHp), 0.12);
    }
}

function body(ctx: CanvasRenderingContext2D, r: number, color: string, white: boolean): void {
    ctx.fillStyle = white ? '#fff' : color;
    ctx.beginPath(); ctx.arc(0, 0, r, 0, Math.PI * 2); ctx.fill();
}

function drawRifleman(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    ctx.fillStyle = white ? '#fff' : '#334155'; // rifle
    ctx.fillRect(0.1, 0.12, r + 0.45, 0.14);
    body(ctx, r, C.tin, white);
    body(ctx, r * 0.6, C.tinDark, white);
    ctx.fillStyle = C.visor;
    ctx.fillRect(r * 0.45, -0.18, 0.12, 0.36);
    const winding = e.state === 'wind';
    ctx.save();
    ctx.translate(-r - 0.12, 0);
    ctx.rotate(winding ? sim.time * 18 : 0);
    ctx.fillStyle = winding ? '#fde047' : '#ca8a04';
    ctx.fillRect(-0.06, -0.28, 0.12, 0.56);
    ctx.restore();
    if (winding) {
        ctx.rotate(-e.facing);
        ctx.strokeStyle = `rgba(253,224,71,${0.5 + 0.5 * Math.sin(sim.time * 20)})`;
        ctx.lineWidth = 0.08;
        ctx.beginPath(); ctx.arc(0, 0, r + 0.25, 0, Math.PI * 2); ctx.stroke();
    }
}

function drawSpotter(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    body(ctx, r, C.drone, white);
    rotor(ctx, sim.time * 30, 0.8);
    ctx.fillStyle = e.state === 'spot' ? '#fde047' : '#78716c';
    ctx.beginPath(); ctx.arc(Math.cos(e.facing) * r * 0.5, Math.sin(e.facing) * r * 0.5, 0.16, 0, Math.PI * 2); ctx.fill();
}

function rotor(ctx: CanvasRenderingContext2D, a: number, len: number): void {
    ctx.strokeStyle = 'rgba(226,232,240,0.6)';
    ctx.lineWidth = 0.06;
    ctx.beginPath(); ctx.moveTo(Math.cos(a) * len, Math.sin(a) * len); ctx.lineTo(-Math.cos(a) * len, -Math.sin(a) * len); ctx.stroke();
}

function drawRiot(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    const glow = e.state === 'windup' ? 0.5 + 0.5 * Math.sin(sim.time * 30) : 0;
    body(ctx, r, glow > 0 ? `rgb(${100 + glow * 140},${116 - glow * 40},${139 - glow * 60})` : C.riot, white);
    ctx.fillStyle = C.visor;
    ctx.beginPath(); ctx.arc(Math.cos(e.facing) * r * 0.45, Math.sin(e.facing) * r * 0.45, 0.14, 0, Math.PI * 2); ctx.fill();
    if (e.state !== 'stunned') {
        ctx.strokeStyle = white ? '#fff' : C.shield;
        ctx.lineWidth = 0.22;
        ctx.beginPath(); ctx.arc(0, 0, r + 0.18, e.facing - 1.2, e.facing + 1.2); ctx.stroke();
    } else stars(ctx, sim, r);
}

function stars(ctx: CanvasRenderingContext2D, sim: Sim, r: number): void {
    for (let i = 0; i < 3; i++) {
        const a = sim.time * 5 + (i * Math.PI * 2) / 3;
        ctx.fillStyle = '#fde047';
        ctx.beginPath(); ctx.arc(Math.cos(a) * 0.5, -r - 0.2 + Math.sin(a) * 0.15, 0.1, 0, Math.PI * 2); ctx.fill();
    }
}

// The bolt drone: a dart shape that points where it's going; stuck in the floor it tilts and sparks.
function drawDiver(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    ctx.fillStyle = white ? '#fff' : C.diver;
    ctx.beginPath(); ctx.moveTo(r + 0.3, 0); ctx.lineTo(-r, -r * 0.8); ctx.lineTo(-r * 0.5, 0); ctx.lineTo(-r, r * 0.8); ctx.fill();
    ctx.fillStyle = e.state === 'lock' || e.state === 'dive' ? '#ef4444' : '#a78bfa';
    ctx.beginPath(); ctx.arc(r * 0.2, 0, 0.12, 0, Math.PI * 2); ctx.fill();
    if (e.state === 'stuck') {
        ctx.rotate(-e.facing);
        stars(ctx, sim, r);
    } else rotor(ctx, sim.time * 40, 0.6);
}

// The grenadier: a stout toy with a bomb hopper on its back; the arm glows before a throw.
function drawGrenadier(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    ctx.fillStyle = white ? '#fff' : '#3f3f46'; // hopper
    ctx.fillRect(-r - 0.25, -0.3, 0.35, 0.6);
    body(ctx, r, C.grenadier, white);
    ctx.fillStyle = C.visor;
    ctx.fillRect(r * 0.45, -0.15, 0.1, 0.3);
    const winding = e.state === 'throw';
    ctx.fillStyle = winding ? `rgb(251,${146 + Math.sin(sim.time * 30) * 60},60)` : '#57534e'; // the throwing arm
    ctx.fillRect(0, r * 0.6, r * (winding ? 0.4 : 0.9), 0.18);
}

// A wind-up mouse: tiny, fast, with a key on its back that spins as it runs.
function drawMouse(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    ctx.rotate(e.facing);
    ctx.fillStyle = white ? '#fff' : C.mouse;
    ctx.beginPath(); ctx.ellipse(0, 0, r * 1.3, r, 0, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#f9a8d4';
    ctx.beginPath(); ctx.arc(r * 0.7, -r * 0.7, 0.1, 0, Math.PI * 2); ctx.arc(r * 0.7, r * 0.7, 0.1, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#ef4444'; // the fuse light: it's a bomb
    ctx.beginPath(); ctx.arc(r * 1.2, 0, 0.07 + 0.03 * Math.sin(sim.time * 25), 0, Math.PI * 2); ctx.fill();
    ctx.save();
    ctx.translate(-r * 1.2, 0);
    ctx.rotate(sim.time * 20);
    ctx.fillStyle = '#ca8a04';
    ctx.fillRect(-0.03, -0.14, 0.06, 0.28);
    ctx.restore();
}

function drawPlayer(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    if (p.hurtT > 0 && Math.floor(p.hurtT * 20) % 2 === 0) return;
    const r = PLAYER_RADIUS;
    const squash = p.dashT > 0 ? 1.35 : 1 + p.recoil * 0.08;
    ctx.save();
    ctx.translate(p.pos.x, p.pos.y);
    ctx.rotate(p.aim);
    ctx.fillStyle = '#e2e8f0';
    ctx.fillRect(0.2, -0.05, r + 0.35 - p.recoil * 0.15, 0.1);
    ctx.strokeStyle = C.bubble;
    ctx.lineWidth = 0.05;
    ctx.beginPath(); ctx.arc(r + 0.6 - p.recoil * 0.15, 0, 0.14, 0, Math.PI * 2); ctx.stroke();
    ctx.scale(squash, 1 / squash);
    body(ctx, r, C.player, false);
    ctx.fillStyle = C.face;
    ctx.beginPath(); ctx.arc(0.08, 0, r * 0.62, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = C.nose;
    ctx.beginPath(); ctx.arc(r * 0.55, 0, 0.11, 0, Math.PI * 2); ctx.fill();
    ctx.restore();
    if (invincible(p) && p.iframes > 0) {
        ctx.strokeStyle = 'rgba(255,255,255,0.9)';
        ctx.lineWidth = 0.08;
        ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, r + 0.2, 0, Math.PI * 2); ctx.stroke();
    }
    if (sim.marked) {
        ctx.strokeStyle = `rgba(253,224,71,${0.5 + 0.4 * Math.sin(sim.time * 12)})`;
        ctx.lineWidth = 0.1;
        ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, r + 0.45, 0, Math.PI * 2); ctx.stroke();
    }
}

function drawShots(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const s of sim.shots) {
        if (s.big) { // a fizz bubble: fat, purple, wobbling
            const wob = 1 + Math.sin(sim.time * 30) * 0.08;
            ctx.fillStyle = 'rgba(196,181,253,0.4)';
            ctx.beginPath(); ctx.ellipse(s.pos.x, s.pos.y, s.radius * wob, s.radius / wob, 0, 0, Math.PI * 2); ctx.fill();
            ctx.strokeStyle = '#c4b5fd';
            ctx.lineWidth = 0.07;
            ctx.stroke();
            ctx.fillStyle = 'rgba(255,255,255,0.9)';
            ctx.beginPath(); ctx.arc(s.pos.x - s.radius * 0.35, s.pos.y - s.radius * 0.35, s.radius * 0.22, 0, Math.PI * 2); ctx.fill();
        } else if (s.friendly) {
            ctx.fillStyle = 'rgba(165,243,252,0.35)';
            ctx.beginPath(); ctx.arc(s.pos.x, s.pos.y, s.radius, 0, Math.PI * 2); ctx.fill();
            ctx.strokeStyle = C.bubble;
            ctx.lineWidth = 0.05;
            ctx.stroke();
            ctx.fillStyle = 'rgba(255,255,255,0.9)';
            ctx.beginPath(); ctx.arc(s.pos.x - s.radius * 0.35, s.pos.y - s.radius * 0.35, s.radius * 0.25, 0, Math.PI * 2); ctx.fill();
        } else {
            ctx.strokeStyle = 'rgba(251,146,60,0.55)';
            ctx.lineWidth = s.radius * 1.6;
            ctx.lineCap = 'round';
            ctx.beginPath(); ctx.moveTo(s.pos.x, s.pos.y); ctx.lineTo(s.pos.x - s.vel.x * 0.05, s.pos.y - s.vel.y * 0.05); ctx.stroke();
            ctx.fillStyle = '#fff7ed';
            ctx.beginPath(); ctx.arc(s.pos.x, s.pos.y, s.radius * 0.7, 0, Math.PI * 2); ctx.fill();
            ctx.lineCap = 'butt';
        }
    }
}

// Bombs in the air: a growing red circle where it will land, a shadow, and the bomb up above.
function drawBombs(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const b of sim.bombs) {
        const k = Math.min(1, b.t / b.flight);
        ctx.strokeStyle = b.batted ? `rgba(165,243,252,${0.4 + 0.5 * k})` : `rgba(248,113,113,${0.3 + 0.6 * k})`;
        ctx.lineWidth = 0.08;
        ctx.beginPath(); ctx.arc(b.to.x, b.to.y, BOMB_RADIUS * k, 0, Math.PI * 2); ctx.stroke();
        ctx.beginPath(); ctx.arc(b.to.x, b.to.y, BOMB_RADIUS, 0, Math.PI * 2); ctx.setLineDash([0.2, 0.2]); ctx.stroke(); ctx.setLineDash([]);
        const { pos, height } = bombPos(b);
        ctx.fillStyle = 'rgba(0,0,0,0.35)';
        ctx.beginPath(); ctx.ellipse(pos.x, pos.y + 0.1, 0.3, 0.18, 0, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = '#292524';
        ctx.beginPath(); ctx.arc(pos.x, pos.y - height, 0.28, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = Math.sin(sim.time * 30) > 0 ? '#fde047' : '#f97316';
        ctx.beginPath(); ctx.arc(pos.x + 0.15, pos.y - height - 0.25, 0.08, 0, Math.PI * 2); ctx.fill();
    }
}

function drawSpirit(ctx: CanvasRenderingContext2D, p: Vec, t: number): void {
    const bob = Math.sin(t * 10 + p.x) * 0.05;
    ctx.fillStyle = 'rgba(253,224,71,0.3)';
    ctx.beginPath(); ctx.arc(p.x, p.y + bob, 0.32, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = C.joy;
    ctx.beginPath(); ctx.arc(p.x, p.y + bob, 0.17, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#422006';
    ctx.fillRect(p.x - 0.07, p.y + bob - 0.05, 0.035, 0.035);
    ctx.fillRect(p.x + 0.035, p.y + bob - 0.05, 0.035, 0.035);
}

function drawParticles(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const p of sim.particles) {
        const k = p.life / p.maxLife;
        ctx.globalAlpha = Math.min(1, k * 1.5);
        if (p.kind === 'ring') {
            ctx.strokeStyle = p.color;
            ctx.lineWidth = 0.08 * k + 0.02;
            ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, p.size * (1 - k * k), 0, Math.PI * 2); ctx.stroke();
        } else if (p.kind === 'spark') {
            ctx.strokeStyle = p.color;
            ctx.lineWidth = p.size * 0.6;
            ctx.beginPath(); ctx.moveTo(p.pos.x, p.pos.y); ctx.lineTo(p.pos.x - p.vel.x * 0.04, p.pos.y - p.vel.y * 0.04); ctx.stroke();
        } else if (p.kind === 'square') {
            ctx.save();
            ctx.translate(p.pos.x, p.pos.y);
            ctx.rotate(p.spin * (1 - k));
            ctx.fillStyle = p.color;
            ctx.fillRect(-p.size / 2, -p.size / 2, p.size, p.size);
            ctx.restore();
        } else if (p.kind === 'smoke') {
            ctx.globalAlpha = k * 0.6;
            ctx.fillStyle = p.color;
            ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, p.size * (1.6 - k), 0, Math.PI * 2); ctx.fill();
        } else {
            ctx.fillStyle = p.color;
            ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, p.size * k, 0, Math.PI * 2); ctx.fill();
        }
    }
    ctx.globalAlpha = 1;
}

function drawFloaters(ctx: CanvasRenderingContext2D, sim: Sim): void {
    ctx.textAlign = 'center';
    for (const f of sim.floaters) {
        const k = f.life / 0.8;
        ctx.globalAlpha = Math.min(1, k * 2);
        const size = 0.55 * f.size * (1 + (1 - k) * 0.2 + (k > 0.85 ? (k - 0.85) * 3 : 0));
        ctx.font = `800 ${size}px Inter, sans-serif`;
        ctx.lineWidth = size * 0.18;
        ctx.strokeStyle = 'rgba(15,23,42,0.85)';
        ctx.strokeText(f.text, f.pos.x, f.pos.y);
        ctx.fillStyle = f.color;
        ctx.fillText(f.text, f.pos.x, f.pos.y);
    }
    ctx.globalAlpha = 1;
}

function drawCrosshair(ctx: CanvasRenderingContext2D, m: Vec): void {
    ctx.strokeStyle = 'rgba(255,255,255,0.7)';
    ctx.lineWidth = 0.05;
    ctx.beginPath(); ctx.arc(m.x, m.y, 0.3, 0, Math.PI * 2); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(m.x - 0.5, m.y); ctx.lineTo(m.x - 0.15, m.y); ctx.moveTo(m.x + 0.15, m.y); ctx.lineTo(m.x + 0.5, m.y); ctx.stroke();
}

function drawVignette(ctx: CanvasRenderingContext2D): void {
    const w = ctx.canvas.width;
    const g = ctx.createRadialGradient(w / 2, w / 2, w * 0.35, w / 2, w / 2, w * 0.75);
    g.addColorStop(0, 'rgba(0,0,0,0)');
    g.addColorStop(1, 'rgba(0,0,0,0.55)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, w);
}

// Arrows at the screen edge for toys outside the view (red when one is about to attack), and big
// ones for where to go: the machines left to break (amber) or the open exit (green).
function drawOffscreen(ctx: CanvasRenderingContext2D, sim: Sim, k: number): void {
    const v = sim.view;
    const w = ctx.canvas.width;
    const margin = 18;
    for (const target of nextTargets(sim)) {
        const sx = (target.x - (v.x - v.span / 2)) * k;
        const sy = (target.y - (v.y - v.span / 2)) * k;
        if (sx > 0 && sy > 0 && sx < w && sy < w) continue;
        const cx = w / 2;
        const a = Math.atan2(sy - cx, sx - cx);
        const t = Math.min((cx - 34) / Math.abs(Math.cos(a) || 1e-6), (cx - 34) / Math.abs(Math.sin(a) || 1e-6));
        ctx.save();
        ctx.translate(cx + Math.cos(a) * t, cx + Math.sin(a) * t);
        ctx.rotate(a);
        ctx.fillStyle = sim.arena.exitOpen ? '#4ade80' : '#fbbf24';
        ctx.globalAlpha = 0.7 + 0.3 * Math.sin(sim.time * 6);
        ctx.beginPath(); ctx.moveTo(18, 0); ctx.lineTo(-8, -13); ctx.lineTo(-3, 0); ctx.lineTo(-8, 13); ctx.fill();
        ctx.restore();
    }
    for (const e of sim.enemies) {
        const sx = (e.pos.x - (v.x - v.span / 2)) * k;
        const sy = (e.pos.y - (v.y - v.span / 2)) * k;
        if (sx > 0 && sy > 0 && sx < w && sy < w) continue;
        const cx = w / 2;
        const a = Math.atan2(sy - cx, sx - cx);
        const t = Math.min((cx - margin) / Math.abs(Math.cos(a) || 1e-6), (cx - margin) / Math.abs(Math.sin(a) || 1e-6));
        const danger = (e.kind === 'rifleman' && (e.state === 'aim' || e.state === 'burst')) || (e.kind === 'riot' && (e.state === 'windup' || e.state === 'charge'))
            || (e.kind === 'diver' && (e.state === 'lock' || e.state === 'dive')) || (e.kind === 'grenadier' && e.state === 'throw') || e.kind === 'mouse'
            || (e.kind === 'welder' && e.state !== 'walk' && e.state !== 'cool') || (e.kind === 'sniper' && e.state === 'track') || (e.kind === 'turret' && e.state !== 'sweep')
            || (e.kind === 'magnet' && e.state !== 'hover') || (e.kind === 'rigger' && e.state === 'pull') || e.kind === 'springer';
        ctx.save();
        ctx.translate(cx + Math.cos(a) * t, cx + Math.sin(a) * t);
        ctx.rotate(a);
        ctx.fillStyle = danger ? '#f87171' : 'rgba(226,232,240,0.6)';
        ctx.beginPath(); ctx.moveTo(10, 0); ctx.lineTo(-6, -8); ctx.lineTo(-6, 8); ctx.fill();
        ctx.restore();
    }
}

// Hearts, joy, the level and its progress, the core, warnings and the big banner (screen pixels).
function drawHud(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    const w = ctx.canvas.width;
    const def = sim.arena.def;
    ctx.font = '700 22px Inter, sans-serif';
    ctx.textAlign = 'left';
    for (let i = 0; i < sim.settings.playerHp; i++) {
        ctx.fillStyle = i < p.hp ? '#f472b6' : 'rgba(255,255,255,0.15)';
        ctx.fillText('♥', 14 + i * 22, 32);
    }
    const barW = 200;
    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.fillRect(14, 42, barW, 12);
    ctx.fillStyle = p.joy < 8 ? '#f87171' : C.joy;
    ctx.fillRect(14, 42, (barW * p.joy) / sim.settings.maxJoy, 12);
    ctx.font = '700 11px Inter, sans-serif';
    ctx.fillStyle = '#0f172a';
    ctx.fillText(`JOY ${p.joy}`, 18, 52);
    if (p.popCd <= 0) {
        ctx.font = '600 11px Inter, sans-serif';
        ctx.fillStyle = '#a5f3fc';
        ctx.fillText('POP ready (right click / E)', 14, 72);
    }
    for (let i = 0; i < p.fizz; i++) { // fizz bubbles left
        ctx.fillStyle = 'rgba(196,181,253,0.35)';
        ctx.strokeStyle = '#c4b5fd';
        ctx.lineWidth = 2;
        ctx.beginPath(); ctx.arc(22 + i * 20, 92, 7, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
    }
    ctx.font = '600 11px Inter, sans-serif';
    ctx.fillStyle = p.fizz > 0 ? '#c4b5fd' : 'rgba(196,181,253,0.45)';
    ctx.fillText(p.fizz > 0 ? 'FIZZ (Q)' : 'no fizz — shoot crates', 14 + Math.max(1, p.fizz) * 20 + 4, 96);
    p.keys.forEach((key, i) => drawHudKey(ctx, 26 + i * 34, 118, KEY_COLOR[key]));
    ctx.textAlign = 'right';
    ctx.font = '800 16px Inter, sans-serif';
    ctx.fillStyle = '#e2e8f0';
    ctx.fillText(`${THEMES[sim.theme].name} · ${sim.level + 1}. ${def.name}`, w - 14, 28);
    ctx.font = '600 12px Inter, sans-serif';
    ctx.fillStyle = '#94a3b8';
    const waves = `wave ${Math.min(sim.wave, def.waves.length)}/${def.waves.length}`;
    const lifts = sim.arena.lifts.length ? ` · lifts ${sim.arena.lifts.filter((l) => !l.broken).length}/${sim.arena.lifts.length}` : '';
    ctx.fillText(`${waves}${lifts} · toys ${sim.enemies.length} · kills ${sim.stats.kills}`, w - 14, 46);
    const objs = sim.arena.objectives;
    const left = objs.filter((o) => !o.broken).length;
    ctx.font = '800 14px Inter, sans-serif';
    if (sim.arena.exitOpen) {
        ctx.fillStyle = '#4ade80';
        ctx.fillText('EXIT OPEN — follow the green arrow', w - 14, 66);
    } else {
        ctx.fillStyle = '#fbbf24';
        const core = sim.arena.core && sim.arena.core.hp > 0 ? (left > 0 ? ' · then the core' : 'break the core') : '';
        ctx.fillText(left > 0 ? `MACHINES ${objs.length - left}/${objs.length}${core}` : core, w - 14, 66);
    }
    const core = sim.arena.core;
    if (core && core.hp > 0) {
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fillRect(w / 2 - 150, 40, 300, 12);
        ctx.fillStyle = '#f87171';
        ctx.fillRect(w / 2 - 150, 40, (300 * core.hp) / core.maxHp, 12);
        ctx.textAlign = 'center';
        ctx.font = '800 11px Inter, sans-serif';
        ctx.fillStyle = '#fff';
        ctx.fillText('DRILL CORE', w / 2, 50);
    }
    if (p.held) {
        ctx.textAlign = 'center';
        ctx.font = '900 20px Inter, sans-serif';
        ctx.lineWidth = 5;
        ctx.strokeStyle = 'rgba(15,23,42,0.9)';
        const msg = `GRABBED — dash (Space) to wriggle free ${p.struggle}/3`;
        ctx.strokeText(msg, w / 2, ctx.canvas.height * 0.62);
        ctx.fillStyle = Math.sin(sim.time * 12) > 0 ? '#f87171' : '#fecaca';
        ctx.fillText(msg, w / 2, ctx.canvas.height * 0.62);
    }
    if (sim.marked) {
        ctx.textAlign = 'center';
        ctx.font = '800 14px Inter, sans-serif';
        ctx.fillStyle = '#fde047';
        ctx.fillText(p.paintT > 0 ? 'PAINTED — every toy sees you' : 'SPOTTED — they aim faster', w / 2, 26);
    }
    if (sim.banner) {
        const b = sim.banner;
        const k = Math.min(1, b.life / BANNER_LIFE);
        ctx.globalAlpha = Math.min(1, k * 3);
        ctx.textAlign = 'center';
        ctx.font = `900 ${42 + (1 - k) * 6}px Inter, sans-serif`;
        ctx.lineWidth = 8;
        ctx.strokeStyle = 'rgba(15,23,42,0.9)';
        const y = ctx.canvas.height * 0.4;
        ctx.strokeText(b.text, w / 2, y);
        ctx.fillStyle = b.text.startsWith('KNOCKED') ? '#f87171' : '#fde047';
        ctx.fillText(b.text, w / 2, y);
        if (b.sub) {
            ctx.font = '700 17px Inter, sans-serif';
            ctx.lineWidth = 5;
            ctx.strokeText(b.sub, w / 2, y + 32);
            ctx.fillStyle = '#e2e8f0';
            ctx.fillText(b.sub, w / 2, y + 32);
        }
        ctx.globalAlpha = 1;
    }
}
