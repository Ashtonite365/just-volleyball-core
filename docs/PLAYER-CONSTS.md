# Player constants (player.gd `const`) - why they are not editable, and the bypass

Status: implemented (capability `player.consts`, IMPL, API 1.2). Pure rules in
`src/gate/player_consts.*`, boot rewrite in `src/core/player_consts_boot.*`, Goofy wiring in
`src/goofy/goofy_consts.cpp`.

> **HARD REQUIREMENT - every player needs player.consts.**
> Edited player consts only work when **every** player in the match has the mod with
> `player.consts` active, on the same game build as the host. The values are **match-wide** (one
> set for everyone). At match start the host checks every player. If **anyone** lacks it (not
> installed, `player.consts` unavailable on their machine, or a different game build), the host
> turns the edited values **off for the whole match: everyone plays the stock values**, and the
> host gets a notice naming who is missing it. Otherwise the host's frozen set applies to everyone.
> A player without the mod always simulates the stock values locally.
> Texture / cosmetic mods are never turned off by this.
> Settings tooltip: "Needs player.consts on every player. If anyone in the match doesn't have it,
> everyone plays the stock values (the host is told who). Edit in the lobby only - locked during a match."

> **Lobby only - frozen for the match.** Values can be edited in the menu / lobby only (CORE API
> `player_consts_set` / `player_consts_reset`, Goofy config `player_consts`, the menu editor rows).
> At match start they are locked in and the host sends its set to every player before play. Until the
> match ends (back to lobby / menu) every write - API call, Goofy UI, config reload, sync packet - is
> rejected: the value stays unchanged, a warning is logged and the API returns `ok=false` + `error`.
> The settings rows are greyed out while a match runs. At match end every value goes back to the
> exact stock literal (your saved edits are kept for the next match).

## Why GoofyLib cannot edit them today
- The game ships `res://game/player/player.gd` only as compressed binary tokens
  (`player.gdc`, header `GDSC` v101 + zstd, 156,187 B). There is no source text in the pck
  (`has_source_code() == false`, `source_code == ""`).
- Godot 4 has no bytecode format: every load re-parses those tokens. The analyzer turns each
  use of a `const` into its literal value and the compiler copies that literal into the
  constant pool of every function that uses it. No variable or property is left to write to.
- The script API gives no way in either: `get_script_constant_map()` returns a copy, and
  `GDScript.set("<CONST>", v)` is ignored, with no error.
- Other scripts that read `Player.<CONST>` fold the value into their own code too
  (for example `NET_FLOATS` in net.gd, net_sync.gd, replay_data.gd, menu.gd and own_balls.gd).
- `GDScript.reload()` after setting `source_code` returns OK but recompiles the stored tokens,
  because binary tokens take priority over `source_code`. So an in-place text replacement does nothing.
- By contrast, everything Goofy edits today is a `static var` (`FEEL`, the `*_rule` switches),
  which is a real live slot.

## Classification (143 top-level/inner consts)
Gameplay (34, all read only inside player.gd except BLOCK_TAPE_SLACK, also read by the debug
hitbox view):
- COMBO_SPIKE_BONUS, FLOAT_BOT_PACE, SPIKE_RECOVER
- SPRINT_CHARGE_GRACE, JUMP_ORPHAN_S, JUMP_SPIKE_GRACE_US, BAM_NET_SLACK
- SPIKE_TURN_FREE, SPIKE_TURN_FULL, SPIKE_TURN_FLOOR, SPIKE_TURN_SLOW, SPIKE_TURN_FAST,
  SPIKE_TURN_SLOW_COST, SPIKE_TURN_RATE_BLEND
- DIVE_HIT_SHARE, SIDE_DIVE_DEG, DIVE_NET_MARGIN, DIVE_NET_STOP, NET_POLE_X, NET_POLE_REACH, IN_PLAY_DY
- FRONT_ZONE_GRACE, SET_DUMP_SHORT, SET_DUMP_MIN_DEPTH, SET_DUMP_RISE_TAP,
  SET_DUMP_RISE_FULL, SET_POCKET_DEPTH, SET_SELF_ZONE, SET_MIN_DEPTH, SET_APRON, BACK_SET_DOT
