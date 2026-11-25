













#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace jvm::pconsts {

struct Spec { const char *name; double stock; double lo, hi; bool is_int; };
const std::vector<Spec> &specs();
const Spec *find(const std::string &name);

enum class Phase { Lobby, Match };
enum class Context { Offline, Host, Client, Public };



std::string manifest_id(const std::string &pin);


using ActiveFn = std::function<bool(const std::string &player, std::string *why)>;

struct Result { bool ok = false; std::string error; };

struct Plan {
	uint64_t match_id = 0;
	bool apply = false;
	std::map<std::string, double> values;
	std::vector<std::pair<std::string, std::string>> missing;
	std::string notice;
};
std::string encode_plan(const Plan &p);
bool decode_plan(const std::string &bytes, Plan &out, std::string *why = nullptr);

constexpr double kEarlyPlanMaxAgeS = 15.0;

class Lock {
	Phase phase_ = Phase::Lobby;
	std::map<std::string, double> saved_;
	Plan plan_;
	bool plan_applied_ = false;
	Plan early_;
	bool has_early_ = false;
	double early_at_ = -1.0;
	uint64_t match_id_ = 0;
	int rejected_ = 0;
	std::vector<std::string> warnings_;
	void warn(const std::string &w);
public:
	Phase phase() const { return phase_; }
	bool locked() const { return phase_ == Phase::Match; }
	int rejected() const { return rejected_; }
	const std::vector<std::string> &warnings() const { return warnings_; }
	const std::map<std::string, double> &saved() const { return saved_; }


	Result set(const std::string &name, double v, const std::string &source);
	Result reset(const std::string &name, const std::string &source);



	Plan begin_match_host(uint64_t match_id, Context ctx, const std::vector<std::string> &players, const ActiveFn &active);



	void begin_match_client(uint64_t match_id, double now_s = -1.0);



	Result on_plan(const std::string &bytes, bool from_host, double now_s = -1.0);


	std::map<std::string, double> values() const;
	const Plan &plan() const { return plan_; }
	bool plan_received() const { return plan_applied_; }

	void clear_pending() { has_early_ = false; early_ = Plan(); early_at_ = -1.0; }


	std::map<std::string, double> end_match();
};

}
