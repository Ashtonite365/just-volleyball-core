#!/usr/bin/env python3
"""Generate src/gate/goofy_params.inc (the Goofy ball library table) from the game's
BallPhysics exports (res://game/ball/ball_physics.gd, recovered scripts of build 26924091)
plus a curated PlayerFeel subset (reach / ball-contact hitboxes / player gravity) and
Goofy's own runtime flight params. Re-run after a game update:
  tools/gen_goofy_params.py <recovered game dir>/game  > src/gate/goofy_params.inc
  tools/gen_goofy_params.py <recovered game dir>/game --player > src/gate/goofy_player_params.inc
Only names, defaults and our own ranges/units are emitted (no game code)."""
import re, sys
args = [a for a in sys.argv[1:] if not a.startswith("--")]
root = args[0] if args else None
if not root:
    sys.exit("usage: gen_goofy_params.py <game-scripts-folder>")

def exports(path):
    out = []
    for line in open(path, encoding="utf-8"):
        m = re.match(r"@export var (\w+): = (.+)$", line.strip())
        if not m: continue
        v = m.group(2)
        c = re.match(r"Color\(([^)]*)\)", v)
        if c:
            p = [float(x) for x in c.group(1).split(",")] + [1.0]
            out.append((m.group(1), "color", p[:4]))
        else:
            try: out.append((m.group(1), "float", float(v)))
            except ValueError: out.append((m.group(1), "other", v))
    return out

def section(n):
    if n in ("radius", "body_radius", "body_height", "carom_radius", "carom_height", "antenna_radius"): return "hitbox"
    if n in ("net_height", "net_half_width", "antenna_half_width", "antenna_rise"): return "net_geometry"
    if n in ("gravity", "floor_friction", "roll_drag", "magnet_lean_max"): return "physics"
    if n.startswith(("outline_", "trail_", "contest_")): return "visuals"
    if n.startswith(("curve_", "float_", "tip_")) or n in ("drive_time_factor",): return "spin_curve"
    if n.startswith("serve_"): return "serve"
    if "max_speed" in n or "min_speed" in n or n.startswith("spike_speed_") or n in ("spike_overdrive_max",): return "speed_limits"
    if n == "floor_bounce" or n.startswith(("tape_", "net_", "carom_", "glance_", "body_", "tool_", "block_")): return "bounce_collision"
    return "shot_shaping"

def unit(n, kind):
    if kind == "color": return "rgba"
    if n.endswith("_deg"): return "deg"
    if n in ("gravity", "tip_drop_gravity"): return "m/s^2"
    if "speed" in n or n.endswith(("_vy", "_lift", "_lift_max", "_pop_base", "_pop_min", "_pop_max", "_free")) or n in ("tape_drift", "body_lift", "carom_lift", "tool_lift", "tip_rise", "serve_toss_min", "serve_toss_max", "serve_dive_max", "float_brake_from"): return "m/s"
    if "time" in n or n.endswith(("_flight", "_flight_slow")): return "s"
    if n in ("roll_drag",): return "m/s^2"
    if n in ("curve_strength",): return "m/s^2"
    if n in ("float_brake_rate",): return "1/s"
    if any(k in n for k in ("radius", "height", "width", "dist", "band", "margin", "clear", "_h", "rise", "reach", "toss_horiz")): return "m"
    return "x"

LIVE_COPY = {"net_half_width", "antenna_half_width"}

def rng(n, kind, d):
    if kind == "color": return (0, 1)
    if n == "radius": return (0.065, 1.56)
    if n == "gravity": return (0.0, d * 5)
    if n.endswith("_deg"): return (0.0, 180.0)
    if d < 0: return (d * 10, -d * 10)
    if d == 0: return (0.0, 10.0)
    if n in ("floor_bounce",): return (0.0, 1.5)
    if n.endswith(("_keep", "_keep_min", "_keep_rim", "_keep_soft", "_keep_hard")) or "keep" in n: return (0.0, 1.5)
    return (0.0, round(max(d * 5, 1.0), 4))

rows = []
for n, kind, d in exports(f"{root}/ball/ball_physics.gd"):
    lo, hi = rng(n, kind, d)
    rows.append(("ball." + n, section(n), "PHYS", n, kind, d, lo, hi, unit(n, kind), "live_copy" if n in LIVE_COPY else "live"))

feel = dict((n, d) for n, k, d in exports(f"{root}/player/player_feel.gd") if k == "float")
CUR = [
    ("reach", "hit_range", 0.0, 6.0, "m"), ("reach_vertical", "hit_range", 0.0, 4.0, "m"), ("reach_vertical_air", "hit_range", 0.0, 4.0, "m"),
    ("receive_reach_bonus", "hit_range", 0.0, 4.0, "m"), ("spike_air_reach_bonus", "hit_range", 0.0, 4.0, "m"),
    ("dive_reach", "hit_range", 0.0, 6.0, "m"), ("dive_reach_height", "hit_range", 0.0, 3.0, "m"),
    ("set_precharge_reach", "hit_range", 0.0, 15.0, "m"), ("jump_set_down_reach", "hit_range", 0.0, 5.0, "m"),
    ("reach_coyote", "hit_range", 0.0, 1.0, "s"), ("airborne_lost_margin", "hit_range", 0.0, 6.0, "m"),
    ("chest_height", "hit_range", 0.2, 3.0, "m"), ("bump_max_ball_height", "hit_range", 0.0, 6.0, "m"),
    ("spike_min_ball_height", "hit_range", 0.0, 6.0, "m"), ("set_min_ball_height", "hit_range", 0.0, 3.0, "m"),
    ("set_low_height", "hit_range", 0.0, 3.0, "m"), ("kick_height", "hit_range", 0.0, 3.0, "m"), ("touch_lock", "hit_range", 0.0, 1.0, "s"),
    ("block_radius", "hitbox", 0.0, 2.0, "m"), ("block_reach_up", "hitbox", 0.0, 3.0, "m"), ("block_reach_down", "hitbox", 0.0, 4.0, "m"),
    ("block_height", "hitbox", 0.0, 5.0, "m"), ("block_natural_reach", "hitbox", 0.0, 2.0, "m"),
    ("gravity", "player_physics", 0.0, 110.0, "m/s^2"), ("fall_gravity_mult", "player_physics", 0.0, 5.0, "x"),
    ("jump_min_velocity", "player_physics", 0.0, 40.0, "m/s"), ("jump_max_velocity", "player_physics", 0.0, 40.0, "m/s"),
]
for n, sec, lo, hi, u in CUR:
    rows.append(("player." + n, sec, "FEEL", n, "float", feel[n], lo, hi, u, "live"))

