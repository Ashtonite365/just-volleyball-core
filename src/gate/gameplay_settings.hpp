













#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace jvm::gs {

enum class Type { Bool, Int, Float, Enum, Vec };
enum class Role { Off, Local, Client };

using Value = std::vector<double>;

struct Def {
	std::string id;
	std::string mod;
	Type type = Type::Float;
	int dim = 1;
	Value def;
	double min = 0, max = 1, step = 0;
	std::vector<std::string> options;
	std::string label, tooltip, category;
	int order = 0;
};

const char *to_string(Type t);
bool parse_type(const std::string &s, Type &out);
const char *to_string(Role r);

std::string check_id(const std::string &mod, const std::string &id);

std::string check_sync_class(const std::string &sync);

std::string check_def(Def &d);


bool sanitize(const Def &d, const double *in, int n, Value &out, bool *clamped = nullptr);
inline bool sanitize(const Def &d, const Value &in, Value &out, bool *clamped = nullptr) { return sanitize(d, in.data(), int(in.size()), out, clamped); }

struct Result {
	bool ok = false;
	std::string error;
};
struct ApplyResult {
	bool ok = false;
	int applied = 0, rejected = 0, unknown = 0;
	std::vector<std::string> errors;
};

constexpr int kSnapshotVersion = 1;
constexpr int kSnapshotMaxBytes = 32768;
constexpr int kSnapshotMaxEntries = 1024;
constexpr int kMaxSettings = 2048;









using Manifest = std::map<std::string, std::string>;
struct PeerInfo {
	Manifest mods;
	bool loader_ok = true;
};
constexpr int kManifestVersion = 1;
constexpr int kManifestMaxBytes = 8192;
constexpr int kManifestMaxMods = 256;

std::string encode_manifest(const Manifest &mods, bool loader_ok, const std::set<std::string> *disabled = nullptr);
bool decode_manifest(const std::string &bytes, Manifest &mods, bool &loader_ok, std::string *why = nullptr, std::set<std::string> *disabled = nullptr);
struct Missing { std::string player, why; };

std::string version_major(const std::string &v);


bool mod_active(const std::string &mod, const std::string &host_version, const Manifest &player, std::string *why = nullptr);

class Registry {
	std::map<std::string, Def> defs_;
	std::vector<std::string> order_;
	std::map<std::string, Value> local_, host_;
	Role role_ = Role::Off;
	Manifest local_mods_, host_mods_;
	std::set<std::string> host_disabled_;
	bool host_mods_known_ = false;
	bool match_locked_ = false;
	std::map<std::string, std::vector<Missing>> disabled_;
	std::set<std::string> match_players_;
	std::map<std::string, PeerInfo> peers_;
	uint64_t revision_ = 0;

public:


	Result add(Def d, const std::string &sync_class);
	int remove_mod(const std::string &mod);
	const Def *find(const std::string &id) const;
	const std::vector<std::string> &ids() const { return order_; }
	std::vector<std::string> mods() const;
	uint64_t revision() const { return revision_; }


	Result set_local(const std::string &id, const Value &raw);
	bool reset_local(const std::string &id);
	const Value *local(const std::string &id) const;





	void set_role(Role r) {
		role_ = r;
		if (r == Role::Local) { host_.clear(); clear_host_mods(); }
		if (r == Role::Client) { clear_peers(); unlock_match(); }
	}
	Role role() const { return role_; }


	Value get(const std::string &id, std::string *source = nullptr) const;

	bool locked() const { return role_ == Role::Client || (role_ == Role::Local && match_locked_); }



	std::map<std::string, Value> snapshot() const;
	static std::string encode(const std::map<std::string, Value> &values);
	static uint64_t hash(const std::string &bytes);


	ApplyResult apply_host(const std::string &bytes);
	ApplyResult apply_host_values(const std::map<std::string, Value> &raw);
	void clear_host() { host_.clear(); }


	void set_local_mods(const Manifest &m) { local_mods_ = m; }
	const Manifest &local_mods() const { return local_mods_; }

	void set_host_mods(const Manifest &m, const std::set<std::string> &disabled) { host_mods_ = m; host_disabled_ = disabled; host_mods_known_ = true; }
	void clear_host_mods() { host_mods_.clear(); host_disabled_.clear(); host_mods_known_ = false; }
	bool host_mods_known() const { return host_mods_known_; }
	const Manifest &host_mods() const { return host_mods_; }
	const std::set<std::string> &host_disabled() const { return host_disabled_; }

	void set_peer(const std::string &player, const PeerInfo &p) { peers_[player] = p; }
	bool remove_peer(const std::string &player) { return peers_.erase(player) > 0; }
	void clear_peers() { peers_.clear(); }
	const std::map<std::string, PeerInfo> &peers() const { return peers_; }
	bool is_late(const std::string &player) const { return match_locked_ && !match_players_.count(player); }

	std::map<std::string, std::vector<Missing>> preview_disabled() const;

	const std::map<std::string, std::vector<Missing>> &lock_match();
	void unlock_match() { match_locked_ = false; disabled_.clear(); match_players_.clear(); }
	bool match_locked() const { return match_locked_; }
	const std::map<std::string, std::vector<Missing>> &disabled() const { return disabled_; }

	bool match_disabled(const std::string &mod) const;




	bool mod_active_for(const std::string &mod, const std::string &player = "", std::string *why = nullptr) const;
	const std::map<std::string, Value> &host_values() const { return host_; }
};


bool decode(const std::string &bytes, std::map<std::string, Value> &out, std::string *why = nullptr);

}
