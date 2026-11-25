#!/usr/bin/env python3
"""Numerical barrier-clearance and floor-coverage audit for Just Volleyball court packs.

Real game facts (from pck_out, not eyeballing):
  Stage.PLAY_BOX = (15.5, 14.0, 26.0)  — force-wall half-extents (x, y, z)
  Court.DOME_N = 4, DOME_HEIGHT_MUL = 1.7, DOME_THETA = 1.02
  The force wall is a superellipse tube in xz that tapers slightly with height.
  Floor *collision* is an analytic horizontal plane at y = 0 (ball lands at
  y = PHYS.radius). hide_ground only calls set_visible(false) on EnvFloor;
  it does not change collision.
  Visual kit floors:
    grass / beach EnvFloor: BoxMesh 117.6 x 0.04 x 117.6 at y = -0.12
    school: EnvFloor exists but is hidden; gym "Gym walls_001" floor covers
            ±36.57 x ±37.95 at y = 0 (kit scale 0.62 already applied)

Rules (Ashton 03:39):
  1) Non-floor pack geometry / props must not intersect or touch the force-wall
     surface. A clear margin (MIN_MARGIN) is required. Geometry entirely above
     the wall height (y >= 14 + MIN_MARGIN) may sit over the court (ceilings,
     hanging lamps) because the wall is a tube, not a closed dome.
  2) There must always be visible ground under the entire playable area out to
     AND PAST the barrier. Packs that hide EnvFloor must provide a role=floor
     prop that covers at least the playable superellipse plus COVER_PAST meters.
  Ground slabs at y≈0 MAY extend through the vertical wall (they are the
  walking surface). Upright props may not.
"""
from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

import numpy as np

PLAY_BOX = (15.5, 14.0, 26.0)
DOME_N = 4.0
DOME_HEIGHT_MUL = 1.7
DOME_THETA = 1.02
MIN_MARGIN = 0.75
COVER_PAST = 0.50
FLOOR_Y_MIN, FLOOR_Y_MAX = -0.50, 0.25
FORCE_WALL_H = PLAY_BOX[1]


KIT_FLOOR = {
    "grass": {"half_x": 58.8, "half_z": 58.8, "name": "EnvFloor 117.6x117.6", "y": -0.12},
    "beach": {"half_x": 58.8, "half_z": 58.8, "name": "EnvFloor 117.6x117.6", "y": -0.12},
    "school": {"half_x": 36.57, "half_z": 37.95, "name": "gym floor walls_001 ±36.57x±37.95", "y": 0.0},
}



COVER_HX = PLAY_BOX[0] + COVER_PAST
COVER_HZ = PLAY_BOX[2] + COVER_PAST


def dome_scale(y: float) -> float:
    top = PLAY_BOX[1] * DOME_HEIGHT_MUL
    s = min(max(y, 0.0), top) * math.sin(DOME_THETA) / top
    return math.sqrt(max(0.0, 1.0 - s * s))


def dome_reach(x: float, y: float, z: float, pad: float = 0.0) -> float:
    k = dome_scale(y)
    hw = max(PLAY_BOX[0] * k - pad, 0.1)
    hl = max(PLAY_BOX[2] * k - pad, 0.1)
    return (abs(x / hw) ** DOME_N + abs(z / hl) ** DOME_N) ** (1.0 / DOME_N)


def wall_radius(angle: float, y: float = 0.0) -> float:
    from math import cos, sin
    c, s = cos(angle), sin(angle)
    k = dome_scale(y)
    hw = max(PLAY_BOX[0] * k, 0.1)
    hl = max(PLAY_BOX[2] * k, 0.1)
    return 1.0 / (((abs(c) / hw) ** DOME_N + (abs(s) / hl) ** DOME_N) ** (1.0 / DOME_N))


def signed_dist_to_wall(x: float, y: float, z: float) -> float:
    """Positive = outside the wall. Along the xz ray from the origin."""
    r = dome_reach(x, y, z)
    dist_xz = math.hypot(x, z)
    wall = dist_xz / max(r, 1e-9)
    return dist_xz - wall


