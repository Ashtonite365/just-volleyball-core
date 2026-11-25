jv-mods install folder (next to volleyball.exe). Everything jv-mods needs lives here;
the only file outside it is override.cfg next to volleyball.exe.
Framework files (don't edit):
  bin/                 the six DLLs + .gdextension files
  bootstrap.gd         loader started by override.cfg
  pins.json            pinned game version (mods refuse to load on any other build)
  gate_probes.json     how the gate reads the lobby type
  texture_sync.json    texture ID sync settings
  Watch-JvModStatus.ps1  live status viewer (PowerShell)
Your mod content (each folder is scanned at runtime - add or delete folders freely):
  resources/packs/<pack>/skin.json   resource packs (outfits, hats, back accessories, ball, net, court)
                                     {"id": "A1F3", "name": "My skin", "kind": "skin"|"global"}; see resources/packs/README.txt
  resources/packs/equipped.json      {"equipped": ["A1F3"]}  skins you wear (synced as IDs)
  models/<model>/skin.json           full player models {"kind": "model", "id": "BEA0", "model": "my.glb"}; see models/README.txt
  models/equipped.json               {"model": "BEA0"}  the model you wear (local only; locker FULL MODEL tab)
  shots/*.json     extra shots on the game's own controls (see 00-default.json):
                   in the air on a rally touch: forward = topspin (exact landing marker), back = Float
                   (the game's own float + float box), left/right = Curveball (hold longer = further;
                   fading range box, not the exact spot). Ground side shots are the game's own.
                   rally: hold sprint+strafe + set = high shot, + bump/spike = low shot
                   serve: hold sprint + tap = high, medium = low, full charge = fast, jump = jump serve;
                          sprint + move back = standing float (ground) / float serve (jump)
  goofy/*.json     big_ball, ball_physics, player_scale, max_hits_per_team, no_double_hit,
                   time_scale (opt-in) - see 00-default.json; max hits / no double-hit are
                   also rows in the custom-lobby Modifiers panel (host decides)
Kill switch: create an empty file named DISABLE here, or jv_mods.json {"enabled": false}.
All mods: main menu, LAN, private / friends-only custom lobbies, Free Play, offline solo, practice.
Textures + models only: also public custom lobbies, Quick Match, server browser.
Status: %APPDATA%\Godot\app_userdata\volleyball\jv_mods\status.json  (F8 in game = overlay)
