# Allowlist gate

**Fails closed.** Two tiers, both decided every frame by the pure C++ core
`src/gate/gate_logic.{hpp,cpp}` (unit tested) and the `JVModGate` node (`/root/JVModGate`,
process priority -1000, before every mod node):

* **full** tier (shots, goofy, textures): signal `gate_changed(active, reason)`
* **cosmetic** tier (textures, models): signal `cosmetic_gate_changed(active, reason)`

Nothing is ever active until the **version pin** has matched (see LOADING.md).

| mode (status.json) | full | cosmetic (textures) | detected from (recovered game scripts) |
|---|---|---|---|
| `free_play` | ON | ON | menu.gd:4932 Free Play button -> `_start_solo_practice` (4936): `_practice_solo` (170) / `_practice_on` (161); free roam `_freeplay` (107, set by `_enter_freeplay` 11769) - all only while `Net.active` (net.gd:76) is false |
| `practice` | ON | ON | menu.gd `_drill_active` (425) non-empty (drills, `_start_tutorial` 5380), `Net.active` false |
| `offline_solo` | ON | ON | scene root is main.tscn (`_start_local` menu.gd:7650), `Net.active` false and `GameConfig.net_roster` (game_config.gd:1974) empty |
| `lan` | ON | ON | `Net.party_lan` (net.gd:88) or an ENetMultiplayerPeer; vetoed if any ENet peer has an internet address (`multiplayer_peer` probe) |
| `friends` | ON | ON | `Net.custom_lobby` (84) and `Net.privacy` (2720, enum Priv 2719) == FRIENDS, cross-checked with Steam lobby data `priv` (door_label 2728, setLobbyData 2054-2059) |
| `private` | ON | ON | same, privacy == PASSWORD (password or invite-only) |
| `public` | **OFF** | ON (local only, no sync) | privacy == PUBLIC, or `priv`=="public" anywhere. Server-browser-listed games are public custom lobbies |
| `quick_match` | **OFF** | ON (local only, no sync) | `GameConfig.quick_match` + v1.0.4 `GameConfig.ranked_match` (game_config.gd:503, set net.gd:4520 / 5582; ranked search uses the same `_qp`), `Net._lobby_kind` (4523) locked/quick, menu `_member_queuing` (207) or `_qp.state` (quick_play.gd:160) != IDLE |
| `server_browser` | **OFF** | ON (local only, no sync) | only via a generic probe; the game has no separate browser state |
| `menu` | ON | ON (local only, no sync) | idle main menu with nothing running (`Net.active` false, no Steam lobby/peer, no practice/free play flags). Reason `allowed: main menu (local)` |
| `unknown` | OFF | OFF | anything not positively identified: `Net.active` with no lobby kind yet (e.g. Free Play -> joining), custom lobby with unreadable privacy, party only, main scene with a stale roster, missing properties after an update |

Probe implementation: `src/core/mod_gate.cpp` jv_net probe (properties read at
449-455; Quick Play 469-470; practice/free play 487-493; offline solo 498; LAN 508).
Only Godot APIs (`get_node`, `get`, `Steam.getLobbyData`) - never a memory address.

Rules (`decide()`):
* no fresh observation (older than `max_age_s` = 2 s) -> OFF
* any probe reporting public / quick match / server browser vetoes the full tier
  (cosmetic tier skips those vetoes); the idle menu is allowed for both tiers
* known contexts that disagree (e.g. offline play + an online session) -> OFF
* only an **authoritative** probe can turn anything ON
* hysteresis: OFF is immediate (same frame the state changes, measured as 1 frame in
  tests via status.json `changed_frame`), ON needs 0.5 s stable
* kill switch: `mods/DISABLE` file or `mods/jv_mods.json {"enabled": false}`
* the `test` probe type and all `JV_*` env overrides only work in debug builds

Transitions tested (Linux 4.5.2/4.7.2, Windows 4.7.2 under Wine): Free Play -> host public
(full OFF in 1 frame, textures stay ON), Free Play -> joining (everything OFF in 1 frame while
the lobby is unknown), private -> public (full OFF in 1 frame), public -> private.

Mod libraries revert everything they changed when their tier turns OFF; with no gate
node they stay OFF.
