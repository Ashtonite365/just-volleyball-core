#!/usr/bin/env python3
"""Classify every Goofy setting for the gameplay settings registry (docs/GAMEPLAY-SETTINGS.md).
Reads the two Goofy library tables (src/gate/goofy_params.inc, src/gate/goofy_player_params.inc)
plus Goofy's top-level config keys and prints the counts and a markdown table.
  movable      - changes play, a plain number the registry can carry as-is (live apply)
  needs_work   - changes play, but needs a decision or code before it can be a registry setting
  visual_only  - looks only: stays per-player, never registered (cosmetics are refused)
  ported       - already registered by Goofy (goofy.*)
Usage: python3 tools/gameplay_assessment.py [--md]"""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


POSE_ONLY = {"crouch_squash", "launch_stretch", "launch_charge_gain", "land_pop", "launch_spring", "launch_damp",
             "hand_mesh_radius", "gaze_near", "gaze_far", "pose_speed", "recoil_kick", "recoil_stiff", "recoil_damp",
             "air_lean", "dump_tilt", "receive_anim_speed", "receive_pose_min", "receive_pose_max", "set_pose"}
BALL_VISUAL = {"trail_min_speed", "trail_fast_speed"}
NEEDS = {
    "ball.contest_band": "filed under visuals but feeds the net-contest rule (rules.gd:153/236, player.gd:3978): reclassify as gameplay",
    "ball.contest_min_vz": "same as contest_band (ball.gd:1843 net_contested_for)",
    "player.charge_shake": "shakes the serve hold point (player.gd:3382), not only the pose: confirm the ball is not moved before registering",
    "player.charge_shake_hz": "same as charge_shake",
}

PORTED = {"flight.air_drag": "goofy.air_drag", "flight.curve_scale": "goofy.curve_scale",
          "ball.gravity": "goofy.gravity_scale (multiplier)", "ball.radius": "goofy.ball_size (multiplier)"}

TOP = [
    ("big_ball", "ported", "goofy.ball_size"),
    ("ball_physics.gravity_scale", "ported", "goofy.gravity_scale"),
    ("ball_physics.bounce_scale", "ported", "goofy.bounce_scale"),
    ("time_scale", "ported", "goofy.time_scale"),
    ("max_hits_per_team", "ported", "goofy.rules.max_team_touches"),
    ("no_double_hit", "ported", "goofy.rules.double_hit_foul (inverted) + goofy.rules.max_consecutive_touches"),
    ("PlayerFeel.block_classic", "ported", "goofy.rules.block_classic"),
    ("player_scale", "movable", "a float like ball_size (player body scale)"),
    ("ball_physics.<any property>", "needs_work", "free-form property names: register per property or keep as config"),
    ("overrides", "needs_work", "generic node/property overrides (targets + any property): not a single typed value"),
    ("court pack goofy layers", "needs_work", "per-court layers (space gravity, underwater drag): stay pack layers, their effective values are host-synced by Goofy"),
]

def params(path):
    out = []
    for l in open(os.path.join(ROOT, path)):
        m = re.match(r'GP\("([\w.]+)", "(\w+)", \w+, "(\w+)", (true|false), .*"(\w+)"\)\s*$', l.strip())
        if m: out.append(dict(id=m.group(1), section=m.group(2), prop=m.group(3), color=m.group(4) == "true", apply=m.group(5)))
    return out

def classify(p):
    if p["id"] in PORTED: return "ported", PORTED[p["id"]]
    if p["id"] in NEEDS: return "needs_work", NEEDS[p["id"]]
    if p["color"]: return "visual_only", "colour"
    if p["prop"] in POSE_ONLY: return "visual_only", "pose / animation only"
    if p["prop"] in BALL_VISUAL: return "visual_only", "trail look"
    if p["apply"] == "live_copy": return "needs_work", "copied by the game at match start: a change applies from the next match (needs an apply-at-start hook)"
    return "movable", p["section"]

rows = [(p["id"], *classify(p)) for p in params("src/gate/goofy_params.inc") + params("src/gate/goofy_player_params.inc")]
tops = [(k, c, n) for k, c, n in TOP]
counts = {}
for _, c, _ in rows + tops: counts[c] = counts.get(c, 0) + 1
total = len(rows) + len(tops)
if "--md" not in sys.argv:
    print(f"{len(rows)} library params + {len(tops)} config keys = {total}")
    for k in ("ported", "movable", "needs_work", "visual_only"): print(f"  {k}: {counts.get(k, 0)}")
    lib = {}
    for _, c, _ in rows: lib[c] = lib.get(c, 0) + 1
    print("  library params only:", lib)
    sys.exit(0)
print("| setting | class | note |\n|---|---|---|")
for k, c, n in tops + [r for r in rows if r[1] != "movable"]:
    print(f"| `{k}` | {c.replace('_', ' ')} | {n} |")
sec = {}
for i, c, s in rows:
    if c == "movable": sec[s] = sec.get(s, 0) + 1
print("| all other library params | movable | " + ", ".join(f"{s} {n}" for s, n in sorted(sec.items())) + " |")
