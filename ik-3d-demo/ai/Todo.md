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
- [ ] Vision cone: angle + range per creature (wide and short for spiders, long and narrow for
      the sentry). Raycast to a few points on the player (head, chest, car roof and wheels):
      seeing 1 of 4 = partly hidden.
- [ ] Suspicion meter, not instant detection: it fills faster when close, in the middle of the
      cone, when the player moves, in light, and when the engine is loud. It drains when out of
      sight. Stages: unaware → suspicious (looks, steps toward) → searching → alert (hunts) → combat.
- [ ] Hearing: stimuli with a radius (gunshot, crash, horn, engine by speed, footsteps). Hearing
      gives a position, not a sighting: go and check.
- [ ] Memory: last known position + velocity + time. If it loses you it goes where you *were*,
      predicts where you went, and searches (Far Cry style search pattern), then gives up slowly.
- [ ] Shared memory through the squad: one sees you, everyone knows (with a radio/bark delay).

### What the player can see (where to hide)
- [ ] **Raycast sampling** (start here): from the player's eye (or the car camera) to each
      tactical cell at a creature's height. Blocked = hidden. Spread the rays over frames (a
      budget, e.g. 32 per frame) and nearest/most needed cells first. Cache with a timestamp,
      re-check cells near movers more often.
- [ ] Use the player's **facing** too: a cell in sight but well outside the camera view (behind,
      far to the side) counts as "unseen right now". That's what makes flanking work.
- [ ] Later, if rays get expensive: **shadow-map trick**. Render a small depth cubemap from the
      player's eye (the player as a light). A cell "in shadow" is hidden. One render answers
      thousands of cells; the same image can show a visibility debug view.
- [ ] Cover = a cell hidden from the player with a blocker between, next to a peek cell that can
      see the player (pop out, shoot, hide).

## 3. Positions: utility scoring (the brain of flank and surround)

Each candidate cell gets a score per role. Pick the best, claim it so no one else takes it,
and re-score every ~0.3 s or when the player moves a lot.

- Distance to the player vs the role's preferred ring
- Line of sight to the player (attackers need it, flankers on the way must avoid it)
- Exposure: is it seen by the player right now / soon (facing, speed)
- Angle around the player compared to the player's facing (behind = flank)
- Spacing from squadmates (don't bunch; no friendly fire lines)
- Path cost to get there (exposure-aware) and height advantage

Roles (assigned by the squad director):
- [ ] **Bait / pressure**: in front, in sight, keeps the player busy (fires, roars, dodges).
- [ ] **Suppressor** (sentry gun): long range, line of sight, sweeps fire to pin the player.
- [ ] **Flanker**: goes for an angle ≥ 90° from the player's facing by a hidden route, then attacks
      from the side or behind when the player is busy.
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
- [ ] Each enemy's vision cone, suspicion meter, last known position, current role, token holder.
- [ ] Hidden routes drawn; hearing events as expanding rings.

## Build order

1. ~~Tactical grid in a test arena: layered cells, stairs, drops, ladders, jumps, A*, walker.~~ (done)
2. Player visibility on the grid (rays from the player's eye to each cell), cover and peek
   cells, debug view.
2b. Senses: vision cone + LOS rays + suspicion meter + hearing + last known position + search.
   Debug cones and meters. (One spider, the orb as the player.)
3. Position scoring + roles + claims; exposure-aware A* routes on the grid.
4. Squad director: roles, attack tokens, surround slots, flank plan, barks.
5. Car player: speed and engine noise in the senses, road ambushes on the predicted path.
6. Fight-area hand-off: scan a grid when a fight starts around the player; road/steering between.
7. Performance pass, then move the hot parts to C++.
