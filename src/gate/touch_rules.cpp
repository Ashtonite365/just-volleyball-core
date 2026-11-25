#include "touch_rules.hpp"

namespace jvm::rules {

int clamp_max_hits(int v) { return v <= 0 ? 0 : (v > 10 ? 10 : v); }

int after_touch(Shadow &s, int game_side, int game_count, int max_hits) {



	if (game_side != s.side || game_count <= 1) s.count = game_count < 1 ? 1 : game_count;
	else s.count += 1;
	s.side = game_side;
	max_hits = clamp_max_hits(max_hits);
	if (max_hits == 0 || max_hits == kGameTouches) return -1;


	if (s.count >= max_hits) return kGameTouches;

	return s.count < kGameTouches - 1 ? s.count : kGameTouches - 1;
}

int clamp_consecutive(int v) { return v < 1 ? 1 : (v > 10 ? 10 : v); }
int clamp_team_touches(int v) { return v < 1 ? 1 : (v > 10 ? 10 : v); }

void streak_after_touch(Streak &s, unsigned long long who, int side, int game_count) {
	if (game_count <= 0) { s = Streak(); return; }
	if (game_count == 1 || who != s.who || side != s.side) { s.who = who; s.side = side; s.count = 1; return; }
	s.count++;
}

bool may_touch_again(const Streak &s, bool foul_on, int max_consecutive) {
	if (s.who == 0) return true;
	if (!foul_on) return true;
	return s.count < clamp_consecutive(max_consecutive);
}

}
