# Main-menu UI (core JVMenuUI) - pinned build only, runs regardless of the gameplay gate
* Finds the menu by its own members (ui/menu.gd): `_card_btn` (3448-3523), `_open_credits`
  (7332), `_home` (123), `_bottom_bar` (318, built 2817-2848), `_discord_btn` (210, 2834-2848),
  `_tilt` (227), `ui` (72), `_tint_button` (3006), and the game's `UiStyle` (ui/style/ui_style.gd:
  PRIMARY 22, INK 24, icon 249). Anything missing -> one log line
  "menu UI not changed: <why>", menu untouched; a menu instance is never changed twice.

## Home card order
Vanilla (`_build_home`, menu.gd:3311-3342): online, local, practice, controller config,
system settings, feedback & bug reports, credits.
jv-mods: online, local, practice, **mods**, controller config, system settings,
feedback & bug reports. The mods card is a full-size `_card_btn` like its neighbours (74 px,
subtitle "skins, shots, goofy library & their settings", yellow sticker disc). Every card keeps its own icon/press; `_tilt` is reordered the same way
(the fan depth in `_tilt_cards`, menu.gd:3378-3411, follows it); top/bottom focus neighbours
re-chained, last card -> credits. The game's pad navigation (`_nav_move`, menu.gd:12203:
zones + PAD_NAV.neighbor) finds credits in the bottom-bar zone.

## Credits = stationary yellow bottom-bar button
The vanilla credits card leaned/rotated with the cursor even in the bar (its icon sticker and
offset transform came from the card code). It is removed; instead the Discord button is copied
(`Node.duplicate`, no signals - the copy never opens Discord), renamed "credits", icon
`UiStyle.icon("movie", black)` from the Game Icons set (ui_style.gd:249 - the vanilla credits card's
glyph; `icon_board("movie")` is null because the BoardGame set has no movie.png, which would leave
Discord's logo on the copy), tinted with the menu's own `_tint_button(btn, UiStyle.PRIMARY)`
(the mods card's yellow) with `UiStyle.INK` text, wired to `_open_credits()`, placed right of
discord, not in `_tilt` / `_lean`. Discord itself is not modified. It hides with discord during
matchmaking (menu.gd:6303).

## Mods panel (modal)
* Layout (Geode-style): a narrow left **sidebar** (300 px, vertically scrollable) lists each mod as one row (name + ON / BLOCKED). Clicking a row keeps the sidebar and shows that mod's full content in the large right-hand **detail** pane (description, where it is allowed, state, config/gameplay fields, save / back / close). The card is 1100×600 px minimum and centred. On open, and after Back / Esc from a detail view, no sidebar row is selected and the right pane shows the library overview (title, mode, pack list, dev tools, close). First-open focus is the first sidebar row.
* Opens on the `ui` layer as a full-screen `JvmModsPanel` (mouse_filter STOP backdrop, z 50).
* Modal the game's own way: it is put into the menu's `_guide_page` slot (only used by
  `_open_guide`, menu.gd:7346-7384), so `_modal_up()` (7417) is true: the menu's `_process`
  returns before nav / tilt / menu_start (12514) and `_input` stops eating ui_* (12401). Cards are
  flattened with the menu's `_flatten_cards()` (7435) as `_open_credits` does; `_modal_closed()`
  (7429) on close.
* Focus is trapped inside; Esc / pad B goes detail -> list -> closed; focus returns to the mods card.
* Any menu open - ours, the game's modals (`_modal_up`), friends panel, CPU editor, modifiers
  panel, password panel, stage picker, UiSelect dropdowns, popup windows - keeps the home cards
  flat: no hover pop, no z-raise, mouse ignored (restored on close). Runs after the menu's own
  `_process` (process_priority 1000).

## Per-mod detail view + config editor
* Sidebar: each mod (resource packs, models, shots, goofy) as a button with ON / BLOCKED; the
  selected row stays highlighted. The right pane on the list/overview also has the
  resource-pack list. Click -> detail (right pane): what it does, where it is allowed (tiers), current state +
  gate reason, and its config files as fields: toggles (bool), number boxes, dropdowns (enums),
  text, JSON boxes for lists/nested objects, plus "edit as raw JSON" for the whole file. The
  editor area scrolls vertically. Non-host / locked gameplay fields stay read-only as before.
* Save: every field / raw text is validated (JSON object, per-field JSON, type kept); nothing is
  written if anything is invalid. Then `<file>.tmp` is written and read back, the old file is kept
  as `<file>.bak`, and the tmp is renamed over the file. `_comment` keys are kept.
* Applied live: the library's `reload_config()` (revert + apply) when its tier is open; otherwise
  "applies when this mod is allowed".
* Tier gate: in public lobbies / Quick Match the shots / goofy settings are read-only
  ("settings locked here: <reason>"); resource packs (cosmetic) stay editable.

## Editor validation
Before anything is written, the open mod checks each edited file (`validate_config` on JVExtraShots /
JVGoofyMods): ranges, enums, known shot ids, real game action names and the game-clash rules
(topspin / Curveball / Float stay airborne-only). A refusal reads `not saved: <file>: <why>`; nothing is
written. Valid saves keep the `.bak` + atomic write + live reload.

## Menu motion cost (jvm_core `menu_perf`)
The stock menu already spends a lot of CPU while it moves (page `_show` / stagger tweens,
`_tilt_cards` / `_lean_tiles` / `_place_strips` rewriting transforms and `z_index` every
frame, plus the 3D court still drawing). That is a base-game cost, not just the Mods card —
FPS can drop from 300+ to around 200 on pages we never changed.

`JVMenuPerf` in jvm_core (runtime injection only; the pck is not edited) does the same tilt
and lean math in C++, skips no-op `z_index` and unchanged strip-offset writes, and is
toggleable with `"menu_perf": true` in `mods/core/00-default.json` (on by default; set
`false` to A/B against the stock path). Look and behaviour stay the same. Goofy no longer
walks the whole menu tree every frame (it uses the menu's `_modifiers` pointer, or a one-shot
walk until the panel is found). Picker pill labels refresh at 5 Hz instead of every frame;
card flattening (`suppress_cards`) only writes when the overlay state changes.
