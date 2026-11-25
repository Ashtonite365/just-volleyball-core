#include "shot_math.hpp"
#include <cmath>
#include <algorithm>

namespace jvm::shots {

static const double kPi = 3.14159265358979323846;
static Vec3 flat_unit(Vec3 f);

static Combo C(const char *trigger, std::vector<std::string> hold, const char *ctx, const char *touch, const char *air, double minp = -1.0, double maxp = 99.0) {
	Combo c; c.trigger = trigger; c.hold = std::move(hold); c.context = ctx; c.touch = touch; c.air = air; c.min_power = minp; c.max_power = maxp;
	return c;
}

std::vector<ShotDef> default_shots() {

	std::vector<ShotDef> v = {
		{ "high_shot",      "High shot",      false, 11.0, 55.0,  0.0, 0.0, 0.0, "KEY_KP_1" },
		{ "low_shot",       "Low shot",       false, 15.0, 8.0,   0.0, 0.0, 0.0, "KEY_KP_2" },
		{ "topspin",        "Topspin",        false, 16.0, 12.0,  6.0, 0.0, 0.0, "KEY_KP_3" },
		{ "standing_float", "Standing float", true,  14.0, 14.0,  0.0, 2.5, 1.6, "KEY_KP_4" },
		{ "float_serve",    "Float serve",    true,  16.0, 12.0,  0.0, 3.0, 1.4, "KEY_KP_5" },
		{ "high_serve",     "High serve",     true,  12.0, 60.0,  1.0, 0.0, 0.0, "KEY_KP_6" },
		{ "low_serve",      "Low serve",      true,  15.0, 7.0,   0.0, 0.0, 0.0, "KEY_KP_7" },
		{ "fast_serve",     "Fast serve",     true,  22.0, 6.0,   2.0, 0.0, 0.0, "KEY_KP_8" },
		{ "jump_serve",     "Jump serve",     true,  24.0, -4.0,  8.0, 0.0, 0.0, "KEY_KP_9" },
	};


	v[0].combo = C("touch", { "sprint", "strafe" }, "rally", "set", "any");
	v[1].combo = C("touch", { "sprint", "strafe" }, "rally", "not_set", "any");

	v[2].combo = C("touch", { "move_forward" }, "rally", "any", "air");


	v[3].combo = C("touch", { "sprint", "move_back" }, "serve", "any", "ground");
	v[4].combo = C("touch", { "sprint", "move_back" }, "serve", "any", "air");
	v[3].combo.curve = v[4].combo.curve = false;
	v[5].combo = C("touch", { "sprint" }, "serve", "any", "ground", -1.0, 0.4);
	v[6].combo = C("touch", { "sprint" }, "serve", "any", "ground", 0.4, 0.95);
	v[7].combo = C("touch", { "sprint" }, "serve", "any", "ground", 0.95, 99.0);
	v[8].combo = C("touch", { "sprint" }, "serve", "any", "air");



	ShotDef ss; ss.id = "curveball"; ss.label = "Curveball"; ss.is_serve = false; ss.side_spin = true;
	ss.combo = C("touch", { "side" }, "rally", "any", "air");
	v.push_back(ss);




	ShotDef fl; fl.id = "float"; fl.label = "Float"; fl.is_serve = false; fl.rally_float = true;
	fl.combo = C("touch", { "back" }, "rally", "not_set", "air");
	fl.combo.curve = false;
	v.push_back(fl);
	return v;
}

static bool has(const std::vector<std::string> &v, const std::string &x) {
	for (auto &e : v) if (e == x) return true;
	return false;
}

int combo_specificity(const Combo &c) {
	return int(c.hold.size()) * 10 + (c.touch != "any") * 2 + (c.air != "any") * 2 + (c.min_power > -1.0 || c.max_power < 99.0) + (c.trigger == "forward_keys") * 50;
}

static bool combo_matches(const Combo &c, const TouchState &t) {
	if ((c.context == "serve") != t.serving) return false;
	if (c.trigger == "forward_keys" && !t.forward_armed) return false;
	for (auto &h : c.hold) if (!has(t.held, h)) return false;
	if (c.touch == "set" && !t.set_touch) return false;
	if (c.touch == "not_set" && t.set_touch) return false;
	if (c.air == "air" && !t.airborne) return false;
	if (c.air == "ground" && t.airborne) return false;
	if (t.power < c.min_power || t.power >= c.max_power) return false;
	return true;
}

int select_shot(const std::vector<ShotDef> &shots, const TouchState &t) {
	int best = -1, best_spec = -1;
	for (size_t i = 0; i < shots.size(); i++) {
		if (shots[i].combo.trigger == "off" || !combo_matches(shots[i].combo, t)) continue;
		int sp = combo_specificity(shots[i].combo);
		if (sp > best_spec) { best = int(i); best_spec = sp; }
	}
	return best;
}

bool combos_ambiguous(const Combo &a, const Combo &b) {
	if (a.trigger == "off" || b.trigger == "off") return false;
	if (a.context != b.context || combo_specificity(a) != combo_specificity(b)) return false;
	if (a.trigger != b.trigger) return false;
	auto disjoint = [](const std::string &x, const std::string &y, const char *p, const char *q) {
		return (x == p && y == q) || (x == q && y == p);
	};
	if (disjoint(a.touch, b.touch, "set", "not_set")) return false;
	if (disjoint(a.air, b.air, "air", "ground")) return false;
	if (a.max_power <= b.min_power || b.max_power <= a.min_power) return false;

	if ((has(a.hold, "move_forward") && has(b.hold, "side")) || (has(b.hold, "move_forward") && has(a.hold, "side"))) return false;

	for (const char *o : { "move_forward", "side" })
		if ((has(a.hold, "back") && has(b.hold, o)) || (has(b.hold, "back") && has(a.hold, o))) return false;

	return true;
}

std::string combo_game_clash(const Combo &c) {
	for (auto &h : c.hold) {
		if (h == "move_forward" && !(c.context == "rally" && c.air == "air"))
			return "move_forward is walking: only usable for an airborne rally touch (topspin)";
		if (h == "move_left" || h == "move_right") return "left/right are the curve input (use \"side\")";
		if (h == "side" && !(c.context == "rally" && c.air == "air"))
			return "left/right on the ground is walking: the Curveball is airborne rally only";
		if (h == "back" && !(c.context == "rally" && c.air == "air"))
			return "back on the ground is walking: the Float is airborne rally only";
		if (h == "move_back" && c.context == "rally")
			return "move_back in a rally is walking backwards: use \"back\" (airborne Float)";
		if (h == "contact" || h == "set" || h == "jump" || h == "dive" || h == "block")
			return h + " is a touch/jump/dive action: use \"touch\"/\"air\" instead of holding it";
		if (h == "serve" || h == "ball_spawn") return "serve tosses the ball (input_collector.gd:305)";
		if (h.rfind("callout_", 0) == 0 || h == "chat_toggle" || h == "stats" || h == "free_mouse" || h == "camera_toggle" || h == "camera_zoom" || h == "aim_recenter")
			return h + " has its own effect on press";
	}
	if (c.context == "serve" && has(c.hold, "strafe")) return "strafe during the serve drops the serve charge (player.gd:3660)";
	if (c.context == "rally" && c.trigger == "touch" && has(c.hold, "sprint") && !has(c.hold, "strafe"))
		return "sprint alone is normal running into the ball";
	return "";
}

double camera_curve(int side, Vec3 cam_right, Vec3 flight, double amount) {
	if (side == 0) return 0.0;
	Vec3 r = flat_unit(cam_right);
	Vec3 d = flat_unit(flight);

	Vec3 left = { d.z, 0, -d.x };

	return amount * side * (left.x * r.x + left.z * r.z);
}

Vec3 camera_relative_move(Vec3 cx, Vec3 cz, double rx, double ry) {
	Vec3 fwd = { -cz.x, 0, -cz.z }, right = { cx.x, 0, cx.z };
	double lf = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z), lr = std::sqrt(right.x * right.x + right.z * right.z);
	if (lf > 1e-9) { fwd.x /= lf; fwd.z /= lf; }
	if (lr > 1e-9) { right.x /= lr; right.z /= lr; }
	return { right.x * rx + fwd.x * -ry, 0, right.z * rx + fwd.z * -ry };
}

