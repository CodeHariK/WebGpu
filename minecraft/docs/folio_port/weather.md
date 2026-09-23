# Weather & Season System — how it works, and the plan to simplify it

> Status: the current system is a straight port of folio-2025's `Weather.js`. It
> works but is hard to reason about and doesn't produce reliable seasonal events.
> This doc explains **how it works today**, **why it has so many parameters**, and
> **the proposed deterministic replacement**. See `TODO.md` → "Weather/season
> system — known issues & planned rework" for the short bug list.

Source: `src/folio/weather.{h,cpp}`, `src/folio/cycles/year_cycles.{h,cpp}`,
`src/folio/cycles/cycle.{h,cpp}`. Diagnosed with the `--debug` parameter logger →
`docs/env_report.html`.

---

## 1. How the current system works

It is built on **noise**, but not Perlin / FastNoiseLite. There is one hand-rolled
pseudo-noise function (`FolioWeather::noise`):

```
noise(x) = sin(x) · sin(1.678·x) · sin(2.345·x)
```

Three sine waves at incommensurate (non-repeating) frequencies, multiplied. The
result wobbles in roughly [−1, 1] and, because 1.678 and 2.345 don't collapse to a
simple period, it *looks* random while being fully **deterministic** — same every
run, no RNG. It's cheap (3 sines) and was chosen for ambient portfolio weather.

Everything is driven by **one clock**:

```
progress = elapsedScaledTime / 240s
```

Each weather value samples that same noise at a **different frequency**:

| value | formula |
|---|---|
| temperature | `seasonBase + noise(progress·0.40)·7.5` |
| humidity    | `seasonBase + noise(progress·0.36)·0.2` |
| clouds      | `clamp(seasonBase + noise(progress·0.44)·0.4, 0, 1)` |
| wind        | `noise(progress·1.0)·0.5 + 0.5` |

The last two are **not independent** — they're *derived* from the ones above:

- **rain** = `remapClamp(humidity, 0.65→1) · remapClamp(clouds, 0→1)`
  → it only rains when it's **humid AND cloudy**.
- **snow** = `rainThatFreezes − meltAbove0C`
  - `rain_ratio  = remapClamp(rain, 0.05→0.3, 0→1)`
  - `freeze_ratio = remapClamp(temp, 0→−5, 0→1)`
  - `melt_ratio  = remapClamp(temp, 0→10, 0→−1)`
  - `snow = rain_ratio · freeze_ratio + melt_ratio`
  → snow is **literally "rain, gated by temperature."**

`YearCycles` (tick 7, before Weather at tick 8) pushes seasonal baselines into
`base_temperature` / `base_humidity` / `base_clouds`, so the noise wobbles *around*
a seasonal centre. `leaves` comes from a **separate** track in `YearCycles`, not
from Weather at all. An override (`set_override`) can force any value for
testing / events.

All six values are published as global shader uniforms
(`folio_weather_temperature/_humidity/_clouds/_wind/_rain/_snow`) and `wind` also
drives the shared `FolioWind` strength.

### This directly explains the observed bugs
- **Rain ≈ snow** — because snow *is* rain with a cold filter; they track each other.
- **Leaf %/visuals disagree** — leaves come from YearCycles' `leaves` track, a
  density value, not a fall *event*, and it's decoupled from Weather.
- **Snow stat vs line mismatch** — `snow` decays negative (melt term), while the
  logger counts `snow > 0.01`; and `*_pct_of_year` divides multi-year totals by one
  year length, so it can exceed 100% (e.g. rain 121.9%). Reporting artifact, not sim.

---

## 2. Why it needs so many parameters

There are really **4 independent inputs** (temperature, humidity, clouds, wind —
each its own noise phase), plus **2 derived** (rain, snow) and **1 external**
(leaves). The count isn't the real problem — the coupling is:

temperature, humidity and clouds ride **three different noise frequencies**
(0.40 / 0.36 / 0.44). To get snow you need **cold + humid + cloudy at the same
instant** — three independent wobbles that happen to peak together. That alignment
is luck, which is why snow was rare and erratic until the winter cloud baseline was
forced up.

