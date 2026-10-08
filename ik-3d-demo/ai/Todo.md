# Enemy AI — Todo

Cute is the art style of the world and the player. Enemies are dangerous: Doom, F.E.A.R. and Far Cry
are the bar. They see and hear, hunt you, coordinate, flank, surround and ambush. The player is
often in a car, so roads, speed and engine noise matter.

## 1. Pathfinding: a layered grid, no navmesh

Fights happen in bounded areas, so one layered (2.5D) grid is the whole AI world there: paths,
cover, visibility and squad positions all live in the same cells. No baking.

- [x] `TacticalGrid` (ai/tactical_grid.gd): scan the area by casting rays down every column;
      keep each walkable surface with headroom and body room — several cells per column
      (floor, balcony, roof). Neighbours link by step height, so **stairs and ramps link floors
      by themselves**; ledges become one-way **drops**; **ladders and jumps** come from
      `GridLink` marker pairs placed in the level. Edge cells (next to a drop) cost more, so
      paths keep off ledges and stair sides.
- [x] A* over cells and links; string-pulled waypoints that each say how they're reached.
- [x] `GridWalker`: steers a Spider along the path (walk, walk off and fall, leap), re-plans
      every 0.5 s and after every landing. `TacticalGridDebug` draws cells, links and the path.
- [x] Test arena (ai/grid_demo.tscn): walls, crates, stairs to a platform, a balcony to walk
      under or drop from, a ladder, a jump gap. Click to move the orb.
- [ ] Moving obstacles: re-scan only the columns around something that moved.
- [ ] Between fights (open world): follow the road / steer at the player until a fight area
      takes over. Revisit only if enemies must chase across the whole map.