EXT = [
    ("flight.air_drag", "drag", 0.0, 0.0, 5.0, "1/s", "air drag: speed x e^(-k dt) each frame in flight (0 = none, the game's own)"),
    ("flight.vertical_drag_scale", "drag", 1.0, 0.0, 2.0, "x", "share of air_drag applied to vertical speed (0 = only horizontal)"),
    ("flight.max_speed", "speed_limits", 0.0, 0.0, 80.0, "m/s", "hard cap on ball speed in flight (0 = off)"),
    ("flight.min_speed", "speed_limits", 0.0, 0.0, 40.0, "m/s", "minimum ball speed in flight (0 = off)"),
    ("flight.curve_scale", "spin_curve", 1.0, 0.0, 5.0, "x", "multiplier on the sideways curve the game gives a shot (Ball.curve at contact)"),
]
for i, sec, d, lo, hi, u, desc in EXT:
    rows.append((i, sec, "EXT", i.split(".")[1], "float", d, lo, hi, u, "live"))




def psection(n):
    if n.startswith(("jump_", "apex_", "bam_")) or n in ("gravity", "fall_gravity_mult", "air_charge_rate", "jump_carry", "jump_sprint_carry"): return "jump"
    if n.startswith(("sprint_", "stamina_")): return "sprint_stamina"
    if n.startswith(("dive_", "tumble_", "knock_", "getup_", "settle_")) or n in ("trip_chance", "trip_sweat_min", "slip_chance_max", "slip_planted_factor", "restless_full"): return "dive_fall"
    if n.startswith("block_"): return "block"
    if n.startswith(("float_", "drive_")) or n.startswith("serve_"): return "serve"
    if n.startswith(("move_", "accel", "plant_", "strafe_", "turn_", "push_")) or n in ("stop_speed", "windup_move_factor", "receive_stance_speed", "receive_plant_speed", "receive_plant", "strafe_receive_heat"): return "movement"
    if n.startswith(("pose_", "recoil_", "crouch_", "land_", "gaze_", "charge_shake", "launch_", "idle_", "receive_pose", "receive_anim")) or n in ("air_lean", "dump_tilt", "hand_mesh_radius", "set_pose"): return "animation"
    if n.endswith("_color") or n.startswith("ring_") or n in ("dive_ring_fade",): return "visuals"
    return "contact_timing"

def pexports(path):
    out = []
    for line in open(path, encoding="utf-8"):
        m = re.match(r"@export var (\w+): = (.+)$", line.strip())
        if not m: continue
        n, v = m.group(1), m.group(2)
        c = re.match(r"Color\(([^)]*)\)", v)
        if c: out.append((n, "color", ([float(x) for x in c.group(1).split(",")] + [1.0])[:4]))
        elif re.match(r"^-?\d+$", v): out.append((n, "int", float(v)))
        elif re.match(r"^-?\d+\.\d+$", v): out.append((n, "float", float(v)))
    return out

prows = []
contact = set(n for n, *_ in CUR)
for n, kind, d in pexports(f"{root}/player/player_feel.gd"):
    if n in contact: continue
    sec = psection(n)
    if kind == "color": lo, hi = 0.0, 1.0
    elif d < 0: lo, hi = d * 10, -d * 10
    elif d == 0: lo, hi = 0.0, 10.0
    else: lo, hi = 0.0, round(max(d * 5, 1.0), 4)
    if n == "gravity": lo, hi = 0.0, 110.0
    prows.append(("player." + n, sec, "FEEL", n, kind, d, lo, hi, unit(n, kind), "live_int" if kind == "int" else "live"))
PEXT = [
    ("player.body_scale", "body", 1.0, 0.25, 4.0, "x", "uniform scale of every Player node (visual + collision); config key player_scale"),
]
for i, sec, d, lo, hi, u, desc in PEXT:
    prows.append((i, sec, "EXT", i.split(".")[1], "float", d, lo, hi, u, "node_scale"))

if "--player" in sys.argv:
    rows = prows
def f(x): return repr(float(x))
print("// GENERATED by tools/gen_goofy_params.py - do not edit by hand.")
print("// GP(id, section, target, prop, is_color, default, r, g, b, a, min, max, unit, apply)")
for (i, sec, tgt, prop, kind, d, lo, hi, u, ap) in rows:
    col = d if kind == "color" else [d, 0, 0, 0]
    print(f'GP("{i}", "{sec}", {tgt}, "{prop}", {"true" if kind=="color" else "false"}, {f(col[0])}, {f(col[1])}, {f(col[2])}, {f(col[3])}, {f(lo)}, {f(hi)}, "{u}", "{ap}")')
