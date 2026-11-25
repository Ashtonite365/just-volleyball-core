
#pragma once
#include <string>
#include <vector>
#include <functional>

namespace jvm::shots {

struct Vec3 { double x = 0, y = 0, z = 0; };



struct Combo {
	std::string trigger = "touch";
	std::vector<std::string> hold;
	std::string context = "rally";
	std::string touch = "any";
	std::string air = "any";
	double min_power = -1.0, max_power = 99.0;
	bool curve = true;
};

struct TouchState {
	bool serving = false;
	bool set_touch = false;
	bool airborne = false;
	double power = 0.0;
	std::vector<std::string> held;
	bool forward_armed = false;
};

struct ShotDef {
	std::string id;
	std::string label;
	bool is_serve = false;
	double speed = 10.0;
	double elevation_deg = 20.0;
	double topspin_rps = 0.0;
	double float_strength = 0.0;
	double float_duration_s = 0.0;
	std::string default_key;
	double curve = 0.0;
	bool side_spin = false;
	bool rally_float = false;
	Combo combo;
};


struct SpinParams {
	double max_lateral = 6.0;
	double spread_long = 1.0;
	double spread_short = 0.35;
	double quality_spread = 1.0;
	double extreme_spread = 0.6;
	double extreme_from = 0.7;
	double curve_strength = 12.0;
	bool range_box = true;

	double bar_forward = 1.5;
	double k_min = 0.3;
	double k_max = 4.0;


	bool even_bend = true;
};
struct Court { double center_x = 0.0, half_w = 4.5, half_l = 9.0; };


Vec3 spin_target(Vec3 aim, Vec3 spin_dir, double charge01, const SpinParams &p, const Court &c);






double spin_k(double charge01, const SpinParams &p);
double spin_g(double k, double s);
double spin_g_slope(double k, double s);
struct SpinPath {
	Vec3 c, f, d; double F = 0, L = 0, k = 0;
	bool even = false;
	double g(double s) const;
	double g_slope(double s) const;
	double s_for(double charge01) const;
	double k_time() const { return even ? 0.0 : k; }
	Vec3 at(double s) const;
	Vec3 tangent(double s) const;
	double lateral(double s) const { return L * g(s); }
};
SpinPath spin_path(Vec3 aim, Vec3 forward, Vec3 spin_dir, double charge01, const SpinParams &p, const Court &c);


struct BarShard { Vec3 tip; Vec3 point; };
std::vector<BarShard> spin_bar_shards(const SpinPath &path, double w, double spacing);

double ramp_curve(double A, double k, double T, double t);




struct RampSolve { Vec3 aim; double A = 0, err = 1e9, defl_per_A = 0; int iters = 0; };
RampSolve solve_ramp(const std::function<Vec3(Vec3, double)> &land, Vec3 want, Vec3 d, double lat, double a_max = 80.0);

double spin_spread_scale(double quality, double charge01, const SpinParams &p);



Vec3 float_side_target(Vec3 contact, Vec3 aim, Vec3 player_fwd, int side, double lateral);





struct TopspinSolve { Vec3 vel; double dive_g = 1.0, apex = 0.0, net_y = 0.0, rise = 0.0; bool crosses_net = false; };
TopspinSolve solve_topspin(Vec3 from, Vec3 target, Vec3 v_normal, double g, double land_y, double net_clear_y, double speed_k, double rise, double dt = 0.0);








constexpr double kSpikeBoostDefault = 1.12, kSpikeBoostMin = 1.0, kSpikeBoostMax = 1.3;
constexpr int kTouchSpike = 3;
double clamp_spike_boost(double v);



constexpr int kTouchDump = 4;
bool yields_to_game_dump(const ShotDef &s, int touch_kind);
bool spike_boost_applies(const ShotDef &s, bool serve_contact, int touch_kind, bool airborne, bool forward_held);

double fhash(long long seed, int i);



Vec3 spin_landing(Vec3 target, Vec3 spin_dir, double scale, const SpinParams &p, long long seed);


int select_shot(const std::vector<ShotDef> &shots, const TouchState &t);

int combo_specificity(const Combo &c);

bool combos_ambiguous(const Combo &a, const Combo &b);

std::string combo_game_clash(const Combo &c);




double camera_curve(int side, Vec3 cam_right, Vec3 flight, double amount);




Vec3 camera_relative_move(Vec3 cam_x, Vec3 cam_z, double raw_x, double raw_y);




struct MoveIntent { bool forward = false; int side = 0; bool back = false; };
MoveIntent move_intent(double raw_x, double raw_y);






struct FrameRaw { double x = 0, y = 0; Vec3 net_dir, net_right, world; };
FrameRaw hitter_frame_raw(Vec3 cam_x, Vec3 cam_z, double raw_x, double raw_y, Vec3 net_dir);

Vec3 hitter_net_dir(Vec3 player, Vec3 aim, int team_side);

double world_curve(Vec3 world_side, Vec3 flight, double amount);



struct GameLaunch { Vec3 vel; double curve = 0; double dive_g = 1; bool start_float = false; double float_charge = 0; };
GameLaunch game_launch(const ShotDef &s, Vec3 forward);

bool touch_matches(const ShotDef &s, bool serving_recently);

bool parse_key_name(const std::string &name, int &kp_digit);



std::vector<ShotDef> default_shots();



Vec3 launch_velocity(const ShotDef &s, Vec3 forward);

Vec3 launch_spin(const ShotDef &s, Vec3 forward);

Vec3 float_accel(const ShotDef &s, Vec3 forward, double t);

}