def _mat_from_node(n: dict) -> np.ndarray:
    if "matrix" in n:
        return np.array(n["matrix"], dtype=np.float64).reshape(4, 4).T
    T = np.eye(4)
    if "translation" in n:
        T[:3, 3] = n["translation"]
    R = np.eye(4)
    if "rotation" in n:
        x, y, z, w = n["rotation"]
        R[:3, :3] = np.array(
            [
                [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
            ]
        )
    S = np.eye(4)
    if "scale" in n:
        sx, sy, sz = n["scale"]
        S[0, 0], S[1, 1], S[2, 2] = sx, sy, sz
    return T @ R @ S


def _euler_deg_mat(rx, ry, rz) -> np.ndarray:
    """Godot YXZ (default) rotation, degrees."""
    ax, ay, az = [math.radians(v) for v in (rx, ry, rz)]
    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)
    cz, sz = math.cos(az), math.sin(az)

    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    M = np.eye(4)
    M[:3, :3] = Ry @ Rx @ Rz
    return M


def load_glb_world_verts(path: Path):
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise ValueError(f"not a glb: {path}")
    off = 12
    js = None
    blob = None
    while off + 8 <= len(data):
        clen, ctype = struct.unpack_from("<II", data, off)
        off += 8
        chunk = data[off : off + clen]
        off += clen
        if ctype == 0x4E4F534A:
            js = json.loads(chunk)
        elif ctype == 0x004E4942:
            blob = chunk
    if js is None:
        raise ValueError(f"no JSON chunk: {path}")
    nodes = js.get("nodes", [])
    parent = [-1] * len(nodes)
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i
    world = [None] * len(nodes)

    def world_of(i):
        if world[i] is None:
            M = _mat_from_node(nodes[i])
            world[i] = world_of(parent[i]) @ M if parent[i] >= 0 else M
        return world[i]

    accessors = js.get("accessors", [])
    views = js.get("bufferViews", [])

    def acc_pos(idx):
        a = accessors[idx]
        bv = views[a["bufferView"]]
        boff = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        n = a["count"]
        stride = bv.get("byteStride", 12)
        if stride == 12:
            return np.frombuffer(blob, dtype=np.float32, count=n * 3, offset=boff).reshape(n, 3).astype(np.float64)
        raw = np.frombuffer(blob, dtype=np.uint8, count=n * stride, offset=boff)
        out = np.empty((n, 3), dtype=np.float64)
        for i in range(n):
            out[i] = np.frombuffer(raw[i * stride : i * stride + 12], dtype=np.float32)
        return out

    pts = []
    faces = []
    meshes = js.get("meshes", [])
    acc_offset = 0
    def acc_idx(idx):
        a = accessors[idx]
        typ = a["componentType"]
        dt = {5121: np.uint8, 5123: np.uint16, 5125: np.uint32}[typ]
        bv = views[a["bufferView"]]
        boff = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        n = a["count"]
        return np.frombuffer(blob, dtype=dt, count=n, offset=boff).astype(np.int64)

    for i, n in enumerate(nodes):
        if "mesh" not in n:
            continue
        M = world_of(i)
        mesh = meshes[n["mesh"]]
        for prim in mesh.get("primitives", []):
            pos = acc_pos(prim["attributes"]["POSITION"])
            w = (M @ np.hstack([pos, np.ones((len(pos), 1))]).T).T[:, :3]
            base = sum(len(p) for p in pts)
            pts.append(w)
            if "indices" in prim:
                ind = acc_idx(prim["indices"])
                mode = prim.get("mode", 4)
                if mode == 4 and len(ind) >= 3:
                    faces.append(ind.reshape(-1, 3) + base)
            else:
                if len(w) >= 3:
                    faces.append(np.arange(base, base + len(w)).reshape(-1, 3))
    if not pts:
        return np.zeros((0, 3), dtype=np.float64), np.zeros((0, 3), dtype=np.int64)
    V = np.vstack(pts)
    F = np.vstack(faces) if faces else np.zeros((0, 3), dtype=np.int64)
    return V, F



