# Stage and ball pickers

The main-menu bottom-bar pills (`stage: …`, `ball: …` in `ui/menu.gd`) list the
game's own options plus every matching resource pack found at runtime under
`mods/resources/packs/`. Nothing about a pack is compiled in: drop a new folder
in and it appears as a tile the next time the picker opens (no code rebuild).

Selections are stored in `mods/resources/packs/picker.json`:

```json
{ "stage": "1005", "ball": "1006" }
```

An empty string (or a missing key) means "the game's own pick". The file is
separate from `equipped.json` so outfit skins are not disturbed. If
`picker.json` is missing, `stage` / `ball` keys on `equipped.json` are still
read as a fallback.

Custom picks are **client-side only**. They are never sent as a game skin or
stage id, so other players keep seeing the vanilla ball and court. The pills
themselves live on the main menu / Free Play court (the same place the game's
own pills apply). The lobby StagePicker used for matches is not given custom
tiles, because a custom stage id would not resolve for other players.

## Pack format

`skin.json` may declare `"type": "stage"` or `"type": "ball"`. If `type` is
omitted it is inferred: a `court` block means stage, a `ball` block means ball.
`kind: "global"` on those packs is accepted but they are **not** always-on —
they only apply when selected in the picker. Net / texture-replacement globals
without a ball or court block stay always-on.

```json
{
  "id": "1006",
  "name": "Galaxy Tri-Color Ball",
  "label": "Galaxy",
  "type": "ball",
  "preview": "preview.png",
  "ball": {
    "albedo_tex": "ball_tex.png",
    "albedo_color": "#ffffff",
    "wrap": "equirect"
  }
}
```

```json
{
  "id": "1005",
  "name": "Royal Court & Autumn Grass",
  "label": "Royal Court",
  "type": "stage",
  "preview": "preview.png",
  "base_stage": "grass",
  "court": { "floor_tex": "court_floor.png", "grass_tex": "autumn_grass.png" }
}
```

| key | meaning |
|---|---|
| `type` | `stage` or `ball` (or inferred from the `court` / `ball` block) |
| `name` | full name; used if `label` is absent |
| `label` | short name on the tile and the bottom-bar pill |
| `preview` | pack-relative image for the tile (default `preview.png`) |
| `base_stage` | stage packs: the game stage loaded underneath (default `grass`) |
| `variants` / `tod` | optional names for day/evening/night; otherwise the base stage's variants are used |
| `ball` | look-only. Known fields: `albedo_tex` (or `tex`), `albedo_color`, `color_a`, `color_b`, `shade`, `wrap` (e.g. `"equirect"`). Extra fields are kept and ignored by the current apply path. |
| `court` | look-only court / grass colours and textures (no collision change) |

The game's balls in the picker keep the game's own ids and labels:

| pill label | game id (`menu.gd` `BALL_SKINS`) | thumbnail |
|---|---|---|
| pro | `nationals` | `res://game/images/gameballs/ball_nationals.png` |
| school | `school` | `res://game/images/gameballs/ball_school.png` |
| outdoor | `beach` | `res://game/images/gameballs/ball_outdoor.png` |

Picking a game ball does exactly what the old cycle-pill did (`set_ball_skin`,
`Net.send_party_ball`, `GameConfig.menu_ball`). Picking a custom ball leaves
that game skin in place as the mesh and applies the pack's look on top. Apply
goes through one function, `JVTextureLoader.apply_picked_ball` (called via
`set_picked_ball` / `apply_picked_cosmetics`), so a later wrap/equirect
renderer can replace just that path. Physics and hitbox are never changed;
goofy `big_ball` scaling still applies on top.

## Pack skies and environments

A stage pack may include a `sky` block (distant panorama only — not a stand-in
for close scenery) and an `environment` block that instances a real 3D scene
around the court:

```json
{
  "sky": {
    "panorama": "sky.png",
    "energy": 1.0,
    "ambient": "#6a88b0",
    "fog_color": "#101018",
    "fog_density": 0.012
  },
  "environment": {
    "mesh": "env.glb",
    "position": [0, 0, 0],
    "rotation": [0, 0, 0],
    "scale": 1,
    "hide_env_props": true,
    "hide_ground": false,
    "hide": ["Gym walls", "Ceiling beams"],
    "baked_lighting": true,
    "sway": false,
    "lod": [
      {"mesh": "env.glb", "visible_end": 40},
      {"mesh": "env_lod1.glb", "visible_begin": 35}
    ]
  }
}
```

`sky` swaps the stage `WorldEnvironment` to a `PanoramaSkyMaterial`. It is for
the far sky only; close hills, halls, terraces, kelp and so on belong in
`environment`.

`environment` loads the pack-relative glTF/GLB under a `JVM_CourtEnv` node and
removes it when the pack is cleared. `hide_env_props` hides kit branches
(`Hills`, `Trees`, `GrassField`, beach `WorldFill` / `Coast` / `City`, …).
`hide_ground` hides `EnvFloor` by `set_visible(false)` only — it does **not**
change the real floor collision, which is an analytic horizontal plane at
`y = 0` (the ball lands at `y = PHYS.radius`). Packs that hide the visual
ground must still show a visible floor over the full playable area out to and
past the force wall (`Stage.PLAY_BOX` = `(15.5, 14.0, 26.0)` half-extents,
superellipse `Court.DOME_N = 4`). `hide` lists extra kit node names (e.g. a
stock gym roof). `baked_lighting` enables vertex-color albedo on the loaded
meshes. Env props have realtime shadows forced off. Optional `lod` entries set
`visibility_range_begin` / `visibility_range_end` on each mesh. `sway` (or
node names containing `sway` / `kelp` / `grass`) applies a look-only vertex
shader.

Geometry in the environment must stay a clear margin (`0.75 m`) off the
force-wall surface, except a thin ground slab at `y ≈ 0`, which may extend
through the vertical wall (it is the walking surface). Geometry may hang over
the court only when it is entirely above the wall (`y ≥ 14.75`, e.g. a high
ceiling or hanging lamp). Env meshes are visual only — they do not add
collision. The visual floor must match the real collision plane (no z-fighting,
no gaps, no void inside the playable area).

A pack test (`tests/court_clearance.py`, also run from `run_integration.sh` in
pickers mode) fails if any non-floor geometry intersects or touches the
barrier, or if visible floor coverage is smaller than the playable area out to
and past the barrier.

Court-specific gravity or drag is not applied here. That comes through the
Goofy API `goofy` block via this unified loader.

## UI

The stage picker is the game's own `StagePicker`. Custom stage tiles are
appended after the last game tile (after Outer Space in Free Play). Selecting
one loads `base_stage` (so day/evening/night still work) and then applies the
pack's court look. The banner title is the pack label.

The ball picker is a new panel (`JvmBallPicker`) built to match StagePicker:
dim overlay, card, banner, 4-column 200×86 tiles, yellow selection ring,
yellow **done** button, Esc / click-outside to close. It uses the game's
`UiStyle` (fonts, `card_box`, `style_button`, sounds).
