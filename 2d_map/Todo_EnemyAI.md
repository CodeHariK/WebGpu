# Enemy AI — the simple version (Todo)

The tactical-grid AI in `ik-3d-demo/ai/` works but is too smart and too big (22 algorithms,
~2000 lines). Games like Far Cry, Batman Arkham and BioShock get great fights from a few cheap
tricks. This is the plan to rebuild it simple, prototype it fast in 2D here (Map 20 "Enemy
Arena"), then port to Godot.

## What those games actually do

**Far Cry (outposts)**
- Guards stand at a post or walk 2–4 patrol waypoints and never leave their area (leash).
- Detection meter over the head fills while they see you (faster close / when you move).
- "Huh?" → walk to the noise / last seen spot, look around a few seconds, go back.
- Alerted → shout; anyone in earshot joins. Some run to an alarm (reinforcements).
- Fight → hand-placed cover spots near the outpost; grenade if you camp.

**Batman Arkham (freeflow)**
- Thugs stand in a loose ring around you and shuffle sideways.
- Only 1–2 may attack at once (attack tokens).
- A warning before every hit (blue lightning) → the player can counter.
- Predator rooms: fixed patrol routes; a takedown → others gather at the body, then panic and split.

**BioShock**
- Splicers run at you or strafe and shoot; hurt → run to a health station.
- Some hide on ceilings and drop down. Big Daddies ignore you until hit.

**Other famous behaviours**
- Retreat when hurt, come back. Call for help. Flush you out (grenade) if you hide too long.
- Peek from cover, shoot, duck. Pack hunting: one distracts in front, others circle behind.
- Feints / taunts (step in, step back, roar). Ambush from above / burrow out of the ground.
- Stagger when hit. Flee when the leader dies. Get bored: lose you → shrug → walk home.

## The simple package

| Need | Cheap answer |
|---|---|
| Not fighting | Waypoint patrol + leash (give up when too far from home) |
| Noticing | Vision cone + 1 ray + detection meter; noise radius |
| Group alert | Shout radius: one spots you → everyone within N m is alerted |
| Surround | Slot ring: fixed angle slots round the player, each checked with 1–2 rays, nearest free slot |
| Moving to a slot | Context steering (8–16 directions: interest vs danger); short A* only if a wall blocks |
| Taking turns | Attack tokens + a visible wind-up before each hit (dodgeable) |
| Hiding | Nearest hand-placed cover marker the player can't see (1 ray) |
| Losing you | Go to last seen → look around → walk home |
| Car | Hand-placed ambush markers beside the road; jump when the car is in range |

About 12 small pieces, ~300–400 lines, each easy to reason about.

## Slot ring (the surround idea)

1. 8 slots on a circle round the player (3–5 m), fixed angles 45° apart (stable, no jitter).
2. A slot is valid if it isn't inside a wall and a ray from the player to it is clear.
3. Each enemy takes the nearest free slot (greedy; small bonus for keeping its old one).
4. 2–3 rings: close (token holder attacks), mid (waiting), far (ranged enemies).
5. Flank: some enemies prefer slots behind the player's facing.

## Other cheap algorithms (toolbox)

- **Steering**: seek, arrive, separation from friends, 3 whisker rays to slide along walls.
- **Context steering**: score 8–16 directions; pick the best. Great in small arenas.
- **Flow field / Dijkstra map**: one BFS from the player over a small local grid; every enemy
  steps downhill. One pass for 50 enemies.
- **Hand-placed markers (smart objects)**: cover, patrol, ambush, vantage, alarm points in the
  level. Replaces cover scans, ambush planners and search logic; the designer stays in control.
- **Random cooldowns / jitter** so enemies don't act on the same frame (looks alive for free).
- **Think every 0.2–0.5 s**, not every frame; far / off-screen enemies tick rarely.

## Keep from ik-3d-demo/ai (Godot)

- Grid scan + walk/drop/ladder/jump links, A* + heap, string pulling, waypoint steering,
  ballistic leap — movement is solid and cheap; use it only for short hops in a fight area.
- Cone + LOS rays + suspicion meter, noise bus, last known position, attack tokens.

## Drop (too smart)

- Player-visibility sweep of every cell, cover/peek classification, per-role utility scoring,
  claims + hysteresis, exposure route costs, SCC regions, pincer/surround plan assignment,
  squad shared-memory delay, hidden-spot search, the ambush planner.
- Don't delete `ai/` until the simple version is ported and working in Godot.

## Steps

1. [x] Map 20 "Enemy Arena" in 2d_map (TypeScript, canvas): arena of boxes, raycasts,
       player (WASD, mouse aim, click attack, Space noise, Shift sneak).
2. [x] Patrol + leash, cone + meter, hearing, shout.
3. [x] Slot ring surround + context steering + separation.
4. [x] Attack tokens + wind-up telegraph + player can dodge / hit back (stagger, hp).
5. [x] Ranged enemy: cover markers, peek and shoot with a telegraph.
6. [x] Search last seen → return home. Retreat when hurt.
7. [x] Toggles for every behaviour + sliders; keep only what earns its place.
   Done as a first pass: src/maps/Map20*.ts (AI ~900 lines incl. drawing; the AI files are plain
   TypeScript). Headless test: 7 attacks, never more than 1 at once; 0.06 ms per step for 7 enemies.
   Fixed: an enemy giving up at the leash turned straight back (now ignores you 3 s while returning).
   [ ] Play it and tune: which toggles earn their place? Feel of wind-up / lunge / dodge.
   [ ] More enemies per camp (packs), a 'pack leader' that shouts, flee when the leader dies.
8. [ ] Port to Godot (GDScript first, then C++): new small folder, reuse grid movement.
9. [ ] Car: hand-placed ambush markers along the road.