def load_glb_nodes(path: Path):
    """Return list of {name, verts, faces} in glb-local space."""
    import json as _json, struct as _struct
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise ValueError(f"not a glb: {path}")
    off = 12
    js = None
    blob = None
    while off + 8 <= len(data):
        clen, ctype = _struct.unpack_from("<II", data, off)
        off += 8
        chunk = data[off : off + clen]
        off += clen
        if ctype == 0x4E4F534A:
            js = _json.loads(chunk)
        elif ctype == 0x004E4942:
            blob = chunk
    nodes = js.get("nodes", [])
    parent = [-1] * len(nodes)
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i
    world = [None] * len(nodes)

    def world_of(i):
        if world[i] is None:
            M = _mat_from_node(nodes[i])
            world[i] = world_of(parent[i]) @ M if parent[i] >= 0 else M
        return world[i]

    accessors = js.get("accessors", [])
    views = js.get("bufferViews", [])

    def acc_pos(idx):
        a = accessors[idx]
        bv = views[a["bufferView"]]
        boff = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        n = a["count"]
        stride = bv.get("byteStride", 12)
        if stride == 12:
            return np.frombuffer(blob, dtype=np.float32, count=n * 3, offset=boff).reshape(n, 3).astype(np.float64)
        raw = np.frombuffer(blob, dtype=np.uint8, count=n * stride, offset=boff)
        out = np.empty((n, 3), dtype=np.float64)
        for i in range(n):
            out[i] = np.frombuffer(raw[i * stride : i * stride + 12], dtype=np.float32)
        return out

    def acc_idx(idx):
        a = accessors[idx]
        typ = a["componentType"]
        dt = {5121: np.uint8, 5123: np.uint16, 5125: np.uint32}[typ]
        bv = views[a["bufferView"]]
        boff = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        n = a["count"]
        return np.frombuffer(blob, dtype=dt, count=n, offset=boff).astype(np.int64)

    meshes = js.get("meshes", [])
    out = []
    for i, n in enumerate(nodes):
        if "mesh" not in n:
            continue
        M = world_of(i)
        mesh = meshes[n["mesh"]]
        pts = []
        faces = []
        for prim in mesh.get("primitives", []):
            pos = acc_pos(prim["attributes"]["POSITION"])
            w = (M @ np.hstack([pos, np.ones((len(pos), 1))]).T).T[:, :3]
            base = sum(len(p) for p in pts)
            pts.append(w)
            if "indices" in prim:
                ind = acc_idx(prim["indices"])
                if prim.get("mode", 4) == 4 and len(ind) >= 3:
                    faces.append(ind.reshape(-1, 3) + base)
        if not pts:
            continue
        out.append({
            "name": n.get("name", f"node{i}"),
            "verts": np.vstack(pts),
            "faces": np.vstack(faces) if faces else np.zeros((0, 3), dtype=np.int64),
        })
    return out


def transform_verts(verts: np.ndarray, position, rotation_deg, scale) -> np.ndarray:
    S = np.eye(4)
    sx, sy, sz = scale
    S[0, 0], S[1, 1], S[2, 2] = sx, sy, sz
    R = _euler_deg_mat(*rotation_deg)
    T = np.eye(4)
    T[:3, 3] = position
    M = T @ R @ S
    if verts.size == 0:
        return verts
    return (M @ np.hstack([verts, np.ones((len(verts), 1))]).T).T[:, :3]


def _vec3(v, default=(0.0, 0.0, 0.0)):
    if v is None:
        return tuple(default)
    if isinstance(v, (int, float)):
        return (float(v), float(v), float(v))
    if isinstance(v, (list, tuple)) and len(v) >= 3:
        return (float(v[0]), float(v[1]), float(v[2]))
    return tuple(default)


def load_prop_geom(pack_dir: Path, prop: dict):
    rel = prop.get("mesh") or prop.get("path") or ""
    path = (pack_dir / rel).resolve()
    pack_res = pack_dir.resolve()
    if pack_res not in path.parents and path != pack_res:

        if not str(path).startswith(str(pack_res)):
            raise ValueError(f"prop path escapes pack: {rel}")
    if prop.get("_verts") is not None:
        verts, faces = prop["_verts"], prop.get("_faces")
        if faces is None:
            faces = np.zeros((0, 3), dtype=np.int64)
    else:
        verts, faces = load_glb_world_verts(path)
    if prop.get("_verts") is not None:
        scale = prop.get("scale", 1)
        if isinstance(scale, (int, float)):
            scale = (float(scale), float(scale), float(scale))
        else:
            scale = _vec3(scale, (1, 1, 1))
        tv = transform_verts(
            verts,
            _vec3(prop.get("position") or prop.get("pos")),
            _vec3(prop.get("rotation_deg") or prop.get("rotation")),
            scale,
        )
        return tv, faces

    verts, faces = verts, faces
    scale = prop.get("scale", 1)
    if isinstance(scale, (int, float)):
        scale = (float(scale), float(scale), float(scale))
    else:
        scale = _vec3(scale, (1, 1, 1))
    tv = transform_verts(
        verts,
        _vec3(prop.get("position") or prop.get("pos")),
        _vec3(prop.get("rotation_deg") or prop.get("rotation")),
        scale,
    )
    return tv, faces


