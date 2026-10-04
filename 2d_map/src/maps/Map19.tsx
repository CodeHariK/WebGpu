// Puzzle Islands — playable lab for the game's Dino-and-Aliens puzzle rooms.
// Generate seeded island levels, play them (arrows / WASD, Z undo, R restart), watch the
// solver's solution, and scan neighbouring seeds as thumbnails.
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { DEFAULT_GEN, generateLevel, type GenParams, type GenResult } from './Map19Generator';
import { parseLevel } from './Map19Rules';
import { solveKeyAndExit } from './Map19Solver';
import { applyToken, detonateRemotes, dropBomb, face, interact, melee, solutionSteps, startPlay, step, throwBomb, tick, type Play } from './Map19Play';
import { freeBatteries } from './Map19Env';
import { drawLevel } from './Map19Render';
import { LOCK_COLORS } from './Map19Rules';
import { parseEnemies } from './Map19Enemies';
import { budgetParams, scoreLevel, type Difficulty } from './Map19Difficulty';
import { DEFAULT_DUNGEON, generateDungeon, type DungeonParams, type RoomInfo, type RoomRole } from './Map19Dungeon';

const UI = { accent: '#22b07d', text: '#94a3b8' };
const KEY_DIRS: Record<string, number> = {
    ArrowRight: 0, d: 0, D: 0, ArrowLeft: 1, a: 1, A: 1, ArrowDown: 2, s: 2, S: 2, ArrowUp: 3, w: 3, W: 3,
};
const THUMBS = 8;
const ROLE_COLOR: Record<RoomRole, string> = { start: '#7dff9a', puzzle: '#ffd23f', rest: '#7ec8ff', bonus: '#ff9adf', final: '#ff7a5a' };
const TIER_COLOR: Record<Difficulty['tier'], string> = { Easy: '#7dff9a', Medium: '#fde68a', Hard: '#ffa94d', Expert: '#ff6b6b' };