MoveIntent move_intent(double rx, double ry) {
	MoveIntent m;
	m.forward = -ry > 0.5;
	m.side = rx > 0.5 ? 1 : (rx < -0.5 ? -1 : 0);
	m.back = ry > 0.5 && ry >= std::fabs(rx);
	return m;
}

FrameRaw hitter_frame_raw(Vec3 cam_x, Vec3 cam_z, double rx, double ry, Vec3 net_dir) {
	FrameRaw f;
	f.net_dir = flat_unit(net_dir);
	f.net_right = { -f.net_dir.z, 0.0, f.net_dir.x };
	f.world = camera_relative_move(cam_x, cam_z, rx, ry);
	f.x = f.world.x * f.net_right.x + f.world.z * f.net_right.z;
	f.y = -(f.world.x * f.net_dir.x + f.world.z * f.net_dir.z);
	return f;
}

Vec3 hitter_net_dir(Vec3 p, Vec3 aim, int team_side) {
	double sd = team_side != 0 ? double(team_side) : (p.z >= 0 ? 1.0 : -1.0);
	double dx = aim.x - p.x, dz = aim.z - p.z, l = std::sqrt(dx * dx + dz * dz);


	if (aim.x == aim.x && l > 0.75 && l < 1e6 && aim.z * sd < 0.0) return { dx / l, 0.0, dz / l };
	return { 0.0, 0.0, -sd };
}