- BLOCK_TAPE_SLACK, BLOCK_TURN
- SET_AUTO_LOOKAHEAD (v1.0.4, build 26924091: charged-set auto-fire lookahead, 2.0). v1.0.4 removed
  DUMP_PUSH_BUFFER (0.3); its replacement DUMP_PUSH_HOLD (0.15, dumps need forward held) is read by
  net_sync.gd (`Player.DUMP_PUSH_HOLD`, folded), so it is a protocol const and stays `const`.

Leave as they are (protocol or shared IDs; changing them would break netcode or other scripts):
- NET_FLOATS (the snapshot float layout), DUMP_PUSH_HOLD (v1.0.4 input `dh` clamp in net_sync.gd)
- CALLOUT_* IDs, QUICK_CHAT, QUICK_CHAT_ACTIONS, CALL_VARIANTS, CALLOUT_GESTURES, CALL_SLACK_MS
- HIGH_FIVE_ON

Visual, animation, audio or UI (they could be offered later as per-player cosmetics, never host-synced):
- poses and recoil (POSE_*, CEL_HF_*, RECOIL_DIR)
- hand reach IK (REACH_*)
- _MIN_ACTION_HOLD, TOSS_CANCEL_SPEED, GESTURE_*, HIGH_FIVE_HOLD
- DIVE_DENY_TIME (only the shake and fade timer of the dive-denied indicator)
- callout text, sounds and pitches, colours, shaders, icons, badges, flashes, cursor ring and contact shadow
- chat rate limits (CALLOUT_BURST, CALLOUT_BURST_WINDOW_MS, CALLOUT_COOLDOWN_MS)

Literal numbers written inline in code (for example `_recover_left = 0.25`) are not consts and are
not covered by any option.

## Options, ranked by safety
1. **Token rewrite + overlay pck at boot (implemented).** Before anything loads player.gd,
   CORE (`JVPlayerConsts.prepare_at_boot`, called from `bootstrap.gd` `_init`) reads the game's own
   `res://game/player/player.gdc` and decompresses it. It then rewrites `const NAME` to
   `static var NAME` (same literal, one value for the whole match) for the 34 gameplay names: the
   `const` token becomes `static var`, line/column maps shifted. Token ids are learned from the file itself
   and every literal is checked against the expected stock value. CORE packs the result into a cache
   pck under user:// and mounts it with `load_resource_pack(..., replace_files=true)`. The game
   then compiles *its* Player from that buffer, so the script identity, `class_name Player`, every
   `is Player` check and every other script stay as they are; the three static helpers and the
   hitbox view's `Player.BLOCK_TAPE_SLACK` read the live value too. Covers all 34.
   (A per-body `var` variant was prototyped and dropped: the values are match-wide by design.)
2. **Subclass override plus PackedScene swap.** A script `extends player.gd` overrides only the
   functions that use a const. It is swapped into `player.tscn` through `_bundled` on the cached
   scene, and swapping back reverts it. This works, but it covers only 26 of the 34. The other 8 sit
   inside `_do_touch`, `_move`, `_try_armed_receive`, `_set_contact` and `_turn`, which run 110 to 455
   lines each. It also copies about 280 lines of game logic into the mod, and that copy drifts
   silently when the game updates. It changes `get_script()` and the resource path. Static overrides
   only apply through an instance (`Player.spike_turn_keep()` stays stock), and hitbox_view keeps
   the folded value.
3. **Value compensation through FEEL.** This never matches exactly. At most 3 consts can be
   approximated (SPRINT_CHARGE_GRACE via jump_charge_time, BLOCK_TURN via turn_speed, BAM_NET_SLACK
   via bam_delay/bam_window), and each one has side effects on other mechanics.
4. **Runtime source replacement.** Not viable. An in-place `reload()` ignores source text (see above).
   A new script object breaks Player identity: the global class conflicts and about 120 `is Player`/type
   sites stop matching. It would also mean shipping the decompiled script.
5. **Patching the compiled constants in memory.** Unsafe, so it was not prototyped. It needs private
   engine struct offsets in a symbol-less custom build. The literals are copied, and may be
   deduplicated, in each function's constant pool, so one patch could change unrelated literals with
   the same value. There is also no safe way to revert it.

