#!/usr/bin/env python3
"""Loopback stand-in for the assets repo (docs/CONTENT-SHARING.md, tests only).

  mock_assets_server.py <workdir> <port>

Writes the source packs a test host wears to <workdir>/src/{packs,models}/<folder>, builds the
repo (manifest.json schema 1 + <path>/pack.zip per item, base http://127.0.0.1:<port>/) into
<workdir>/repo, writes <workdir>/ready and serves <workdir>/repo until killed. Requests are logged
to <workdir>/requests.log. Items:
  C001 outfit   clean                        -> downloaded, rendered
  C002 outfit   clean (equipped mid-lobby)   -> downloaded after the re-sent offer
  C0B1 ball     clean                        -> downloaded, the host's ball is shown
  B0D1 model    clean                        -> downloaded, the host's model is grafted on its rig
  C0DE outfit   the repo's copy differs from what the host wears -> hash mismatch, never fetched
  C0FF outfit   served pack.zip is not the manifest's (tampered)  -> rejected after download
  C0EE outfit   pack.zip also holds "../../evil.png"              -> rejected (unsafe entry)
"""
import hashlib, http.server, io, json, os, shutil, socketserver, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIX = os.path.normpath(os.path.join(HERE, "..", "fixtures"))
work, port = sys.argv[1], int(sys.argv[2])
base = f"http://127.0.0.1:{port}/"
src, repo = os.path.join(work, "src"), os.path.join(work, "repo")
shutil.rmtree(work, ignore_errors=True)
os.makedirs(repo)

def rd(p): return open(os.path.join(FIX, p), "rb").read()
red, green, bean = rd("red.png"), rd("green.png"), rd("models/bean.glb")
def outfit(i, name, tex): return {"skin.json": json.dumps({"id": i, "name": name, "kind": "skin"}).encode(), "res/tex/ball.png": tex}
ITEMS = [
    ("C001", "outfit", "outfits/c001_red", "packs", outfit("C001", "Shared Red", red), None, None),
    ("C002", "outfit", "outfits/c002_green", "packs", outfit("C002", "Shared Green", green), None, None),
    ("C0B1", "ball", "balls/c0b1_ball", "packs", {"skin.json": json.dumps({"id": "C0B1", "name": "Shared Ball", "type": "ball", "ball": {"albedo_tex": "ball_tex.png", "wrap": "equirect"}}).encode(), "ball_tex.png": red}, None, None),
    ("B0D1", "model", "models/b0d1_bean", "models", {"skin.json": json.dumps({"kind": "model", "id": "B0D1", "name": "Shared Bean", "model": "bean.glb"}).encode(), "bean.glb": bean}, None, None),
    ("C0DE", "outfit", "outfits/c0de_mismatch", "packs", outfit("C0DE", "Mine Differs", red), outfit("C0DE", "Mine Differs", green), None),
    ("C0FF", "outfit", "outfits/c0ff_tampered", "packs", outfit("C0FF", "Tampered", green), None, "tamper"),
    ("C0EE", "outfit", "outfits/c0ee_slip", "packs", outfit("C0EE", "Zip Slip", red), None, "slip"),
]
manifest = {"schema_version": 1, "name": "mock-assets", "items": []}
for i, typ, path, kind_dir, host_files, repo_files, tamper in ITEMS:
    folder = os.path.join(src, kind_dir, path.split("/")[-1])
    for rel, data in host_files.items():
        os.makedirs(os.path.dirname(os.path.join(folder, rel)), exist_ok=True)
        open(os.path.join(folder, rel), "wb").write(data)
    files = repo_files or host_files
    def mkzip(extra=None):
        b = io.BytesIO()
        with zipfile.ZipFile(b, "w", zipfile.ZIP_DEFLATED) as z:
            for rel, data in sorted(files.items()): z.writestr(rel, data)
            if extra: z.writestr(extra[0], extra[1])
        return b.getvalue()
    listed = mkzip()
    served = listed
    if tamper == "tamper":
        b = bytearray(listed); b[len(b) // 2] ^= 0xFF; served = bytes(b)
    if tamper == "slip": listed = served = mkzip(("../../evil.png", red))
    os.makedirs(os.path.join(repo, path), exist_ok=True)
    open(os.path.join(repo, path, "pack.zip"), "wb").write(served)
    manifest["items"].append({
        "id": i, "name": i, "label": i, "type": typ, "kind": "model" if typ == "model" else ("global" if typ == "ball" else "skin"), "version": "1.0.0",
        "sha256": hashlib.sha256(listed).hexdigest(), "size": len(listed), "download_url": base + path + "/pack.zip", "path": path,
        "files": [{"path": rel, "sha256": hashlib.sha256(d).hexdigest(), "size": len(d), "download_url": base + path + "/" + rel} for rel, d in sorted(files.items())]})
open(os.path.join(repo, "manifest.json"), "w").write(json.dumps(manifest, indent=1))
log = open(os.path.join(work, "requests.log"), "a")

class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=repo, **k)
    def log_message(self, fmt, *args): log.write(self.path + "\n"); log.flush()
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("127.0.0.1", port), H) as srv:
    open(os.path.join(work, "ready"), "w").write(base)
    srv.serve_forever()
