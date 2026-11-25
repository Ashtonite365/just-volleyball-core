


#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

namespace jvm::api {

constexpr int kVersionMajor = 1, kVersionMinor = 3, kVersionPatch = 0;
std::string version_string();

enum class Access { Read, LocalUi, Cosmetic, Gameplay, Sync, Offline };
enum class Scope { Local, Host, Peers };
enum class Status { Impl, Mapped, Planned };

struct Capability {
	const char *id;
	const char *since;
	Access access;
	Scope scope;
	Status status;
	const char *feature;
	const char *game_refs;
	const char *functions;
};

const std::vector<Capability> &capabilities();
const Capability *find(const std::string &id);

const char *to_string(Access a);
const char *to_string(Scope s);
const char *to_string(Status s);


struct GateView {
	bool pin_ok = false;
	bool full = false;
	bool cosmetic = false;
	std::string mode = "unknown";
	std::string full_reason, cosmetic_reason;
};


bool access_open(Access a, const GateView &g, std::string *why = nullptr);
bool is_sync_mode(const std::string &mode);
bool is_offline_mode(const std::string &mode);


bool version_compatible(const std::string &wanted, std::string *why = nullptr);

struct RegisterResult {
	bool ok = false;
	std::string reason;
	std::vector<std::string> granted, denied;
};

struct CheckResult {
	bool ok = false;
	std::string why;
};

class Registry {
public:
	struct Mod {
		std::string name, wanted_version;
		std::set<std::string> declared;
		std::map<std::string, long> calls, refusals;
	};
	RegisterResult register_mod(const std::string &name, const std::string &wanted_version, const std::vector<std::string> &caps);




	CheckResult check(const std::string &mod, const std::string &cap, const GateView &g, bool need_open = true);

	CheckResult peek(const std::string &mod, const std::string &cap, const GateView &g, bool need_open = true) const;
	const std::map<std::string, Mod> &mods() const { return mods_; }
	bool registered(const std::string &mod) const { return mods_.count(mod) != 0; }
	void clear() { mods_.clear(); }

private:
	std::map<std::string, Mod> mods_;
};






struct GoofyParam {
	const char *id, *section, *target, *prop;
	bool is_color;
	double v[4];
	double min, max;
	const char *unit, *apply;
};
const std::vector<GoofyParam> &goofy_params();
int goofy_ball_param_count();
bool goofy_is_player_library(const GoofyParam &p);
const GoofyParam *find_goofy_param(const std::string &id);


struct EventInfo { const char *name; const char *cap; const char *data; const char *when; };
const std::vector<EventInfo> &events();
const EventInfo *find_event(const std::string &name);




bool allowed_signal(const std::string &domain, const std::string &name);
const std::vector<std::string> &allowed_signals(const std::string &domain);



const char *library_path(const std::string &lib);


const std::vector<std::string> &mode_labels();

const std::set<std::string> &migrated_libraries();


bool library_allowed(const std::string &short_name, bool strict, std::string *why = nullptr);

struct Coverage {
	int total = 0, impl = 0, mapped = 0, planned = 0;
	int game_refs = 0;
	int used_by_mods = 0;
	std::map<std::string, int> by_access;
};
Coverage coverage(const Registry *r = nullptr);

}