export default function Map19({ width = 800, height = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    // Fit the panel's real width (the sidebar / explanation can leave less than `width`).
    const rootRef = useRef<HTMLDivElement>(null);
    const [avail, setAvail] = useState(width);
    useEffect(() => {
        const el = rootRef.current;
        if (!el) return;
        const ro = new ResizeObserver(() => setAvail(Math.floor(el.clientWidth)));
        ro.observe(el);
        return () => ro.disconnect();
    }, []);
    const [gen, setGen] = useState<GenParams>(DEFAULT_GEN);
    const [budget, setBudget] = useState(0); // DIFFICULTY 1..10 drives the settings; 0 = manual sliders
    // Touching a setting slider switches DIFFICULTY to manual, starting from the budget's values.
    const set = <K extends keyof GenParams>(k: K) => (v: GenParams[K]) => {
        const keep = k === 'seed' || !budget;
        setGen((g) => ({ ...(keep ? g : budgetParams(budget, g)), [k]: v }));
        if (!keep) setBudget(0);
    };
    const params = useMemo(() => (budget ? budgetParams(budget, gen) : gen), [budget, gen]);
    // Generator mode: the new kid-friendly DUNGEON (rooms with roles; World 1 = crates + keys)
    // or the CLASSIC everything-mixed lab generator.
    const [mode, setMode] = useState<'dungeon' | 'classic'>('dungeon');
    const [dgen, setDgen] = useState<DungeonParams>(DEFAULT_DUNGEON);
    const setD = <K extends keyof DungeonParams>(k: K) => (v: DungeonParams[K]) => setDgen((g) => ({ ...g, [k]: v }));
    const seed = mode === 'dungeon' ? dgen.seed : params.seed;
    const setSeed = useCallback((v: number) => (mode === 'dungeon' ? setDgen((g) => ({ ...g, seed: v })) : setGen((g) => ({ ...g, seed: v }))), [mode]);
    const generate = useCallback(
        (sd: number): GenResult & { roomInfo?: RoomInfo[]; sequence?: string } =>
            mode === 'dungeon' ? generateDungeon({ ...dgen, seed: sd }) : generateLevel({ ...params, seed: sd }),
        [mode, dgen, params],
    );

    // --- Generation (synchronous; usually < 0.3 s)
    const { result, genMs } = useMemo(() => {
        const t0 = performance.now();
        const r = generate(seed);
        return { result: r, genMs: Math.round(performance.now() - t0) };
    }, [generate, seed]);
    const level = useMemo(() => (result.ok ? parseLevel(result.rows) : null), [result]);
    // Canvas = exactly the level (no letterbox), cell size from the available width / height.
    const cell = level ? Math.max(8, Math.floor(Math.min(Math.min(width, avail) / level.w, height / level.h))) : 16;
    const canvasW = level ? level.w * cell : Math.min(width, avail);
    const canvasH = level ? level.h * cell : 200;
    const score = useMemo(() => (level && result.ok ? scoreLevel(level, result.rows, result.pushes, result.states, result.legs) : null), [level, result]);

    // --- Play: the live state runs in real time (a ref, advanced every animation frame, so
    // enemies act on their own clocks); undo keeps a snapshot per player step.
    const enemies0 = useMemo(() => (level && result.ok ? parseEnemies(level, result.rows) : []), [level, result]);
    const live = useRef<Play | null>(null);
    const undo = useRef<Play[]>([]);
    const [play, setPlay] = useState<Play | null>(null); // UI mirror for the status line

    const restart = useCallback(() => {
        live.current = level ? startPlay(level, enemies0) : null;
        undo.current = [];
        setPlay(live.current);
    }, [level, enemies0]);
    useEffect(() => restart(), [restart]);

    /** Apply a player action (step, bomb, melee) with an undo snapshot. */
    const doAction = useCallback((fn: (p: Play) => Play) => {
        const cur = live.current;
        if (!cur) return;
        const next = fn(cur);
        if (next === cur) return;
        undo.current.push(cur);
        live.current = next;
        setPlay(next);
    }, []);
    const doStep = useCallback((dir: number) => doAction((p) => step(p, dir)), [doAction]);
    const doUndo = useCallback(() => {
        const prev = undo.current.pop();
        if (prev) {
            live.current = prev;
            setPlay(prev);
        }
    }, []);

    // --- Solution replay (the solver ignores enemies, so they may still get you)
    const [replay, setReplay] = useState<number[] | null>(null);
    const showSolution = useCallback(() => {
        if (!level) return;
        const sol = solveKeyAndExit(level);
        if (!sol.solved || !sol.legs) return;
        restart();
        setReplay(solutionSteps(level, sol.legs));
    }, [level, restart]);
    useEffect(() => {
        if (!replay) return;
        if (!replay.length) { setReplay(null); return; }
        const tok = replay[0];
        const id = setTimeout(() => { doAction((p) => applyToken(p, tok)); setReplay(replay.slice(1)); }, 110);
        return () => clearTimeout(id);
    }, [replay, doAction]);

    // --- Keyboard
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            if ((e.target as HTMLElement)?.tagName === 'INPUT') return;
            if (e.key in KEY_DIRS) {
                e.preventDefault();
                setReplay(null);
                const d = KEY_DIRS[e.key];
                if (e.shiftKey) doAction((p) => face(p, d)); // turn in place
                else doStep(d);
            }
            else if (e.key === 'e' || e.key === 'E') doAction(interact);
            else if (e.key === ' ') { e.preventDefault(); doAction(melee); }
            else if (e.key === 'b' || e.key === 'B') doAction((p) => dropBomb(p, 'time'));
            else if (e.key === 't' || e.key === 'T') doAction(throwBomb);
            else if (e.key === 'c' || e.key === 'C') doAction((p) => dropBomb(p, 'remote'));
            else if (e.key === 'x' || e.key === 'X') doAction(detonateRemotes);
            else if (e.key === 'z' || e.key === 'Z') doUndo();
            else if (e.key === 'r' || e.key === 'R') { setReplay(null); restart(); }
            else if (e.key === 'n' || e.key === 'N') setSeed(seed + 1);
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, [doStep, doAction, doUndo, restart, setSeed, seed]);

    // --- Frame loop: advance enemies / bullets / lasers in real time, then draw.
    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas || !level) return;
        const ctx = canvas.getContext('2d')!;
        let raf = 0;
        let last = performance.now();
        const frame = (ms: number) => {
            const dt = Math.min(0.1, (ms - last) / 1000);
            last = ms;
            const cur = live.current;
            if (cur) {
                const next = tick(cur, dt);
                live.current = next;
                if (next.caught !== cur.caught || next.zapped !== cur.zapped || next.kills !== cur.kills || next.message !== cur.message || next.bombs.length !== cur.bombs.length || Math.ceil(next.health * 10) !== Math.ceil(cur.health * 10)) setPlay(next); // status line
                ctx.setTransform(1, 0, 0, 1, 0, 0);
                ctx.fillStyle = '#0d1a12';
                ctx.fillRect(0, 0, canvas.width, canvas.height);
                drawLevel(ctx, level, next.s, { cell, hasKey: next.hasKey, time: ms / 1000, enemies: next.enemies, bullets: next.bullets, health: next.health, hurt: next.hurt, beat: next.time,
                    items: next.items, bombs: next.bombs, blasts: next.blasts, deadAliens: next.deadAliens, facing: next.facing, swing: next.swing });
                if (result.roomInfo) drawRooms(ctx, result.roomInfo, cell);
            }
            raf = requestAnimationFrame(frame);
        };
        raf = requestAnimationFrame(frame);
        return () => cancelAnimationFrame(raf);
    }, [level, cell, result]);

    // --- Neighbouring seeds as thumbnails (generated one per tick so the UI stays live)
    const [thumbs, setThumbs] = useState<{ seed: number; rows: string[]; pushes: number; score: Difficulty }[]>([]);
    useEffect(() => {
        setThumbs([]);
        let k = 1;
        let id = 0;
        const next = () => {
            const sd = seed + k;
            const r = generate(sd);
            if (r.ok) setThumbs((t) => [...t, { seed: sd, rows: r.rows, pushes: r.pushes, score: scoreLevel(parseLevel(r.rows), r.rows, r.pushes, r.states, r.legs) }]);
            if (++k <= THUMBS) id = window.setTimeout(next, 0);
        };
        id = window.setTimeout(next, 30);
        return () => clearTimeout(id);
    }, [generate, seed]);

    const info = result.ok && result.sequence
        ? `${score ? `${score.tier} ${score.total} · ` : ''}${result.sequence} · ${result.pushes} pushes in total · attempt ${result.attempts} · ${genMs} ms`
        : result.ok
        ? `${score ? `${score.tier} ${score.total} (sokoban ${score.sokoban.toFixed(1)} + mechanics ${score.mechanics.toFixed(1)} + enemies ${score.enemies.toFixed(1)}, ${score.onRoute}/${score.enemyCount} on route) · ` : ''}${level?.gates.size ?? 0} locks · key + exit in ${result.pushes} pushes · bomb → alien in ${result.bombPushes} · attempt ${result.attempts} · ${result.states} solver states · ${genMs} ms`
        : `no solvable level in ${result.attempts} attempts — loosen the settings (rejected: layout ${result.rejected.layout}, unsolvable ${result.rejected.unsolvable}, easy ${result.rejected.easy}, bomb ${result.rejected.bomb})`;

    return (
        <div ref={rootRef} style={{ display: 'flex', flexDirection: 'column', width: '100%', maxWidth: width, minWidth: 0, background: '#0a0b12', borderRadius: 12, overflow: 'hidden' }}>
            <div>
                <div style={{ padding: '10px 12px 8px', color: '#e2e8f0', fontSize: '0.8rem', fontWeight: 600 }}>
                    <div>{info}</div>
                    {play && (
                        <div style={{ marginTop: 4, color: play.won ? '#7dff9a' : play.caught ? '#ff7a7a' : '#fde68a' }}>
                            {play.message} · ❤ {Math.round(play.health * 10) / 10} · 💣 time {play.ammo.time} · throw {play.ammo.throw} · remote {play.ammo.remote}
                            {freeBatteries(level!, play.s) ? ` · 🔋 ${freeBatteries(level!, play.s)}` : ''}{play.kills ? ` · kills ${play.kills}` : ''} · steps {play.steps} · pushes {play.pushes}
                            {` · 🔑 ${level!.golds.filter((_, i) => (play.s.gold >> i) & 1).length}/${level!.golds.length}`}{level!.aliens.length ? ` · aliens ${play.deadAliens.length}/${level!.aliens.length}` : ''}
                            {LOCK_COLORS.map((col, i) => (play.s.keys & (1 << i) ? <span key={i} style={{ color: col }}> ●</span> : null))}
                        </div>
                    )}
                </div>
                <canvas ref={canvasRef} width={canvasW} height={canvasH} style={{ display: 'block', margin: '0 auto' }} />
                <div style={{ padding: '8px 12px 10px', color: UI.text, fontSize: '0.72rem' }}>
                    Arrows / WASD move · Shift+arrow turn · E use (flip mirror / battery in-out) · Space melee · B time bomb · T throw · C remote / X detonate · Z undo · R restart · N next seed ·{' '}
                    <span style={{ color: '#ff4d4d' }}>● red melee</span>{' '}
                    <span style={{ color: '#ffd23f' }}>● yellow shooter</span>{' '}
                    <span style={{ color: '#4aa8ff' }}>● blue laser</span>{' '}
                    <span style={{ color: '#ff9a3c' }}>● patrol</span>{' '}
                    <span style={{ color: '#a66de0' }}>◎ alien = bomb target (static)</span>{' '}
                    <span style={{ color: '#36d6ff' }}>▥ laser gate: light ◎ receivers</span>{' '}
                    <span style={{ color: '#ffa94d' }}>▥ plate gate: crate on plate</span>{' '}
                    <span style={{ color: '#ff8a3b' }}>— pulsing laser</span>{' '}
                    <span style={{ color: '#c08a4e' }}>▣ wooden door: bomb next to it</span>{' '}
                    <span style={{ color: '#c99a5b' }}>▤ wooden wall: lasers pass, unbreakable</span>
                </div>
            </div>

            <div style={{ display: 'flex', gap: 8, padding: '10px 14px', overflowX: 'auto', borderTop: '1px solid rgba(255,255,255,0.08)' }}>
                {thumbs.map((t) => (
                    <Thumb key={t.seed} {...t} onClick={() => setSeed(t.seed)} />
                ))}
            </div>

            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, alignItems: 'center', padding: '14px 16px', borderTop: '1px solid rgba(255,255,255,0.08)' }}>
                <button onClick={() => setMode(mode === 'dungeon' ? 'classic' : 'dungeon')} style={btn(false)}>
                    GENERATOR: {mode === 'dungeon' ? 'DUNGEON · WORLD 1' : 'CLASSIC (everything)'}
                </button>
                <button onClick={() => setSeed(Math.floor(Math.random() * 99999))} style={btn(true)}>RANDOMIZE</button>
                <button onClick={showSolution} style={btn(false)}>SHOW SOLUTION</button>
                {mode === 'dungeon' && (
                    <>
                        <Slider label={`DIFFICULTY ${dgen.difficulty}`} min={1} max={10} value={dgen.difficulty} onChange={setD('difficulty')} />
                        <Slider label={`SEED ${dgen.seed}`} min={0} max={99999} value={dgen.seed} onChange={setD('seed')} />
                        <Slider label={`WIDTH ${dgen.width}`} min={13} max={35} value={dgen.width} onChange={setD('width')} />
                        <Slider label={`HEIGHT ${dgen.height}`} min={9} max={25} value={dgen.height} onChange={setD('height')} />
                        <Slider label={`ROOMS ${dgen.rooms}`} min={2} max={9} value={dgen.rooms} onChange={setD('rooms')} />
                        <span style={{ color: UI.text, fontSize: '0.72rem' }}>
                            {(Object.keys(ROLE_COLOR) as RoomRole[]).map((r) => <span key={r} style={{ color: ROLE_COLOR[r], marginRight: 8 }}>■ {r}</span>)}
                        </span>
                    </>
                )}
                {mode === 'classic' && (<>
                <Slider label={`DIFFICULTY ${budget || 'MANUAL'}`} min={0} max={10} value={budget} onChange={(v) => { if (!v && budget) setGen(budgetParams(budget, gen)); setBudget(v); }} />
                <Slider label={`SEED ${params.seed}`} min={0} max={99999} value={params.seed} onChange={set('seed')} />
                <Slider label={`WIDTH ${params.width}`} min={11} max={35} value={params.width} onChange={set('width')} />
                <Slider label={`HEIGHT ${params.height}`} min={9} max={25} value={params.height} onChange={set('height')} />
                <Slider label={`ROOMS ${params.rooms}`} min={1} max={10} value={params.rooms} onChange={set('rooms')} />
                <Slider label={`CRATES ${params.extraCrates}`} min={0} max={12} value={params.extraCrates} onChange={set('extraCrates')} />
                <Slider label={`BOMBS ${params.bombs}`} min={0} max={5} value={params.bombs} onChange={set('bombs')} />
                <Slider label={`ALIENS ${params.aliens}`} min={0} max={6} value={params.aliens} onChange={set('aliens')} />
                <Slider label={`MIN PUSHES ${params.minPushes}`} min={0} max={20} value={params.minPushes} onChange={set('minPushes')} />
                <Slider label={`LOCKS ${params.locks}`} min={0} max={3} value={params.locks} onChange={set('locks')} />
                <Slider label={`PATROLLERS ${params.patrollers}`} min={0} max={6} value={params.patrollers} onChange={set('patrollers')} />
                <Slider label={`RED ${params.reds}`} min={0} max={6} value={params.reds} onChange={set('reds')} />
                <Slider label={`YELLOW ${params.yellows}`} min={0} max={4} value={params.yellows} onChange={set('yellows')} />
                <Slider label={`BLUE ${params.blues}`} min={0} max={3} value={params.blues} onChange={set('blues')} />
                <Slider label={`LASER GATES ${params.laserGates}`} min={0} max={2} value={params.laserGates} onChange={set('laserGates')} />
                <Slider label={`MIRROR CHAINS ${params.mirrorChains}`} min={0} max={2} value={params.mirrorChains} onChange={set('mirrorChains')} />
                <Slider label={`WOOD LASERS ${params.woodLasers}`} min={0} max={2} value={params.woodLasers} onChange={set('woodLasers')} />
                <Slider label={`GOLD KEYS ${params.goldKeys}`} min={1} max={5} value={params.goldKeys} onChange={set('goldKeys')} />
                <Slider label={`TELEPORTERS ${params.teleporters}`} min={0} max={3} value={params.teleporters} onChange={set('teleporters')} />
                <Slider label={`PLATE GATES ${params.plateGates}`} min={0} max={2} value={params.plateGates} onChange={set('plateGates')} />
                <Slider label={`DEAD EMITTERS ${Math.round(params.deadEmitterChance * 100)}%`} min={0} max={100} value={Math.round(params.deadEmitterChance * 100)} onChange={(v) => set('deadEmitterChance')(v / 100)} />
                <Slider label={`HEARTS ${params.hearts}`} min={0} max={4} value={params.hearts} onChange={set('hearts')} />
                <Slider label={`BOMB PICKUPS ${params.bombPickups}`} min={0} max={8} value={params.bombPickups} onChange={set('bombPickups')} />
                <Slider label={`WOOD WALLS ${params.woodWalls}`} min={0} max={8} value={params.woodWalls} onChange={set('woodWalls')} />
                <Slider label={`WOODEN DOORS ${params.woodDoors}`} min={0} max={3} value={params.woodDoors} onChange={set('woodDoors')} />
                <Slider label={`PULSE LASERS ${params.pulseLasers}`} min={0} max={4} value={params.pulseLasers} onChange={set('pulseLasers')} />
                </>)}
            </div>
        </div>
    );
}