double world_curve(Vec3 w, Vec3 flight, double amount) {
	double l = std::sqrt(w.x * w.x + w.z * w.z);
	if (l < 1e-9) return 0.0;
	Vec3 d = flat_unit(flight);
	Vec3 left = { d.z, 0, -d.x };
	return amount * (left.x * w.x + left.z * w.z) / l;
}

Vec3 spin_target(Vec3 aim, Vec3 dir, double charge, const SpinParams &p, const Court &c) {
	Vec3 d = flat_unit(dir);
	double k = std::fmax(0.0, std::fmin(1.0, charge)) * p.max_lateral;

	const double side = aim.z >= 0 ? 1.0 : -1.0;
	const double x0 = c.center_x - c.half_w, x1 = c.center_x + c.half_w;
	const double z0 = side > 0 ? 0.3 : -std::fmax(0.3, c.half_l), z1 = side > 0 ? std::fmax(0.3, c.half_l) : -0.3;
	Vec3 a = { std::fmax(x0, std::fmin(x1, aim.x)), 0.0, std::fmax(z0, std::fmin(z1, aim.z)) };



	auto limit = [&](double pos, double dv, double lo, double hi) {
		if (dv > 1e-9) k = std::fmin(k, (hi - pos) / dv);
		else if (dv < -1e-9) k = std::fmin(k, (lo - pos) / dv);
	};
	limit(a.x, d.x, x0, x1);
	limit(a.z, d.z, z0, z1);
	k = std::fmax(0.0, k);
	return { a.x + d.x * k, 0.0, a.z + d.z * k };
}

