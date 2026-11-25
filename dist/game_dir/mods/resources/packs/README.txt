Resource packs (read by jvm_textures). One folder per pack; the game scans this folder every
time it starts (and on reload), so nothing about a pack is built into the mod.

  <any folder name>\skin.json     required: {"id": "1A2B", "name": "...", "kind": "skin" | "global", ...}
                                  id = 4 hex characters, unique among the packs in THIS folder
                                  type "ball" or "stage" packs are picker tiles (not always-on)
  <any folder name>\*.png / *.glb  the files skin.json names (textures, hat / back models)
  <any folder name>\preview.png   tile picture for the ball/stage picker
  <any folder name>\res\<path>    optional: replaces res://<path> in the game
  equipped.json                   {"equipped": ["1001", "1002"], "ball": "1006"}
                                  "equipped" = which "skin" packs you wear (ids synced)
                                  optional "ball" / "stage" = picker fallback if picker.json is missing
  picker.json                     {"ball": "1006", "stage": ""}  selected picker tiles
                                  empty or missing = the game's own ball/stage

"global" packs that are NOT type ball/stage (net, court without a type) still apply as soon as
their folder is here. type "ball" / "stage" packs apply only when selected in the picker (or
named in picker.json / equipped.json). "skin" packs (outfit, hat, back) apply when their id
is in equipped.json.
Custom balls (type "ball", wrap "equirect") use a smooth cube-sphere and a 2:1 equirect PNG
(V=0 at the top, north pole). They never overwrite the game's school / pro / outdoor models.
Add a pack: drop its folder in here. Remove a pack: delete its folder. Two packs with the same
id: the first folder (alphabetical) wins and the other is ignored with a warning in jv_mods.log.
Full player models are NOT here - they live in mods\models\ (separate, works without this folder).
Old location mods\resource packs\ is still read if this folder does not exist.

Hat / face / back "skin" packs also appear as their own tiles in the locker's
accessories section (HEAD, FACE, EXTRA). Equip them there independently.
equipped.json "equipped" still lists what you wear; ids are migrated into empty
locker slots on first load and kept in sync when you change a slot. Outfit packs
(no hat/face/back) stay equipped.json-only. picker.json is unchanged.
