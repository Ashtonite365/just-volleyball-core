Full player models (read by jvm_models only). One folder per model; scanned at runtime.

  <any folder name>\skin.json   {"kind": "model", "id": "BEA0", "name": "...", "model": "my.glb"}
                                model file: .glb / .gltf / .tscn / .scn using the game's 26-bone skeleton
                                optional: "game_hands": "tint" | "own" | "hide", "color": "#RRGGBB"
  <any folder name>\<model file> the model itself (plus any textures it uses)
  <any folder name>\preview.png optional picture
  equipped.json                 {"model": "BEA0"}   which model you wear; no "model" = the game's own
  model_settings.json           written by the locker's FULL MODEL tab (colour / hands / who wears it)

Pick a model in the locker (FULL MODEL tab) or edit equipped.json. Add a model: drop its folder in.
Remove: delete its folder (you go back to the game's own model). Models are only shown to you.
Resource packs (outfits, hats, ball) are separate, in mods\resources\packs\; either folder works
without the other.