double spin_k(double charge01, const SpinParams &p) {
	double c = std::fmax(0.0, std::fmin(1.0, charge01));
	return p.k_min + (p.k_max - p.k_min) * c;
}
double spin_g(double k, double s) {
	s = std::fmax(0.0, std::fmin(1.0, s));
	if (std::fabs(k) < 1e-6) return s;
	return std::expm1(k * s) / std::expm1(k);
}
double spin_g_slope(double k, double s) {
	s = std::fmax(0.0, std::fmin(1.0, s));
	if (std::fabs(k) < 1e-6) return 1.0;
	return k * std::exp(k * s) / std::expm1(k);
}
double SpinPath::g(double s) const {
	if (!even) return spin_g(k, s);
	s = std::fmax(0.0, std::fmin(1.0, s));
	return s * s;
}
double SpinPath::g_slope(double s) const {
	if (!even) return spin_g_slope(k, s);
	return 2.0 * std::fmax(0.0, std::fmin(1.0, s));
}
double SpinPath::s_for(double charge) const {
	double c = std::fmax(0.0, std::fmin(1.0, charge));
	return even ? std::sqrt(c) : c;
}
Vec3 SpinPath::at(double s) const {
	double g = this->g(s);
	s = std::fmax(0.0, std::fmin(1.0, s));
	return { c.x + f.x * F * s + d.x * L * g, 0.0, c.z + f.z * F * s + d.z * L * g };
}
Vec3 SpinPath::tangent(double s) const {
	double gs = g_slope(s);
	return flat_unit({ f.x * F + d.x * L * gs, 0.0, f.z * F + d.z * L * gs });
}
SpinPath spin_path(Vec3 aim, Vec3 forward, Vec3 dir, double charge, const SpinParams &p, const Court &c) {
	SpinPath P;
	P.d = flat_unit(dir);
	P.f = flat_unit(forward);
	P.k = spin_k(charge, p);
	P.even = p.even_bend;
	const double side = aim.z >= 0 ? 1.0 : -1.0;
	const double x0 = c.center_x - c.half_w, x1 = c.center_x + c.half_w;
	const double z0 = side > 0 ? 0.3 : -std::fmax(0.3, c.half_l), z1 = side > 0 ? std::fmax(0.3, c.half_l) : -0.3;
	P.c = { std::fmax(x0, std::fmin(x1, aim.x)), 0.0, std::fmax(z0, std::fmin(z1, aim.z)) };
	auto inside = [&](double F, double L) {
		SpinPath q = P; q.F = F; q.L = L;
		for (int i = 0; i <= 24; i++) {
			Vec3 a = q.at(i / 24.0);
			if (a.x < x0 - 1e-6 || a.x > x1 + 1e-6 || a.z < z0 - 1e-6 || a.z > z1 + 1e-6) return false;
		}
		return true;
	};
	double lo = 0.0, hi = 1.0;
	if (inside(p.bar_forward, p.max_lateral)) lo = 1.0;
	else for (int i = 0; i < 30; i++) { double m = 0.5 * (lo + hi); (inside(p.bar_forward * m, p.max_lateral * m) ? lo : hi) = m; }
	P.F = p.bar_forward * lo; P.L = p.max_lateral * lo;
	if (lo < 1.0) {




		double ll = 0.0, lh = 1.0;
		for (int i = 0; i < 30; i++) { double m = 0.5 * (ll + lh); (inside(0.0, p.max_lateral * m) ? ll : lh) = m; }
		if (ll > lo + 1e-3) {
			double fl = 0.0, fh = 1.0;
			for (int i = 0; i < 30; i++) { double m = 0.5 * (fl + fh); (inside(p.bar_forward * m, p.max_lateral * ll) ? fl : fh) = m; }
			P.F = p.bar_forward * fl; P.L = p.max_lateral * ll;
		}
	}
	return P;
}
std::vector<BarShard> spin_bar_shards(const SpinPath &path, double w, double spacing) {
	std::vector<BarShard> out;

	double len = 0.0; Vec3 prev = path.at(0);
	for (int i = 1; i <= 32; i++) { Vec3 a = path.at(i / 32.0); len += std::hypot(a.x - prev.x, a.z - prev.z); prev = a; }
	int n = std::max(1, int(std::ceil(len / std::fmax(0.2, spacing))));
	for (int i = 0; i <= n; i++) {
		double s = double(i) / n;
		Vec3 a = path.at(s), t = path.tangent(s);
		if (t.x == 0 && t.z == 0) t = path.f;
		Vec3 nl = { -t.z, 0.0, t.x };
		for (int e = -1; e <= 1; e += 2) {
			Vec3 tip = { a.x + nl.x * w * e, 0.0, a.z + nl.z * w * e };
			Vec3 pt = { -nl.x * e, 0.0, -nl.z * e };
			if (i == 0) pt = flat_unit({ pt.x + t.x, 0.0, pt.z + t.z });
			else if (i == n) pt = flat_unit({ pt.x - t.x, 0.0, pt.z - t.z });
			out.push_back({ tip, pt });
		}
	}
	return out;
}
double ramp_curve(double A, double k, double T, double t) {
	if (T <= 1e-6) return A;
	return A * std::exp(k * std::fmax(0.0, std::fmin(t, T)) / T);
}

