











#pragma once
#include <string>
#include <vector>

namespace jvm::gate {

enum class Context {
	Unknown,
	Offline,
	Lan,
	CustomLobby,
	QuickMatch,
	ServerBrowser,
	Online,
	OfflinePlay,
};

enum class Tier { Full, Cosmetic };

enum class Visibility {
	Unknown,
	Private,
	FriendsOnly,
	Public,
	Invisible,
};

struct Observation {
	std::string source;
	Context context = Context::Unknown;
	Visibility visibility = Visibility::Unknown;
	bool password_protected = false;
	std::string detail;
	bool authoritative = false;

	double timestamp = 0.0;
};

struct Policy {
	double max_age_s = 2.0;
};

struct Decision {
	bool allowed = false;
	std::string reason;
	std::string mode = "unknown";
};

Decision decide(const std::vector<Observation> &obs, double now, const Policy &policy = Policy(), Tier tier = Tier::Full);




std::string mode_label(const std::vector<Observation> &obs, double now, const Policy &policy = Policy());



class GateStateMachine {
public:
	explicit GateStateMachine(double enable_after_s = 0.5) : enable_after_s_(enable_after_s) {}

	bool update(const Decision &d, double now);
	bool active() const { return active_; }
	const std::string &reason() const { return reason_; }
	void force_off(const std::string &why);

private:
	double enable_after_s_;
	bool active_ = false;
	bool pending_ = false;
	double allowed_since_ = 0.0;
	std::string reason_ = "startup (fail closed)";
};


Context context_from_string(const std::string &s);



Visibility visibility_from_string(const std::string &s);
const char *to_string(Context c);
const char *to_string(Visibility v);



bool is_lan_address(const std::string &ip);

}