function Thumb({ seed, rows, pushes, score, onClick }: { seed: number; rows: string[]; pushes: number; score: Difficulty; onClick: () => void }) {
    const ref = useRef<HTMLCanvasElement>(null);
    const L = useMemo(() => parseLevel(rows), [rows]);
    const cell = 6;
    useEffect(() => {
        const ctx = ref.current?.getContext('2d');
        if (ctx) drawLevel(ctx, L, L.start, { cell });
    }, [L]);
    return (
        <button onClick={onClick} title={`seed ${seed}`} style={{ background: 'rgba(255,255,255,0.04)', border: '1px solid rgba(255,255,255,0.1)', borderRadius: 8, padding: 6, cursor: 'pointer', color: UI.text, fontSize: '0.68rem', flexShrink: 0 }}>
            <canvas ref={ref} width={L.w * cell} height={L.h * cell} style={{ display: 'block', borderRadius: 4 }} />
            <div style={{ marginTop: 4 }}>#{seed} · {pushes} pushes</div>
            <div style={{ color: TIER_COLOR[score.tier] }}>{score.tier} {score.total}</div>
        </button>
    );
}

const btn = (accent: boolean) => ({
    background: accent ? UI.accent : 'rgba(255,255,255,0.08)',
    color: accent ? 'white' : '#e2e8f0',
    border: accent ? 'none' : '1px solid rgba(255,255,255,0.14)',
    padding: '10px 16px',
    borderRadius: 8,
    cursor: 'pointer',
    fontWeight: 700,
    fontSize: '0.78rem',
});