RampSolve solve_ramp(const std::function<Vec3(Vec3, double)> &land, Vec3 want, Vec3 dir, double lat, double a_max) {
	RampSolve r;
	Vec3 d = flat_unit(dir);
	Vec3 aim = { want.x - d.x * lat, 0.0, want.z - d.z * lat };
	double A = 0.0;
	Vec3 best = aim; double bestA = 0.0;
	auto newton_aim = [&](int iters) {
		for (int it = 0; it < iters; it++) {
			Vec3 p = land(aim, A);
			Vec3 e = { want.x - p.x, 0.0, want.z - p.z };
			double el = std::hypot(e.x, e.z);
			r.iters++;
			if (el < r.err) { r.err = el; best = aim; bestA = A; }
			if (el < 0.02) return;
			const double h = 0.25;
			Vec3 px = land({ aim.x + h, 0.0, aim.z }, A), pz = land({ aim.x, 0.0, aim.z + h }, A);
			double j11 = (px.x - p.x) / h, j21 = (px.z - p.z) / h, j12 = (pz.x - p.x) / h, j22 = (pz.z - p.z) / h;
			double det = j11 * j22 - j12 * j21, sx, sz;
			if (std::fabs(det) < 1e-6) { sx = e.x; sz = e.z; }
			else { sx = (j22 * e.x - j12 * e.z) / det; sz = (-j21 * e.x + j11 * e.z) / det; }
			double sl = std::hypot(sx, sz);
			if (sl > 4.0) { sx *= 4.0 / sl; sz *= 4.0 / sl; }
			double t = 1.0; Vec3 next = { aim.x + sx, 0.0, aim.z + sz };
			for (int bt = 0; bt < 5; bt++) {
				Vec3 q = land(next, A);
				if (std::hypot(want.x - q.x, want.z - q.z) < el) break;
				t *= 0.5; next = { aim.x + sx * t, 0.0, aim.z + sz * t };
			}
			aim = next;
		}
	};

	auto bend_A = [&]() {
		if (std::fabs(lat) < 1e-3) return 0.0;
		Vec3 l0 = land(aim, 0.0);
		auto f = [&](double a) { Vec3 l = land(aim, a); return (l.x - l0.x) * d.x + (l.z - l0.z) * d.z - lat; };
		double a0 = 0.0, f0 = -lat, a1 = 0.5, f1 = f(a1);
		r.defl_per_A = (f1 - f0) / (a1 - a0);
		for (int i = 0; i < 10 && std::fabs(f1) > 0.02; i++) {
			if (std::fabs(f1 - f0) < 1e-9) break;
			double a2 = a1 - f1 * (a1 - a0) / (f1 - f0);
			a2 = std::fmax(0.0, std::fmin(a_max, a2));
			a0 = a1; f0 = f1; a1 = a2; f1 = f(a1);
		}
		return a1;
	};
	for (int outer = 0; outer < 3; outer++) {
		A = bend_A();
		newton_aim(8);
		if (r.err < 0.02) break;
	}
	r.aim = best; r.A = bestA;
	return r;
}

