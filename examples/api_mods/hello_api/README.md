# hello_api — example CORE API script mod

Shows the pattern every mod should follow with the jv-mods CORE API (docs/CORE-API.md):

1. `Engine.get_singleton("JVModAPI")` (never `/root/JV*` paths).
2. `register_mod(name, "1.1", caps)` declaring every capability it calls.
3. Read through query functions, write through gated functions, subscribe to events.
4. Undo on `access_changed(open=false)` and in `_exit_tree` (undo calls are always allowed).

Not installed by the release zip. To try it: copy this folder to `<game>/mods/api_mods/hello_api/`.
Disable without deleting: set `"enabled": false` in `mod.json`.