This is a **simulationist** model: you author *noise* and hope a season emerges.
Fine for background ambience where exact timing doesn't matter; wrong for a game
that wants "it snows in winter" to reliably happen, with distinct rain / snow /
leaf-fall events.

---

## 3. How games usually do weather

Almost no stylised game physically simulates weather. They use a small **scheduled
state machine** keyed to season/time:

- **Stardew Valley** — rolls each day's weather from the calendar; winter forces
  snow, festival days are hardcoded, spring has a rain chance. Fully table-driven.
- **Zelda: BOTW / TOTK** — a weather state per region with scheduled + probabilistic
  transitions and a visible forecast.
- **Animal Crossing / Pokémon** — weather tied to a seasonal pattern plus a per-day
  seed.

Common thread: weather is **authored against the season, not simulated.** For a
cute, seasonal game, Stardew's model is the closest fit.

---

## 4. Proposed simplified system — "season timeline"

**Key move: stop deriving events from noise. Author each visible effect directly as
a curve over the year.**

We already have the right tool: `FolioCycle` is a keyframe interpolator over a 0–1
phase, and `YearCycles` already produces that phase (`p`). So each effect becomes a
**track**:

- **One input:** `p` = year phase ∈ [0,1) (already exists).
- **A handful of authored curves**, each a deterministic window over `p`.

| effect | winter (0–.25) | spring (.25–.5) | summer (.5–.75) | autumn (.75–1) |
|---|---|---|---|---|
| **snow**              | ramp up → hold | melt → 0 | 0 | 0 |
| **rain**              | 0 | showers (med) | dry (low) | showers (med) |
| **leaf-fall** (event) | 0 | 0 | 0 | **peak** |
| **tree / grass cover**| bare / low | growing | full | shedding |
| temperature | derived readout only (tint / HUD) — no longer gates anything |

### What this buys us
- **Proper named periods** — read the table, know exactly when it snows.
- **Rain ≠ snow** — separate authored tracks, not one gated by the other.
- **Leaf-fall is its own event**, decoupled from leaf density on the trees.
- **7 signals collapse to ~1 input + a few curves.** Temperature survives only as a
  cosmetic readout (tint, HUD).
- **Still testable with variety:** if wanted, add ±10% noise to *when* a shower
  fires *inside* its window — the window stays deterministic.

### Fits what already exists (low effort)
- Reuses `FolioCycle` (keyframe interpolation over `p`) — same mechanism `YearCycles`
  already uses for `leaves` / `temperature` / tint.
- Retires most of `Weather.cpp`'s noise + `remapClamp` derivation math.
- The **tree/grass appear-disappear** system (separate TODO) becomes the same model:
  a `treeCover` / `grassCover` track on `p` driving spawn/despawn — one coherent
  system instead of two.

### Suggested track list (first pass)
- `snow`        — raised-cosine bump centred on deep winter.
- `rain`        — two low humps in spring and autumn; near-zero in summer.
- `leafFall`    — sharp bump in autumn (a *falling* event, not density).
- `treeCover`   — high in summer, shedding through autumn, bare in winter, regrowing in spring.
- `grassCover`  — similar to treeCover, slightly ahead in spring.
- `temperature` — keep as a smooth seasonal curve for tint/HUD only.

Each is a 4-keyframe track (winter/spring/summer/fall stops, like the existing
YearCycles presets), so authoring is just editing numbers in one place.

---

## 5. Migration notes (when we build it)
- Keep the global shader uniforms consumers already read
  (`folio_weather_rain/_snow/…`) so shaders don't change — just feed them from the
  new deterministic tracks instead of the noise pipeline.
- Keep `set_override` for debug/events.
- Fix the logger's `*_pct_of_year` to divide by actual elapsed (or clamp to the last
  full year) so multi-year runs don't report >100%.
- The `--debug` HUD + `env_report.html` plot stay useful for verifying the new
  curves produce the intended periods.
