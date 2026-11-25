





#pragma once

namespace jvm::rules {

constexpr int kGameTouches = 3;

struct Shadow { int side = 0; int count = 0; };





int after_touch(Shadow &s, int game_side, int game_count, int max_hits);


int clamp_max_hits(int v);






constexpr int kGameConsecutive = 1;
struct Streak { unsigned long long who = 0; int side = 0; int count = 0; };


void streak_after_touch(Streak &s, unsigned long long who, int side, int game_count);


bool may_touch_again(const Streak &s, bool foul_on, int max_consecutive);
int clamp_consecutive(int v);
int clamp_team_touches(int v);

inline int team_touches_to_max_hits(int v) { v = clamp_team_touches(v); return v == kGameTouches ? 0 : v; }

}