Vec3 float_side_target(Vec3 contact, Vec3 aim, Vec3 player_fwd, int side, double lateral) {
	Vec3 f = flat_unit(player_fwd);
	Vec3 a = { aim.x, 0.0, aim.z };


	Vec3 h = flat_unit({ a.x - contact.x, 0.0, a.z - contact.z });
	if ((h.x != 0 || h.z != 0) && f.x * h.x + f.z * h.z < 0.5) f = h;
	if (side == 0 || (f.x == 0 && f.z == 0)) return a;
	Vec3 right = { -f.z, 0.0, f.x };
	Vec3 t = { a.x + right.x * side * lateral, 0.0, a.z + right.z * side * lateral };
	double rx = a.x - contact.x, rz = a.z - contact.z, range = std::sqrt(rx * rx + rz * rz);
	double tx = t.x - contact.x, tz = t.z - contact.z, tl = std::sqrt(tx * tx + tz * tz);
	if (tl < 1e-6 || range < 1e-6) return t;
	return { contact.x + tx / tl * range, 0.0, contact.z + tz / tl * range };
}

TopspinSolve solve_topspin(Vec3 from, Vec3 target, Vec3 v_normal, double g, double land_y, double net_clear_y, double speed_k, double rise, double dt) {
	TopspinSolve r;
	double dx = target.x - from.x, dz = target.z - from.z, D = std::sqrt(dx * dx + dz * dz);
	double ux = D > 1e-6 ? dx / D : 0.0, uz = D > 1e-6 ? dz / D : -1.0;
	double vh_n = std::sqrt(v_normal.x * v_normal.x + v_normal.z * v_normal.z);
	double y0 = from.y;
	double apex_n = v_normal.y > 0 ? y0 + v_normal.y * v_normal.y / (2 * g) : y0;
	double vh = std::fmax(1.0, vh_n * speed_k);
	double t = std::fmax(0.05, D / vh);

	r.crosses_net = (from.z > 0) != (target.z > 0) && std::fabs(dz) > 1e-6;
	double fn = r.crosses_net ? from.z / (from.z - target.z) : -1.0;
	for (double extra = 0.0; extra <= 4.0 + 1e-9; extra += 0.1) {
		double H = std::fmax(0.05, apex_n + rise + extra - y0);
		double vy = 2.0 * H * (1.0 + std::sqrt(std::fmax(0.0, 1.0 + (y0 - land_y) / H))) / t;
		double gp = vy * vy / (2.0 * H);
		if (gp < g) { gp = g; vy = (land_y - y0 + 0.5 * g * t * t) / t; }
		double ny = 0.0;
		if (r.crosses_net) { double tn = fn * t; ny = y0 + vy * tn - 0.5 * gp * tn * tn; }


		r.vel = { ux * vh, vy + 0.5 * gp * dt, uz * vh };
		r.dive_g = gp / g;
		r.apex = vy > 0 ? y0 + vy * vy / (2 * gp) : y0;
		r.net_y = ny;
		r.rise = r.apex - apex_n;
		if (!r.crosses_net || ny >= net_clear_y) break;
	}
	return r;
}

double clamp_spike_boost(double v) {
	if (!std::isfinite(v)) return kSpikeBoostDefault;
	return std::fmax(kSpikeBoostMin, std::fmin(kSpikeBoostMax, v));
}

bool yields_to_game_dump(const ShotDef &s, int touch_kind) {
	return touch_kind == kTouchDump && has(s.combo.hold, "move_forward");
}

bool spike_boost_applies(const ShotDef &s, bool serve_contact, int touch_kind, bool airborne, bool forward_held) {
	if (s.is_serve || serve_contact) return false;
	if (!(s.topspin_rps > 0.0) || s.side_spin || s.rally_float) return false;
	if (touch_kind != kTouchSpike) return false;
	return airborne && forward_held;
}