- [ ] Later: spiders on walls and ceilings (a 3D grid can, a navmesh can't).

## 2. FOV both ways

### What an enemy sees (senses)
- [x] Vision cone: angle + range per creature (ai/creature_senses.gd: 120°, 14 m for the spider;
      long and narrow for the sentry later). A ray to each point on the player (feet, chest, head;
      car roof and wheels later): seeing 1 of 3 = partly hidden, fills a third as fast.
- [x] Suspicion meter, not instant detection: fills faster when close, centred, more visible,
      moving, and ×3 while searching; drains when out of sight; point blank (< 2.5 m) = alert.
      Seen in the open: ~1 s to alert at 6 m, ~2.5 s at 12 m (fill_rate tunes it).
      Stages (ai/hunter_brain.gd): unaware (wanders) → suspicious (stares, creeps over, looks
      around) → alert (runs at the last known position) → searching → unaware.
- [ ] Light and engine noise in the fill; combat stage (hand to roles / director).
- [x] Hearing: ai/noise_bus.gd, a noise = position + radius (demo: footsteps 3 m, N = 10 m).
      A noise gives a position, not a sighting: suspicious + go and check; while alert or
      searching it moves the last known position.
- [ ] Walls muffle noise (a ray or a grid flood fill = path distance); gunshot, crash, horn, engine.
- [x] Memory: last known position + velocity. Lost you → goes to last known + 1.5 s of your
      velocity (not through walls), then checks up to 4 spots near there that it couldn't see
      when it lost you (open spots fill in), looking around at each; gives up after 10 s.
- [ ] Smarter search: spots along your likely routes (path from last known), cover cells first,
      and remember spots already checked.
- [ ] Shared memory through the squad: one sees you, everyone knows (with a radio/bark delay).

### What the player can see (where to hide)
- [x] **Raycast sampling** (ai/player_visibility.gd): one ray per cell from a creature's body
      height toward the player's eye; blocked = hidden. 64 rays a frame, nearest cells first,
      sweeping again and again (~0.3 s per sweep for ~1000 cells); each cell keeps a timestamp.
- [ ] Re-check cells near movers (and near the player when it moves fast) more often.
- [x] The player's **facing**: `in_view` = seen and inside the view cone right now.
- [ ] Later, if rays get expensive: **shadow-map trick**. Render a small depth cubemap from the
      player's eye (the player as a light). A cell "in shadow" is hidden. One render answers
      thousands of cells; the same image can show a visibility debug view.
- [x] **Cover** = hidden with the blocker within 1.5 m of the cell (the same ray tells);
      **peek** = hidden, one walk from a seen cell (pop out, shoot, hide).
- [x] `CoverPicker`: nearest cover, peek spots preferred; the demo's spider takes cover (F).
- [ ] Cover distance by path length instead of a straight line.

## 3. Positions: utility scoring (the brain of flank and surround)

Each candidate cell gets a score per role. Pick the best, claim it so no one else takes it,
and re-score every ~0.3 s or when the player moves a lot.
**Done (first pass):** ai/position_scorer.gd (ring, angle from the player's facing, wanted sight,
in-view penalty, cover, spacing from claims, level/height, travel, edge), ai/squad.gd (claims
in turn with hysteresis, hold → pounce when outside the player's view, exposure-aware routes),
ai/grid_regions.gd (only cells you can get to and back from), ai/squad_demo.tscn + heatmap.
Re-scoring 4 members costs ~3 ms every 0.4 s in GDScript: stagger members / C++ later.

- Distance to the player vs the role's preferred ring
- Line of sight to the player (attackers need it, flankers on the way must avoid it)
- Exposure: is it seen by the player right now / soon (facing, speed)
- Angle around the player compared to the player's facing (behind = flank)
- Spacing from squadmates (don't bunch; no friendly fire lines)
- Path cost to get there (exposure-aware) and height advantage
  ([x] routes: exposure + personal space as A* cell cost; [ ] score by path cost, not straight line)

Roles (assigned by the squad director):
- [x] **Bait / pressure**: in front, in sight, 4–7 m, likes height. [ ] keeps the player busy (fires, roars, dodges).
- [ ] **Suppressor** (sentry gun): long range, line of sight, sweeps fire to pin the player.
- [x] **Flanker** (left / right / behind): ±105° or 180° from the player's facing, hidden, by a
      hidden route; holds, then pounces when it's outside the player's view.
      [ ] only when the player is busy (with the pressure member) / holds an attack token.
- [ ] **Surround**: angular slots on a ring around the player (N slots, gaps toward escape
      routes closed first). Assign spiders to slots minimising travel (greedy is enough). The
      ring tightens as attack tokens free up.
- [ ] **Ambusher**: a hidden cell on the player's predicted path. In a car that's the road
      ahead (TerraSpline): wait off the road behind cover, freeze (we have ambush already),
      burst out as the car passes.
- [ ] **Retreat / regroup**: hurt or alone → break line of sight, rejoin the pack.

## 4. Squad director (F.E.A.R. / Doom)

- [ ] One director per group: knows all members, the shared memory and the player state.
- [ ] Assigns roles on events (spotted, lost, member died, player in a car / on foot).
- [ ] **Attack tokens** (Doom): only N may attack at once. The others reposition, flank, taunt.
      Tokens go to whoever has the best shot. Fair, readable, and cheaper.
- [ ] Pincer and surround moves: pick a plan (frontal pressure + 2 flankers, full surround, ambush
      ahead), give each member its role, re-plan when it breaks.
- [ ] **Barks**: show the plan. An eye flash and chirp when spotting, a raised leg pointing when
      sending a flanker, a scream when a member dies. The player reads the AI's intent: it feels
      smart, and it's fair.

## 5. Per-creature decision: keep the brain, maybe add a planner later

- [ ] Keep SpiderBrain's behaviours (charge, circle, jump, dive, shoot, ambush, fear, freeze) as
      the actions. The director sets a goal per member; the brain picks the behaviour that serves it.
- [ ] Later, if fights need more variety: GOAP-lite (F.E.A.R.): actions with preconditions and
      effects, a small A* backward from the goal. Only if utility + roles isn't enough.

## 6. Performance (mobile)

- [ ] Combat bubble: tactical grid and full senses only near the player; far enemies get a cheap
      tick (every 0.5–1 s, no rays).
- [ ] Ray budget per frame, time-sliced; cache; prioritise cells that matter (near roles' rings).
- [ ] Re-score positions on a timer, not every frame. Stagger squad members across frames.
- [ ] LOD AI: on screen and close = full; off screen = simulated coarsely.
- [ ] C++ for the grid, A* and scoring once the design settles (GDScript to prototype).

## 7. Debug views (build alongside, not after)

- [ ] Tactical grid coloured by what the player can see (green hidden, red seen, grey not checked).
- [ ] Score heatmap for a chosen role; the chosen cell and its claim.
- [x] Each enemy's vision cone (coloured by awareness), suspicion bar, state, last known
      position + heading, search spots (ai/senses_debug.gd). [ ] current role, token holder.
- [x] Hearing events as expanding rings. [ ] Hidden routes drawn.

## Build order

1. ~~Tactical grid in a test arena: layered cells, stairs, drops, ladders, jumps, A*, walker.~~ (done)
2. ~~Player visibility on the grid, cover and peek cells, debug view, take-cover demo.~~ (done)
2b. ~~Senses: vision cone + LOS rays + suspicion meter + hearing + last known position + search.
   Debug cones and meters. (One spider, the orb as the player.)~~ (done: hunt mode in grid_demo)
3. ~~Position scoring + roles + claims; exposure-aware A* routes on the grid.~~ (first pass done:
   squad_demo; surround slots, ambusher, retreat still to do)
4. Squad director: roles, attack tokens, surround slots, flank plan, barks.
5. Car player: speed and engine noise in the senses, road ambushes on the predicted path.
6. Fight-area hand-off: scan a grid when a fight starts around the player; road/steering between.
7. Performance pass, then move the hot parts to C++.
