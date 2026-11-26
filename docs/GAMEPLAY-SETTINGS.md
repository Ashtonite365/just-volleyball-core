# Gameplay settings API (CORE 1.3)

A translation layer for settings that change play: hitboxes, gravity, time scale, drag,
bounciness, spin, rules and the like. A mod registers its settings once during its config
setup. From then on the core:

* saves the values per mod, with a schema version and a migration hook,
* builds the controls in the game's own **Settings** panel (a `mods` tab, section *gameplay*),
* answers every read through one hook, `gameplay_get`, which applies the lobby rules:

| where you are | role | what `gameplay_get` returns |
|---|---|---|
| Free Play, practice, offline solo, menu | `local` | your value (live) |
| LAN / private / friends lobby, you host | `local` | your value; everyone else uses it |
| LAN / private / friends lobby, not host | `client` | the host's value (stock until it arrives); controls greyed out |
| public lobby, quick match, server browser, unknown | `off` | stock (the default); hooks are not consulted |

Every value is validated and clamped on the way in and on the way out. Visual-only settings
(colours, trails, poses) are **refused**: cosmetics stay per-player and never go through here.

Capabilities: `gameplay.settings` (register, read, set; access gameplay, host scope),
`gameplay.settings_sync` (the host-sync hooks, mod lists and the match decision; access sync) and `game.rules`
(the rally rules, see [GOOFY-RULES.md](https://github.com/Ashtonite365/just-volleyball-mods/blob/main/goofy/docs/GOOFY-RULES.md)). Function reference:
[CORE-API.md](CORE-API.md).

## Registering a setting

A complete script mod (`mods/api_mods/low_grav/mod.gd`) that adds two settings to the
Settings tab and uses them:

```gdscript
extends Node
const MOD := "low_grav"
var api: Object
var gravity_mul := 1.0   # what this mod's own gameplay code uses

func _ready() -> void:
	api = Engine.get_singleton("JVModAPI")
	api.register_mod(MOD, "1.3", PackedStringArray(["gameplay.settings"]))
	var r: Dictionary = api.gameplay_register(MOD, [
		{"id": "low_grav.gravity", "type": "float", "default": 1.0, "min": 0.1, "max": 3.0, "step": 0.05,
		 "label": "moon gravity", "tooltip": "x ball gravity (1 = stock)", "category": "low grav",
		 "sync": "gameplay", "on_change": _on_gravity},
		{"id": "low_grav.mode", "type": "enum", "options": ["calm", "wild"], "default": "calm",
		 "label": "mode", "category": "low grav", "sync": "gameplay"},
	], {"version": 2, "migrate": _migrate})
	if not r.ok: push_warning(str(r.errors))
	_on_gravity("low_grav.gravity", api.gameplay_get(MOD, "low_grav.gravity"), "local")

# called with the EFFECTIVE value: yours offline / as host, the host's on a client, stock in public
func _on_gravity(_id: String, value: Variant, _source: String) -> void:
	gravity_mul = value

# schema 1 stored gravity as a percentage
func _migrate(from: int, values: Dictionary) -> Dictionary:
	if from == 1 and values.has("low_grav.gravity"):
		values["low_grav.gravity"] = float(values["low_grav.gravity"]) / 100.0
	return values
```

From C++ the same goes through `api->call("gameplay_register", mod, defs, options)`
(Goofy does this in `src/goofy/goofy_settings.cpp`).

### Definition fields

| field | required | meaning |
|---|---|---|
| `id` | yes | `<mod>.<name>` (lowercase, digits, `_`, dots inside). Only that mod may register or set it. |
| `type` | yes | `bool`, `int`, `float`, `enum`, `vec` (2-4 numbers: Vector2/3/4) |
| `default` | yes | the stock value; must be inside the range |
| `min` / `max` / `step` | numbers | range (per component for vec) and snap step |
| `options` | enum | option names; values are read and written by name |
| `label`, `tooltip`, `category`, `order` | no | how the row looks and where it goes in the tab (one subheader per category) |
| `sync` | yes | must be `gameplay`. `visual` / `cosmetic` / anything else is refused. |
| `on_change(id, value, source)` | no | called when the effective value changes (edit, host values, role change). Not on first sight. |
| `on_read(id, value, source) -> value` | no | read translation; its result is clamped again. Skipped while the role is `off` (fails closed to stock). |

Options: `version` (int schema, default 1), `migrate(from, values) -> values` (runs when the
stored schema is older; `from` is 0 when nothing was saved yet, which is where a mod imports
its old config), `on_change` (mod-wide hook), `mod_version` (the version string used in mod
lists; default: the schema number).

### Reads, writes, state

* `gameplay_get(mod, id)` is the only read path. Any mod may read any setting.
* `gameplay_set(mod, id, value)` / `gameplay_reset` change the local value of the mod's own
  setting (the Settings tab uses the same path). Refused on a client in a synced lobby.
* Saved to `mods/api/state/gameplay/<mod>.json` as `{"version": N, "values": {...}}` with an
  atomic write. Defaults are not stored. Values that no longer validate fall back to the default.
* Signal `gameplay_setting_changed(id, value, source)` and bus events
  `gameplay_setting_changed`, `gameplay_role_changed`, `gameplay_mods_status`.

## Settings tab

The core watches for the game's `SettingsPanel` and adds a `mods` tab after the stock tabs,
built with the panel's own row builders (`_subheader`, `_check`, `_range`, `_select`), so the
rows look like the game's. The open tab is not changed, and the tab comes back when the game
rebuilds the panel. On a client in a synced lobby the rows are greyed out and show the host's
values; in a public lobby they show your values with a note that everyone plays stock.

## Host sync hooks (for the host-sync library)

The registry is the single source of host-synced gameplay values. It does not send anything
itself; a sync library carries the payloads. All of these need `gameplay.settings_sync`,
which is open only in LAN / private / friends lobbies.

In a lobby, in order:

1. every player sends `gameplay_manifest_bytes(mod)` (their gameplay mod list) to the host,
   which calls `gameplay_set_peer_manifest(mod, player_id, bytes, name)` for each,
2. at match start the host calls `gameplay_match_start(mod)`, then sends everyone its own
   `gameplay_manifest_bytes(mod)` (now carrying the match decision) and
   `gameplay_snapshot_bytes(mod)`,
3. each client calls `gameplay_set_host_manifest(mod, bytes)` and
   `gameplay_apply_snapshot(mod, bytes)`,
4. at match end the host calls `gameplay_match_end(mod)`. Call `gameplay_remove_peer` when a
   player leaves and `gameplay_clear_host` when the host changes.

* Snapshots: `JVGS 1` then `id=value` lines, at most 32 KiB / 1024 entries;
  `gameplay_snapshot(mod)` also gives an FNV-1a 64 `hash` to skip unchanged sends.
* A snapshot is complete: settings the host did not send read as stock. Unknown ids are
  ignored, values are clamped to the client's own ranges, malformed payloads are rejected whole.

## Missing mods (match-wide)

If a player does not have a gameplay mod, the host turns it off for the whole match and
everyone follows.

* **Mod list.** Each player's list holds every mod with registered gameplay settings or a
  gameplay / sync / offline capability, with its version. Texture, model, loader and core UI
  libraries are never in it, so this rule never turns them off.
* **Decision at match start.** `gameplay_match_start` checks every player whose list the host
  has. A gameplay mod that any of them lacks, or has in another major version, is **off for
  everyone in the match, the host included**:
  * its settings read stock everywhere and are left out of the snapshot,
  * the core refuses its gameplay calls (reverting is still allowed),
  * Goofy turns itself fully off (back to stock) while it is disabled,
  * `gameplay_mod_active_for` answers false for it, for every player.
* **Locked for the match.** Settings cannot be edited between `gameplay_match_start` and
  `gameplay_match_end` (the Settings tab is greyed out). A player who joins mid-match does not
  change anything; their list counts at the next match start.
* **Clients** follow the host's decision (the `!mod` lines in its list). A mod the host does not
  have, or has in another major version, is also off. Until the host's list arrives,
  `gameplay_mod_active_for` answers false (fail closed).
* **Shared check.** `gameplay_mod_active_for(mod, mod_id, player_id = "")` is the one function
  every gameplay feature uses; the answer is the same for every player in the match.
  `gameplay_match_status(mod)` gives the decision (`disabled`: mod -> who lacks it, `active`,
  `locked`, and before match start on the host `would_disable`).
* **Status.** `gameplay_player_mods(mod)` returns, per player, their mods, `missing` (the host's
  mods they lack, with why), `late` and `loader_ok`. Each step is logged and fires
  `gameplay_mods_status`.
* **Notices.** At match start the host gets an on-screen notice naming each disabled mod and
  who is missing it. A player whose missing mod caused a disable gets a notice when the host's
  list arrives ("you do not have X: the host turned it off for everyone this match").
* **Texture loader warning.** Some courts and map packs carry custom gameplay values that only
  the texture loader (`jvm_loader`) reads. When a player joins a LAN / private / friends lobby
  and their loader is not running, they get an on-screen notice. When their list reaches the
  host with `loader=0`, the host gets one naming them. Both are also logged. This does not
  disable anything.

## Goofy settings assessment

`tools/gameplay_assessment.py` classifies every Goofy setting (the 156 ball + 195 player library
params plus Goofy's own config keys):

```
351 library params + 11 config keys = 362
  ported: 11
  movable: 312
  needs_work: 9
  visual_only: 30
  library params only: {'ported': 4, 'movable': 311, 'needs_work': 6, 'visual_only': 30}
```

* **ported**: registered by Goofy now (below).
* **movable**: a plain number that changes play; it can be registered as-is.
* **needs work**: changes play but needs a decision or code first.
* **visual only**: stays per-player, never registered.

| setting | class | note |
|---|---|---|
| `big_ball` | ported | goofy.ball_size |
| `ball_physics.gravity_scale` | ported | goofy.gravity_scale |
| `ball_physics.bounce_scale` | ported | goofy.bounce_scale |
| `time_scale` | ported | goofy.time_scale |
| `max_hits_per_team` | ported | goofy.rules.max_team_touches |
| `no_double_hit` | ported | goofy.rules.double_hit_foul (inverted) + goofy.rules.max_consecutive_touches |
| `PlayerFeel.block_classic` | ported | goofy.rules.block_classic |
| `player_scale` | ported | goofy.player_scale |
| `ball_physics.<any property>` | needs work | free-form property names: register per property or keep as config |
| `overrides` | needs work | generic node/property overrides (targets + any property): not a single typed value |
| `court pack goofy layers` | needs work | per-court layers (space gravity, underwater drag): stay pack layers, their effective values are host-synced by Goofy |
| `ball.radius` | ported | goofy.ball_size (multiplier) |
| `ball.gravity` | ported | goofy.gravity_scale (multiplier) |
| `ball.net_half_width` | needs work | copied by the game at match start: a change applies from the next match (needs an apply-at-start hook) |
| `ball.antenna_half_width` | needs work | copied by the game at match start: a change applies from the next match (needs an apply-at-start hook) |
| `ball.trail_min_speed` | visual only | trail look |
| `ball.outline_home` | visual only | colour |
| `ball.outline_away` | visual only | colour |
| `ball.outline_contested` | visual only | colour |
| `ball.contest_band` | needs work | filed under visuals but feeds the net-contest rule (rules.gd:153/236, player.gd:3978): reclassify as gameplay |
| `ball.contest_min_vz` | needs work | same as contest_band (ball.gd:1843 net_contested_for) |
| `ball.trail_fast` | visual only | colour |
| `ball.trail_fast_speed` | visual only | trail look |
| `flight.air_drag` | ported | goofy.air_drag |
| `flight.curve_scale` | ported | goofy.curve_scale |
| `player.charge_shake` | needs work | shakes the serve hold point (player.gd:3382), not only the pose: confirm the ball is not moved before registering |
| `player.charge_shake_hz` | needs work | same as charge_shake |
| `player.crouch_squash` | visual only | pose / animation only |
| `player.launch_stretch` | visual only | pose / animation only |
| `player.launch_charge_gain` | visual only | pose / animation only |
| `player.land_pop` | visual only | pose / animation only |
| `player.launch_spring` | visual only | pose / animation only |
| `player.launch_damp` | visual only | pose / animation only |
| `player.hand_mesh_radius` | visual only | pose / animation only |
| `player.gaze_near` | visual only | pose / animation only |
| `player.gaze_far` | visual only | pose / animation only |
| `player.pose_speed` | visual only | pose / animation only |
| `player.recoil_kick` | visual only | pose / animation only |
| `player.recoil_stiff` | visual only | pose / animation only |
| `player.recoil_damp` | visual only | pose / animation only |
| `player.air_lean` | visual only | pose / animation only |
| `player.dump_tilt` | visual only | pose / animation only |
| `player.receive_anim_speed` | visual only | pose / animation only |
| `player.receive_pose_min` | visual only | pose / animation only |
| `player.receive_pose_max` | visual only | pose / animation only |
| `player.set_pose` | visual only | pose / animation only |
| `player.indicator_color` | visual only | colour |
| `player.dive_ring_color` | visual only | colour |
| `player.stamina_empty_color` | visual only | colour |
| `player.ring_jump` | visual only | colour |
| `player.ring_serve` | visual only | colour |
| all other library params | movable | animation 5, block 24, body 1, bounce_collision 51, contact_timing 33, dive_fall 25, drag 1, hit_range 18, hitbox 10, jump 17, movement 24, net_geometry 2, physics 3, player_physics 4, serve 31, shot_shaping 14, speed_limits 14, spin_curve 14, sprint_stamina 20 |

### Ported (registered by Goofy, schema 2)

Goofy's settings schema is 2; `mod_version` stays `1` so a peer still on schema 1 (build 2614edd) is the same major in the lobby list.

| id | type | default | range | replaces |
|---|---|---|---|---|
| `goofy.gravity_scale` | float | 1 | 0.05 .. 5 | `ball_physics.gravity_scale` |
| `goofy.bounce_scale` | float | 1 | 0 .. 3 | `ball_physics.bounce_scale` |
| `goofy.air_drag` | float | 0 | 0 .. 5 | `ball_params` `flight.air_drag` |
| `goofy.curve_scale` | float | 1 | 0 .. 5 | `ball_params` `flight.curve_scale` (spin) |
| `goofy.ball_size` | float | 1 | 0.25 .. 6 | `big_ball` (real hitbox) |
| `goofy.player_scale` | float | 1 | 0.25 .. 4 | `player_scale` (player body scale) |
| `goofy.time_scale` | float | 1 | 0.25 .. 2 | `time_scale` |
| `goofy.rules.double_hit_foul` | bool | on | | `no_double_hit` (inverted) |
| `goofy.rules.max_consecutive_touches` | int | 1 | 1 .. 10 | new |
| `goofy.rules.max_team_touches` | int | 3 | 1 .. 10 | `max_hits_per_team` (0 = 3) |
| `goofy.rules.block_classic` | bool | off | | `PlayerFeel.block_classic` |
| `goofy.topspin_spike_boost` | float | 1.12 | 1 .. 1.3 | new (v1.0.4): x speed of a topspin SPIKE made in the air with forward held; read by the shots library (see [SHOTS.md](https://github.com/Ashtonite365/just-volleyball-mods/blob/main/shots/docs/SHOTS.md)) |

Old config files keep working. On the first run the migration (schema 0 to 1) imports those
keys from `mods/goofy/*.json` (`{"mul": x}` forms are resolved against stock); schema 2 then
imports `player_scale`. The file is not changed. Later edits of the same keys in the JSON are
still picked up and written to the registry, so a JSON edit and a Settings tab edit both work.
In the Mods panel, ported fields apply live through the registry as you change them (same as
System Settings > mods); overrides and other unported JSON still need Save. The old code paths
for these keys are disabled while the registry is in use, not removed: with a core older than
1.3 Goofy uses them as before. Court pack layers still stack on top (they are per-court, not
settings).

`goofy.time_scale` follows the registry like every other setting: a non-host in a LAN /
private / friends lobby runs the host's game speed (stock until the host's values arrive).