double spin_spread_scale(double q, double charge, const SpinParams &p) {
	q = std::fmax(0.0, std::fmin(1.0, q));
	double ext = std::fabs(2.0 * std::fmax(0.0, std::fmin(1.0, charge)) - 1.0);
	double e = p.extreme_from < 1.0 ? std::fmax(0.0, ext - p.extreme_from) / (1.0 - p.extreme_from) : 0.0;
	return (1.0 + p.quality_spread * (1.0 - q)) * (1.0 + p.extreme_spread * e);
}

double fhash(long long seed, int i) {
	const long long M = 2147483647LL;
	long long x = ((seed < 0 ? -seed : seed) * 374761393LL + (long long)(i + 1) * 668265263LL) & M;
	x = ((x ^ (x >> 13)) * 1274126177LL) & M;
	x = (x ^ (x >> 16)) & M;
	return double(x) / double(M);
}

Vec3 spin_landing(Vec3 t, Vec3 dir, double scale, const SpinParams &p, long long seed) {
	Vec3 d = flat_unit(dir);
	Vec3 across = { -d.z, 0, d.x };
	double u = fhash(seed, 0) + fhash(seed, 1) - 1.0;
	double v = fhash(seed, 2) + fhash(seed, 3) - 1.0;
	double L = p.spread_long * scale, S = p.spread_short * scale;
	return { t.x + d.x * u * L + across.x * v * S, 0.0, t.z + d.z * u * L + across.z * v * S };
}

static Vec3 flat_unit(Vec3 f) {
	double len = std::sqrt(f.x * f.x + f.z * f.z);
	if (len < 1e-9) return { 0, 0, -1 };
	return { f.x / len, 0, f.z / len };
}

Vec3 launch_velocity(const ShotDef &s, Vec3 forward) {
	Vec3 f = flat_unit(forward);
	double e = s.elevation_deg * kPi / 180.0;
	double h = s.speed * std::cos(e);
	return { f.x * h, s.speed * std::sin(e), f.z * h };
}

Vec3 launch_spin(const ShotDef &s, Vec3 forward) {
	Vec3 f = flat_unit(forward);

	Vec3 right = { -f.z, 0, f.x };
	double w = s.topspin_rps * 2.0 * kPi;


	return { -right.x * w, 0, -right.z * w };
}

Vec3 float_accel(const ShotDef &s, Vec3 forward, double t) {
	if (s.float_strength <= 0.0 || t < 0.0 || t > s.float_duration_s) return { 0, 0, 0 };
	Vec3 f = flat_unit(forward);
	Vec3 right = { -f.z, 0, f.x };
	double a = s.float_strength * std::sin(t * 2.0 * kPi * 1.7) ;
	double v = 0.4 * s.float_strength * std::sin(t * 2.0 * kPi * 2.3 + 1.0);
	return { right.x * a, v, right.z * a };
}

GameLaunch game_launch(const ShotDef &s, Vec3 forward) {
	GameLaunch g;
	g.vel = launch_velocity(s, forward);
	g.curve = s.curve;


	g.dive_g = s.topspin_rps > 0.0 ? std::fmin(2.5, 1.0 + 0.12 * s.topspin_rps) : 1.0;


	g.start_float = s.float_strength > 0.0;
	g.float_charge = std::fmax(0.0, std::fmin(1.0, 1.0 - s.float_strength / 4.0));
	return g;
}

bool touch_matches(const ShotDef &s, bool serving_recently) {
	return s.is_serve == serving_recently;
}

bool parse_key_name(const std::string &name, int &kp_digit) {

	std::string n = name;
	if (n.rfind("KEY_", 0) == 0) n = n.substr(4);
	if (n.rfind("KP_", 0) == 0 && n.size() == 4 && n[3] >= '0' && n[3] <= '9') { kp_digit = n[3] - '0'; return true; }
	return false;
}

}