## How it fits CORE (option 1)
- **Capability:** `player.consts` (GAMEPLAY, IMPL). CORE API: `player_consts_get`,
  `player_consts_set`, `player_consts_reset`, `player_consts_locked` (docs/CORE-API.md), forwarded
  to Goofy. `jvm::pconsts::Lock` owns the rules below. Goofy writes the script's static vars through
  the API (`io_set`, capability-checked) only twice per match: the frozen set at match start, the
  stock literals at match end.
- **Match detection:** the current scene has a non-null `_rally` and is not the menu's embedded
  practice court. Checked every 0.1 s, so the set is live before the first serve.
- **Who has it (adapter):** each machine advertises the gameplay mod id `manifest_id(pin)` =
  `player.consts@<matched version pin>` in its Goofy sync hello, only after the overlay mounted and
  verified (`JVPlayerConsts.check`). The host compares every connected peer with its own id: no id =
  missing, another id = other game build. When CORE has the shared "is mod active for player X"
  check, `gameplay_mod_active_for(mod, mod_id, player_id)` (gameplay settings registry, CORE 1.3),
  the host asks it first whether the player has Goofy (same major version), then checks the
  player.consts id: both must pass. A CORE without that function (this branch) uses the id check
  alone. Either way the result feeds the same all-or-nothing rule. Bots belong to the host.
- **All or nothing:** `begin_match_host` freezes the saved values. If every player is active the plan
  is `apply=1` with the edited values; if anyone is not, the plan is `apply=0` (stock for everyone)
  and the host gets the notice, e.g. "Player consts are OFF this match (everyone plays stock):
  Bob (missing) doesn't have player.consts". The notice is logged, emitted as Goofy's
  `player_consts_notice` signal and shown through the API notify hook when present.
- **Start-of-match distribution:** the plan is sent once to every modded client as data only:
  `JVPC 1`, `match=<id>`, `apply=<0|1>`, then `NAME=value` lines (none when `apply=0`). Strictly
  parsed: known names only, finite values, clamped, no duplicates, at most 16 KB, host only. A
  client accepts exactly one plan per match, whether it lands just before (kept up to 15 s) or just
  after its own match start; until then it simulates stock. A plan for the host's next match that
  arrives while the client is still finishing the previous one is kept for that next match. Any
  other plan or value packet during the match is rejected and logged.
- **Contexts:** LAN / private / friends: the host's set (all or nothing as above). Public lobbies /
  quick match: everyone stock. Offline / CPU: your own edits, still locked during the match.
- **Fails closed.** CORE mounts nothing, and the game runs stock (player.consts is not advertised, so
  the host treats you as missing and the whole match plays stock), in any of these cases:
  - `mods/player_consts.json` has `{"enabled": false}`,
  - the version pin does not match,
  - the tokenizer version is not 100 or 101,
  - a name is missing, declared other than once, or its literal is not the expected stock value,
  - the round trip of the unmodified buffer is not byte-identical,
  - player.gd was already loaded when the hook ran.
- **Revert:** `end_match` writes the exact stock literals for all 34. Turning the feature off
  skips the mount on the next launch.
- **Timing:** this runs in `bootstrap.gd` `_init`. By the time `_ready` runs on a prepended
  autoload, the game's autoloads have already loaded player.gd. bootstrap.gd must never name a game
  class (compiling it would load player.gd too early).

## Tests
- Unit (`tests/test_player_consts.cpp`): lobby edits and clamping; mid-match writes rejected with
  the value unchanged; all-or-nothing (anyone missing or on another build -> stock for everyone +
  notice naming them); distribution to a client, second packet / non-host packet rejected, early
  plan kept, stale plan dropped, next-match plan kept; strict decoding; exact revert at match end
  and unlock; public and offline matches.
- Integration (`JV_TEST_MODE=pconsts tests/run_integration.sh`): lobby edits through config, Goofy
  and the CORE API; match start freezes and applies; mid-match writes rejected (Goofy, CORE API,
  config reload); match end restores all 34 exactly; the next match uses the new lobby set.
- Net pairs (`tests/run_net_pairs.sh`, `pc-*`): all modded -> the host's set reaches the client and
  the client's own edits are ignored; unmodded client, client with player.consts unavailable, client
  on another build, and a three-player match with one unmodded client -> everyone stock and the host
  notice names exactly the player(s) missing it.