def audit_points_margin(verts: np.ndarray, faces: np.ndarray | None = None):
    """Return (min_distance_to_wall_surface, violations).

    The rule is about the force-wall *surface*, not the playable interior:
    short grass or similar may sit inside the dome, but nothing may come
    within MIN_MARGIN of the wall, and a mesh may not straddle it.
    Vertices entirely above the wall (y >= 14 + MIN_MARGIN) are ignored
    (ceilings, hanging lamps).
    """
    if verts.size == 0:
        return math.inf, []
    viol = []
    min_abs = math.inf
    y_cut = FORCE_WALL_H + MIN_MARGIN
    step = max(1, len(verts) // 80000)
    sample = verts[::step]
    mn, mx = sample.min(axis=0), sample.max(axis=0)
    corners = np.array([[x, y, z] for x in (mn[0], mx[0]) for y in (mn[1], mx[1]) for z in (mn[2], mx[2])])
    sample = np.vstack([sample, corners])
    ds = []
    pts = []
    for x, y, z in sample:
        if y >= y_cut:
            continue
        d = signed_dist_to_wall(float(x), float(y), float(z))
        ds.append(d)
        pts.append((float(x), float(y), float(z), d))
        min_abs = min(min_abs, abs(d))
        if abs(d) < MIN_MARGIN:
            viol.append((float(x), float(y), float(z), d))
    if faces is not None and len(faces) and verts.size:
        y_cut = FORCE_WALL_H + MIN_MARGIN
        step_f = max(1, len(faces) // 40000)
        for a, b, c in faces[::step_f]:
            tri = verts[[int(a), int(b), int(c)]]

            if (tri[:, 1] >= y_cut).all():
                continue
            td = [signed_dist_to_wall(float(x), float(y), float(z)) for x, y, z in tri]
            if min(td) < 0.0 and max(td) > 0.0:
                i = int(np.argmin(np.abs(td)))
                x, y, z = tri[i]
                viol.append((float(x), float(y), float(z), td[i]))
                break

    return min_abs, viol


def floor_xz_extents(verts: np.ndarray):
    if verts.size == 0:
        return 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    return (
        float(verts[:, 0].min()),
        float(verts[:, 0].max()),
        float(verts[:, 2].min()),
        float(verts[:, 2].max()),
        float(verts[:, 1].min()),
        float(verts[:, 1].max()),
    )


def covers_playable(half_x: float, half_z: float) -> bool:
    return half_x + 1e-6 >= COVER_HX and half_z + 1e-6 >= COVER_HZ


def audit_pack(pack_dir: Path) -> dict:
    pack_dir = Path(pack_dir)
    man = json.loads((pack_dir / "skin.json").read_text())
    name = man.get("label") or man.get("name") or pack_dir.name
    cid = str(man.get("id", ""))
    base = str(man.get("base_stage", "grass")).lower()
    sky = man.get("sky") if isinstance(man.get("sky"), dict) else {}
    hide_ground = bool(sky.get("hide_ground", False))
    props = list(man.get("props") if isinstance(man.get("props"), list) else [])
    env = man.get("environment") if isinstance(man.get("environment"), dict) else {}
    if env:
        hide_ground = hide_ground or bool(env.get("hide_ground", False))
        meshes = []
        if env.get("mesh") or env.get("glb") or env.get("path"):
            meshes.append(env.get("mesh") or env.get("glb") or env.get("path"))
        if env.get("floor"):
            props.append({"mesh": env.get("floor"), "role": "floor", "name": "environment.floor"})

        if not meshes:
            for lod in env.get("lod") or []:
                if isinstance(lod, dict) and (lod.get("mesh") or lod.get("glb")):
                    meshes.append(lod.get("mesh") or lod.get("glb"))

        floor = env.get("floor")
        seen = set()
        for m in meshes:
            if not m or m == floor or m in seen:
                continue
            seen.add(m)

            role = "floor" if any(k in str(m).lower() for k in ("floor", "deck", "seabed", "ground")) else "prop"
            props.append({"mesh": m, "role": role, "name": f"environment:{m}",
                          "position": env.get("position") or env.get("pos"),
                          "rotation": env.get("rotation_deg") or env.get("rotation"),
                          "scale": env.get("scale", 1),
                          "split_nodes": True})

    issues = []
    min_margin = math.inf
    floor_spans = []

    if not hide_ground and base in KIT_FLOOR:
        kf = KIT_FLOOR[base]
        floor_spans.append((kf["half_x"], kf["half_z"], kf["name"], kf["y"], kf["y"]))

    expanded_props = []
    for prop in props:
        if not prop.get("split_nodes"):
            expanded_props.append(prop)
            continue
        rel = prop.get("mesh") or prop.get("path") or ""
        try:
            path = (pack_dir / rel).resolve()
            nodes = load_glb_nodes(path)
        except Exception as e:
            issues.append(f"{prop.get('name') or rel}: failed to load ({e})")
            continue
        if not nodes:
            issues.append(f"{prop.get('name') or rel}: empty environment scene")
            continue
        for nd in nodes:
            nm = str(nd["name"]).lower()
            nrole = "floor" if any(k in nm for k in ("floor", "deck", "seabed", "ground", "sand")) else "prop"


            np_prop = dict(prop)
            np_prop["role"] = nrole
            np_prop["name"] = f"{prop.get('name') or rel}/{nd['name']}"
            np_prop["split_nodes"] = False
            np_prop["_verts"] = nd["verts"]
            np_prop["_faces"] = nd["faces"]
            expanded_props.append(np_prop)
    props = expanded_props

    for i, prop in enumerate(props):

        role = str(prop.get("role", "prop")).strip().lower() or "prop"
        label = prop.get("name") or prop.get("mesh") or f"prop[{i}]"
        try:
            verts, faces = load_prop_geom(pack_dir, prop)
        except Exception as e:
            issues.append(f"{label}: failed to load ({e})")
            continue
        if verts.size == 0:
            issues.append(f"{label}: empty mesh")
            continue
        if role == "floor":
            xmin, xmax, zmin, zmax, ymin, ymax = floor_xz_extents(verts)
            hx = min(abs(xmin), abs(xmax))

            cover_x = min(-xmin, xmax)
            cover_z = min(-zmin, zmax)

            ok_box = xmin <= -COVER_HX and xmax >= COVER_HX and zmin <= -COVER_HZ and zmax >= COVER_HZ
            half_x = min(-xmin, xmax)
            half_z = min(-zmin, zmax)
            floor_spans.append((half_x, half_z, f"pack floor {label} {xmax-xmin:.1f}x{zmax-zmin:.1f}", ymin, ymax))
            if ymin < FLOOR_Y_MIN - 2.5:


                if ymax < FLOOR_Y_MIN:
                    issues.append(
                        f"{label}: floor mesh y=[{ymin:.2f},{ymax:.2f}] is not at the ground plane"
                    )


            inside = verts[(np.abs(verts[:, 0]) <= COVER_HX) & (np.abs(verts[:, 2]) <= COVER_HZ)]
            if len(inside):
                iymax = float(inside[:, 1].max())
                iymin = float(inside[:, 1].min())
                if iymax > FLOOR_Y_MAX + 0.05:
                    issues.append(
                        f"{label}: visible floor in the playable+past area rises to y={iymax:.2f} "
                        f"(must match the y=0 collision plane; no z-fighting or bumps)"
                    )
                if iymin < FLOOR_Y_MIN:
                    issues.append(
                        f"{label}: visible floor in the playable+past area drops to y={iymin:.2f} "
                        f"(gap / void under the collision plane)"
                    )
            if not ok_box:
                issues.append(
                    f"{label}: floor xz=[{xmin:.2f},{xmax:.2f}]x[{zmin:.2f},{zmax:.2f}] "
                    f"does not cover playable+past (±{COVER_HX:.2f} x ±{COVER_HZ:.2f})"
                )
        else:
            d, viol = audit_points_margin(verts, faces)
            if math.isfinite(d):
                min_margin = min(min_margin, d)
            if viol:
                x, y, z, dd = min(viol, key=lambda t: abs(t[3]))
                issues.append(
                    f"{label}: intersects/too close to barrier (dist to wall {dd:.3f}m "
                    f"at ({x:.2f},{y:.2f},{z:.2f}); need |d| ≥ {MIN_MARGIN:.2f}m, no straddle)"
                )


    if floor_spans:
        half_x = max(s[0] for s in floor_spans)
        half_z = max(s[1] for s in floor_spans)
        floor_ok = covers_playable(half_x, half_z)
        floor_desc = "; ".join(s[2] for s in floor_spans)
        coverage_str = f"{half_x*2:.1f}x{half_z*2:.1f} ({floor_desc})"
    else:
        half_x = half_z = 0.0
        floor_ok = False
        coverage_str = "NONE"
    if not floor_ok:
        issues.append(
            f"visible floor does not cover playable area out to and past the barrier "
            f"(need ≥ {COVER_HX*2:.1f}x{COVER_HZ*2:.1f}; have {coverage_str})"
            + (" [hide_ground is set; kit EnvFloor is hidden]" if hide_ground else "")
        )

    if min_margin is math.inf:

        margin_out = None
    else:
        margin_out = round(min_margin, 3)

    return {
        "id": cid,
        "folder": pack_dir.name,
        "name": name,
        "base_stage": base,
        "hide_ground": hide_ground,
        "violated": bool(issues),
        "what": "; ".join(issues) if issues else "none",
        "min_barrier_margin": margin_out,
        "floor_coverage": coverage_str,
        "issues": issues,
    }


def self_check() -> int:
    fails = 0

    def expect(cond, msg):
        nonlocal fails
        if not cond:
            print("SELF-CHECK FAIL:", msg)
            fails += 1
        else:
            print("SELF-CHECK ok:", msg)

    expect(abs(dome_reach(15.5, 0.0, 0.0) - 1.0) < 1e-6, "reach(15.5,0,0)==1")
    expect(abs(dome_reach(0.0, 0.0, 26.0) - 1.0) < 1e-6, "reach(0,0,26)==1")
    expect(dome_reach(0.0, 0.0, 0.0) < 1e-9, "reach(origin)~0")
    expect(abs(signed_dist_to_wall(16.5, 0.0, 0.0) - 1.0) < 1e-4, "dist outside +1m on +x")
    expect(signed_dist_to_wall(14.5, 0.0, 0.0) < 0, "dist inside -x is negative")

    expect(dome_reach(15.5, 0.0, 26.0) > 1.1, "AABB corner is outside the dome")
    expect(covers_playable(58.8, 58.8), "EnvFloor covers")
    expect(covers_playable(36.57, 37.95), "school gym floor covers")
    expect(not covers_playable(15.5, 26.0), "exact barrier AABB is not past enough")
    expect(covers_playable(16.0, 26.5), "barrier+0.5 covers")
    return fails


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Audit court packs against the force wall + floor coverage")
    ap.add_argument("packs", nargs="*", help="pack folders (containing skin.json)")
    ap.add_argument("--self-check", action="store_true")
    ap.add_argument("--expect-fail", action="store_true", help="invert exit: 0 if packs violate")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    rc = 0
    if args.self_check:
        rc += self_check()
    reports = []
    for p in args.packs:
        r = audit_pack(Path(p))
        reports.append(r)
        flag = "VIOLATION" if r["violated"] else "ok"
        print(
            f"{r['id'] or r['folder']}: {flag}  margin={r['min_barrier_margin']}  "
            f"floor={r['floor_coverage']}  {r['what']}"
        )
        if r["violated"] and not args.expect_fail:
            rc = 1
        if (not r["violated"]) and args.expect_fail:
            rc = 1
    if args.expect_fail and reports and all(r["violated"] for r in reports):
        rc = 0
    if args.json:
        print(json.dumps(reports, indent=2))
    return rc


if __name__ == "__main__":
    sys.exit(main())