const Slider = ({ label, min, max, value, onChange }: { label: string; min: number; max: number; value: number; onChange: (v: number) => void }) => (
    <div style={{ padding: '8px 14px', background: 'rgba(255,255,255,0.05)', borderRadius: 8, display: 'flex', alignItems: 'center', gap: 10, color: UI.text, border: '1px solid rgba(255,255,255,0.1)' }}>
        <span style={{ fontSize: '0.72rem', fontWeight: 600, whiteSpace: 'nowrap' }}>{label}</span>
        <input type="range" min={min} max={max} value={value} onChange={(e) => onChange(Number(e.target.value))} style={{ cursor: 'pointer', accentColor: UI.accent }} />
    </div>
);

/** Dungeon mode: tint each room by its role and label it (with its pushes). */
function drawRooms(ctx: CanvasRenderingContext2D, rooms: RoomInfo[], cell: number) {
    ctx.save();
    for (const r of rooms) {
        const x = r.rect.x0 * cell;
        const y = r.rect.y0 * cell;
        const w = (r.rect.x1 - r.rect.x0 + 1) * cell;
        const h = (r.rect.y1 - r.rect.y0 + 1) * cell;
        ctx.globalAlpha = 0.08;
        ctx.fillStyle = ROLE_COLOR[r.role];
        ctx.fillRect(x, y, w, h);
        ctx.globalAlpha = 0.85;
        ctx.font = `bold ${Math.max(9, Math.round(cell * 0.32))}px sans-serif`;
        ctx.textBaseline = 'top';
        const text = r.role === 'start' || r.role === 'rest' ? r.role : `${r.role} ${r.pushes}`;
        ctx.fillStyle = 'rgba(0,0,0,0.55)';
        ctx.fillRect(x + 2, y + 2, ctx.measureText(text).width + 8, Math.max(9, Math.round(cell * 0.32)) + 6);
        ctx.fillStyle = ROLE_COLOR[r.role];
        ctx.fillText(text, x + 6, y + 5);
    }
    ctx.restore();
}
