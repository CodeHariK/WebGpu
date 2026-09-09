# Sky & atmosphere (`src/sky/`)

Textureless, stylized day / dusk / night — no HDRI, nothing Earth-like.

## `SkyCycle` (extends WorldEnvironment)

One node owns the whole atmosphere. Add it in place of the scene's WorldEnvironment; it finds the first
sibling `DirectionalLight3D` (or `sun_light` path) and drives everything from `time_of_day` (0 = midnight,
0.25 = sunrise, 0.5 = noon, 0.75 = sunset), advanced by `day_length` seconds (0 = frozen).

Each frame `_apply()`:
- arcs `sun_dir` on a tilted circle, `moon_dir` opposite; picks `day` / `night` / `dusk` weights from the
  sun altitude;
- feeds the sky shader: interpolated `zenith` / `horizon` (night → dusk → day palettes; dawn reuses dusk),
  the horizon `glow` near the sun at low sun, `sun`/`moon` discs + halos, `moon_vis`, `star_amount`;
- sets the DirectionalLight3D: along the sun by day (warm, `sun_energy`, tinted toward dusk near the
  horizon), along the moon after dusk (`moonlight_color`, dim `moon_energy`) — so the toon terrain and
  props swing into night automatically;
- sets the Environment ambient (muted sky colour, dimmer/bluer at night) and enables glow for the halos.

The sky shader (embedded in sky_cycle.cpp, `shader_type sky`): a banded vertical gradient (`bands`
posterization) with ground below; horizon glow toward the sun's azimuth; hashed twinkling star points at
night; a flat sun disc + halo; and an **invented moon** — a coloured orb (any colour; gold/teal by
default, not our Moon) with a soft halo and an optional flat `moon_ring`. Everything is a uniform the
driver sets, so nothing about the look is hard-coded to real-world values.

Tweak in the inspector: the three phase palettes (`day/dusk/night` × `zenith/horizon`), `bands`,
`sun_color`/`sun_size`, `moon_color`/`moon_size`/`moon_ring`, `star_color`/`star_density`,
`moonlight_color`, `sun_energy`, `moon_energy`. Demo: the scene's `WorldEnvironment` is a `SkyCycle`
(`day_length = 180 s`, teal moon).

### Night darkness

Night crushes the ambient to near-black cold (`night_ambient` × `night_ambient_energy`), dims the moon
light (`moon_energy`), and turns on dark depth fog (`night_fog_color`, `night_fog_density`) that never
touches the sky — so distance vanishes into black and the terrain reads as silhouettes against the
stars (horror-dark). All three lift back to day values across dusk. Turn `day_ambient_energy` /
`night_ambient_energy` up if you want a softer, less scary night.

## `Flashlight` (extends SpotLight3D)

A hand-torch cone. Parent it under the camera (or the vehicle) — it points where that points — and
press `toggle_key` (F) to switch it on/off. Torch defaults (warm cone, 45 m, shadows) are set in the
constructor and overridable like any spot light. `flicker` adds a failing-battery wobble; `use_battery`
drains `battery` while on and worsens the flicker as it runs low, cutting out at empty. `auto_night`
makes it follow the `SkyCycle` (on at night, off by day, ignoring the manual key) — that is how the
demo's vehicle headlights work (two Flashlights on the front of `TruckController2`, `auto_night = true`).
The handheld torch (under `GameCamera`, `F` to toggle) is the only real light in the dark otherwise.

`SkyCycle::is_night()` and the `sky_cycle` group are the shared hook any light uses to know it is dark.

Later: a second moon / ringed planet, drifting toon cloud bands, aurora at night, weather zones,
`time_of_day` hooks for gameplay (headlights on at night, shop hours).
